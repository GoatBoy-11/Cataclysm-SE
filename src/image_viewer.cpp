#include "image_viewer.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <optional>
#include <ranges>
#include <string>
#include <unordered_set>
#include <vector>

#include "assign.h"
#include "cached_options.h"
#include "catacharset.h"
#include "cursesdef.h"
#include "debug.h"
#include "filesystem.h"
#include "ime.h"
#include "input.h"
#include "item.h"
#include "json.h"
#include "messages.h"
#include "mod_manager.h"
#include "output.h"
#include "path_info.h"
#include "player.h"
#include "popup.h"
#include "string_formatter.h"
#include "translations.h"
#include "ui_manager.h"
#include "world.h"
#include "worldfactory.h"

#if defined( TILES )
#include "color.h"
#include "sdl_wrappers.h"
#include "sdltiles.h"
#endif

namespace fs = std::filesystem;

namespace
{

auto has_parent_dir( const fs::path &path ) -> bool
{
    return std::ranges::any_of( path, []( const fs::path & part ) {
        return part == "..";
    } );
}

auto path_is_inside_root( const fs::path &root_path, const fs::path &candidate_path ) -> bool
{
    const auto normalized_root = root_path.lexically_normal();
    const auto normalized_candidate = candidate_path.lexically_normal();
    const auto mismatch = std::mismatch( normalized_root.begin(), normalized_root.end(),
                                         normalized_candidate.begin(), normalized_candidate.end() );
    return mismatch.first == normalized_root.end();
}

auto to_lower_ascii( std::string value ) -> std::string
{
    std::ranges::transform( value, value.begin(), []( const unsigned char ch ) {
        return static_cast<char>( std::tolower( ch ) );
    } );
    return value;
}

auto has_image_extension( const std::string &path ) -> bool
{
    static const auto exts = std::unordered_set<std::string> {
        ".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp"
    };
    return exts.contains( to_lower_ascii( fs::path( path ).extension().generic_string() ) );
}

auto candidate_relative_paths( const std::string &image ) -> std::vector<std::string>
{
    auto relative = fs::path( image ).lexically_normal().generic_string();
    if( relative.empty() || relative == "." || has_parent_dir( relative ) ) {
        return {};
    }

    auto candidates = std::vector<std::string> { relative };
    if( fs::path( relative ).extension().empty() ) {
        candidates.emplace_back( relative + ".png" );
    }
    return candidates;
}

auto try_resolve_in_root( const std::string &relative, const std::string &root ) ->
std::optional<std::string>
{
    if( root.empty() ) {
        return std::nullopt;
    }

    const auto normalized_root = fs::path( root ).lexically_normal();
    const auto candidate = ( normalized_root / relative ).lexically_normal();
    if( !path_is_inside_root( normalized_root, candidate ) ) {
        return std::nullopt;
    }

    const auto candidate_str = candidate.generic_string();
    if( file_exist( candidate_str ) && has_image_extension( candidate_str ) ) {
        return candidate_str;
    }
    return std::nullopt;
}

#if defined( TILES )
struct sdl_render_state_guard {
    const SDL_Renderer_Ptr &renderer;
    point logical_size = point_zero;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    SDL_Rect viewport = {};
    std::optional<SDL_Rect> clip_rect;
    SDL_RendererLogicalPresentation present;

    explicit sdl_render_state_guard( const SDL_Renderer_Ptr &renderer ) : renderer( renderer ) {
        SDL_GetRenderLogicalPresentation( renderer.get(), &logical_size.x, &logical_size.y, &present );
        SDL_GetRenderScale( renderer.get(), &scale_x, &scale_y );
        SDL_GetRenderViewport( renderer.get(), &viewport );
        if( SDL_RenderClipEnabled( renderer.get() ) ) {
            clip_rect.emplace();
            SDL_GetRenderClipRect( renderer.get(), &*clip_rect );
        }
        SDL_SetRenderClipRect( renderer.get(), nullptr );
        SDL_SetRenderLogicalPresentation( renderer.get(), 0, 0, present );
        SDL_SetRenderScale( renderer.get(), 1.0f, 1.0f );
        SDL_SetRenderViewport( renderer.get(), nullptr );
    }

    ~sdl_render_state_guard() {
        if( logical_size.x > 0 && logical_size.y > 0 ) {
            SDL_SetRenderLogicalPresentation( renderer.get(), logical_size.x, logical_size.y, present );
        } else {
            SDL_SetRenderLogicalPresentation( renderer.get(), 0, 0, present );
            SDL_SetRenderScale( renderer.get(), scale_x, scale_y );
            SDL_SetRenderViewport( renderer.get(), &viewport );
        }
        SDL_SetRenderClipRect( renderer.get(), clip_rect ? &*clip_rect : nullptr );
    }
};

auto window_rect_to_buffer( const image_dest_rect &window_rect ) -> std::optional<SDL_Rect>
{
    const auto window_size = get_sdl_window_size();
    const auto buffer_size = get_sdl_display_buffer_size();
    if( window_size.x <= 0 || window_size.y <= 0 || buffer_size.x <= 0 || buffer_size.y <= 0 ) {
        return std::nullopt;
    }

    return SDL_Rect{
        static_cast<int>( std::lround( static_cast<double>( window_rect.pos.x ) * buffer_size.x /
                                       window_size.x ) ),
        static_cast<int>( std::lround( static_cast<double>( window_rect.pos.y ) * buffer_size.y /
                                       window_size.y ) ),
        static_cast<int>( std::max( 1L, std::lround( static_cast<double>( window_rect.size.x ) *
                                    buffer_size.x / window_size.x ) ) ),
        static_cast<int>( std::max( 1L, std::lround( static_cast<double>( window_rect.size.y ) *
                                    buffer_size.y / window_size.y ) ) )
    };
}

auto draw_image_caption( const std::string &caption, const SDL_Rect &image_rect ) -> void
{
    if( caption.empty() ) {
        return;
    }

    const auto font_size = get_sdl_font_size();
    const auto buffer_size = get_sdl_display_buffer_size();
    if( font_size.x <= 0 || font_size.y <= 0 || buffer_size.x <= 0 || buffer_size.y <= 0 ) {
        return;
    }

    const auto text_width = utf8_width( caption, true ) * font_size.x;
    auto caption_pos = point(
                           image_rect.x + ( image_rect.w - text_width ) / 2,
                           image_rect.y + image_rect.h + font_size.y / 4 );
    if( caption_pos.y + font_size.y > buffer_size.y ) {
        caption_pos.y = std::max( 0, image_rect.y - font_size.y );
    }
    caption_pos.x = std::clamp( caption_pos.x, 0, std::max( 0, buffer_size.x - text_width ) );

    draw_sdl_text_outlined( {
        .text = caption,
        .pos_pixel = caption_pos,
        .text_color = catacurses::white,
        .outline_color = catacurses::black,
        .outline_thickness = 2
    } );
}

auto run_image_viewer_modal( const std::string &path, const show_image_options &opts ) -> bool
{
    SDL_Texture_Ptr texture;
    point image_size;
    try {
        auto surface = load_image( path.c_str() );
        image_size = point( surface->w, surface->h );
        texture = CreateTextureFromSurface( get_sdl_renderer(), surface );
    } catch( const std::exception &err ) {
        DebugLog( DL::Error, DC::SDL ) << "show_image failed to load '" << path << "': " << err.what();
        return false;
    }
    if( !texture ) {
        DebugLog( DL::Error, DC::SDL ) << "show_image failed to create texture for '" << path << "'";
        return false;
    }

    ui_adaptor ui;
    ui.on_screen_resize( [&]( ui_adaptor & ui ) {
        ui.position_from_window( catacurses::stdscr );
    } );
    ui.mark_resize();
    ui.on_redraw( [&]( ui_adaptor & /*ui*/ ) {
        const auto dest = get_image_dest_rect( {
            .image_size = image_size,
            .screen_size = get_sdl_window_size(),
            .mode = opts.mode,
            .scale = opts.scale
        } );
        if( !dest ) {
            return;
        }
        const auto buffer_rect = window_rect_to_buffer( *dest );
        if( !buffer_rect ) {
            return;
        }
        const auto &renderer = get_sdl_renderer();
        const auto render_state_guard = sdl_render_state_guard( renderer );
        SDL_FRect f_rect{};
        SDL_RectToFRect( &*buffer_rect, &f_rect );
        RenderCopy( renderer, texture, nullptr, &f_rect );
        draw_image_caption( opts.caption.translated(), *buffer_rect );
    } );

    ime_sentry sentry( ime_sentry::disable );
    input_context ctxt( "SHOW_IMAGE" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "HELP_KEYBINDINGS" );
    ctxt.register_action( "ANY_INPUT" );
    ctxt.register_action( "COORDINATE" );
    ctxt.register_action( "SELECT" );

    while( true ) {
        ui_manager::redraw();
        refresh_display();
        const auto action = ctxt.handle_input( 5 );
        if( action == "TIMEOUT" ) {
            continue;
        }
        if( action == "HELP_KEYBINDINGS" ) {
            continue;
        }
        break;
    }
    return true;
}
#endif

#if !defined( TILES )
auto show_curses_fallback( const show_image_options &opts ) -> bool
{
    if( !opts.caption.empty() ) {
        popup( "%s", opts.caption.translated() );
        return true;
    }
    popup( _( "This picture can only be viewed in the tiles version." ) );
    return false;
}
#endif

} // namespace

auto image_display_mode_from_string( const std::string &str ) -> std::optional<image_display_mode>
{
    if( str == "native" ) {
        return image_display_mode::native;
    }
    if( str == "fullscreen" ) {
        return image_display_mode::fullscreen;
    }
    if( str == "scale" ) {
        return image_display_mode::scale;
    }
    return std::nullopt;
}

auto get_image_search_roots() -> std::vector<std::string>
{
    auto roots = std::vector<std::string> {};
    auto seen = std::unordered_set<std::string> {};

    const auto add_root = [&]( const fs::path & path ) {
        const auto normalized = path.lexically_normal().generic_string();
        if( normalized.empty() || !seen.insert( normalized ).second ) {
            return;
        }
        roots.push_back( normalized );
    };

    auto mods = std::vector<mod_id> {};
    if( world_generator && world_generator->active_world && world_generator->active_world->info ) {
        mods = world_generator->active_world->info->active_mod_order;
    } else if( world_generator ) {
        mods = world_generator->get_mod_manager().all_mods();
    }

    for( const auto &mod : mods | std::views::reverse ) {
        if( !mod.is_valid() || mod->path.empty() ) {
            continue;
        }
        const auto mod_path = fs::path( mod->path );
        add_root( mod_path / "images" );
        add_root( mod_path / "gfx" / "images" );
    }

    add_root( fs::path( PATH_INFO::gfxdir() ) / "images" );
    return roots;
}

auto resolve_image_path_in_roots( const std::string &image,
                                  const std::vector<std::string> &roots ) -> std::optional<std::string>
{
    const auto relatives = candidate_relative_paths( image );
    if( relatives.empty() ) {
        return std::nullopt;
    }

    for( const auto &root : roots ) {
        for( const auto &relative : relatives ) {
            if( const auto resolved = try_resolve_in_root( relative, root ) ) {
                return resolved;
            }
        }
    }
    return std::nullopt;
}

auto resolve_image_path( const std::string &image ) -> std::optional<std::string>
{
    return resolve_image_path_in_roots( image, get_image_search_roots() );
}

auto get_image_dest_rect( const image_dest_rect_options &opts ) -> std::optional<image_dest_rect>
{
    if( opts.image_size.x <= 0 || opts.image_size.y <= 0 || opts.screen_size.x <= 0 ||
        opts.screen_size.y <= 0 ) {
        return std::nullopt;
    }

    auto size = opts.image_size;
    if( opts.mode == image_display_mode::fullscreen ) {
        const auto width_scale = static_cast<double>( opts.screen_size.x ) /
                                 static_cast<double>( opts.image_size.x );
        const auto height_scale = static_cast<double>( opts.screen_size.y ) /
                                  static_cast<double>( opts.image_size.y );
        const auto fit = std::min( width_scale, height_scale );
        size = point(
                   std::max( 1, static_cast<int>( std::lround( opts.image_size.x * fit ) ) ),
                   std::max( 1, static_cast<int>( std::lround( opts.image_size.y * fit ) ) ) );
    } else if( opts.mode == image_display_mode::scale ) {
        if( opts.scale <= 0.0 ) {
            return std::nullopt;
        }
        size = point(
                   std::max( 1, static_cast<int>( std::lround( opts.image_size.x * opts.scale ) ) ),
                   std::max( 1, static_cast<int>( std::lround( opts.image_size.y * opts.scale ) ) ) );
    }

    return image_dest_rect{
        .pos = point( ( opts.screen_size.x - size.x ) / 2, ( opts.screen_size.y - size.y ) / 2 ),
        .size = size
    };
}

auto show_image( const show_image_options &opts ) -> bool
{
    const auto path = resolve_image_path( opts.image );
    if( !path ) {
        DebugLog( DL::Info, DC::Main ) << "show_image could not resolve '" << opts.image << "'";
        return false;
    }
    if( test_mode ) {
        return true;
    }
#if defined( TILES )
    return run_image_viewer_modal( *path, opts );
#else
    return show_curses_fallback( opts );
#endif
}

void show_image_actor::load( const JsonObject &jo )
{
    image = jo.get_string( "image" );
    jo.read( "caption", caption );
    if( jo.has_string( "mode" ) ) {
        const auto parsed = image_display_mode_from_string( jo.get_string( "mode" ) );
        if( !parsed ) {
            jo.throw_error( "mode must be \"native\", \"fullscreen\", or \"scale\"", "mode" );
        }
        mode = *parsed;
    }
    assign( jo, "scale", scale );
    if( jo.has_member( "scale" ) && !jo.has_member( "mode" ) ) {
        mode = image_display_mode::scale;
    }
    if( mode == image_display_mode::scale && scale <= 0.0 ) {
        jo.throw_error( "scale must be greater than 0", "scale" );
    }
}

int show_image_actor::use( player &p, item &, bool /*t*/, const tripoint_bub_ms & ) const
{
    if( p.is_npc() ) {
        return 0;
    }
    if( !show_image( {
    .image = image,
    .caption = caption,
    .mode = mode,
    .scale = scale
} ) ) {
        p.add_msg_if_player( m_info, _( "You can't make anything out." ) );
    }
    return 0;
}

auto show_image_actor::clone() const -> std::unique_ptr<iuse_actor>
{
    return std::make_unique<show_image_actor>( *this );
}

void show_image_actor::info( const item &, std::vector<iteminfo> &dump ) const
{
    dump.emplace_back( "DESCRIPTION", _( "Use this to look at the picture." ) );
}

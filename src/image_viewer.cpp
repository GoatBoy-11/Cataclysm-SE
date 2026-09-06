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
#include "sdl_utils.h"
#include "sdl_wrappers.h"
#include "sdltiles.h"
#endif

namespace fs = std::filesystem;

namespace
{

constexpr int default_frame_duration_ms = 100;
constexpr int max_animation_frames = 256;

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
        ".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp", ".apng"
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

auto frame_duration_for( const image_animation_spec &spec, int frame_index,
                         int default_ms ) -> int
{
    if( !spec.frame_durations_ms.empty() && frame_index >= 0 &&
        frame_index < static_cast<int>( spec.frame_durations_ms.size() ) ) {
        const int delay = spec.frame_durations_ms[frame_index];
        if( delay > 0 ) {
            return delay;
        }
    }
    return default_ms;
}

#if defined( TILES )

struct image_viewer_content {
    point frame_size = point_zero;
    image_animation_spec spec;
    SDL_Texture_Ptr atlas;
    std::vector<SDL_Texture_Ptr> frames;
    bool uses_spritesheet = false;
};

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

auto build_animation_spec( const show_image_options &opts, int frame_width, int frame_height,
                           int frame_count, std::vector<int> frame_durations_ms ) -> image_animation_spec
{
    auto spec = image_animation_spec {
        .frame_width = frame_width,
        .frame_height = frame_height,
        .frame_count = frame_count,
        .columns = opts.animation.columns,
        .frame_durations_ms = std::move( frame_durations_ms ),
        .loop = opts.animation.loop,
    };
    if( opts.animation.frame_duration > 0 ) {
        spec.frame_durations_ms.clear();
    }
    return spec;
}

auto load_spritesheet_content( const std::string &path, const show_image_options &opts ) ->
std::optional<image_viewer_content>
{
    const auto &anim = opts.animation;
    if( anim.frame_width <= 0 || anim.frame_height <= 0 || anim.frame_count <= 1 ) {
        return std::nullopt;
    }
    if( anim.frame_count > max_animation_frames ) {
        DebugLog( DL::Error, DC::Main ) << "show_image spritesheet exceeds frame limit for '" << path << "'";
        return std::nullopt;
    }

    SDL_Surface_Ptr surface;
    try {
        surface = load_image( path.c_str() );
    } catch( const std::exception &err ) {
        DebugLog( DL::Error, DC::SDL ) << "show_image failed to load '" << path << "': " << err.what();
        return std::nullopt;
    }

    const auto columns = compute_spritesheet_columns( surface->w, {
        .frame_width = anim.frame_width,
        .frame_height = anim.frame_height,
        .frame_count = anim.frame_count,
        .columns = anim.columns,
    } );
    const auto rows = ( anim.frame_count + columns - 1 ) / columns;
    if( surface->w < columns * anim.frame_width || surface->h < rows * anim.frame_height ) {
        DebugLog( DL::Error, DC::Main ) << "show_image spritesheet '" << path << "' is too small for layout";
        return std::nullopt;
    }

    auto atlas = CreateTextureFromSurface( get_sdl_renderer(), surface );
    if( !atlas ) {
        DebugLog( DL::Error, DC::SDL ) << "show_image failed to create texture for '" << path << "'";
        return std::nullopt;
    }

    return image_viewer_content {
        .frame_size = point( anim.frame_width, anim.frame_height ),
        .spec = build_animation_spec( opts, anim.frame_width, anim.frame_height, anim.frame_count, {} ),
        .atlas = std::move( atlas ),
        .uses_spritesheet = true,
    };
}

auto load_file_animation_content( const std::string &path, const show_image_options &opts ) ->
std::optional<image_viewer_content>
{
    IMG_Animation *const raw_anim = IMG_LoadAnimation( path.c_str() );
    if( raw_anim == nullptr ) {
        return std::nullopt;
    }

    const auto anim = std::unique_ptr<IMG_Animation, decltype( &IMG_FreeAnimation )>( raw_anim,
    IMG_FreeAnimation );
    if( anim->count <= 1 || anim->w <= 0 || anim->h <= 0 ) {
        return std::nullopt;
    }
    if( anim->count > max_animation_frames ) {
        DebugLog( DL::Error, DC::Main ) << "show_image animation exceeds frame limit for '" << path << "'";
        return std::nullopt;
    }

    auto durations = std::vector<int> {};
    if( opts.animation.frame_duration <= 0 && anim->delays != nullptr ) {
        durations.reserve( anim->count );
        for( int i = 0; i < anim->count; ++i ) {
            durations.push_back( anim->delays[i] > 0 ? anim->delays[i] : default_frame_duration_ms );
        }
    }

    auto frames = std::vector<SDL_Texture_Ptr> {};
    frames.reserve( anim->count );
    const auto &renderer = get_sdl_renderer();
    for( int i = 0; i < anim->count; ++i ) {
        if( anim->frames == nullptr || anim->frames[i] == nullptr ) {
            DebugLog( DL::Error, DC::SDL ) << "show_image missing animation frame in '" << path << "'";
            return std::nullopt;
        }
        SDL_Surface_Ptr frame_surface( SDL_ConvertSurface( anim->frames[i], sdl_color_pixel_format ) );
        if( !frame_surface ) {
            DebugLog( DL::Error, DC::SDL ) << "show_image failed to convert animation frame in '" << path << "'";
            return std::nullopt;
        }
        auto texture = CreateTextureFromSurface( renderer, frame_surface );
        if( !texture ) {
            DebugLog( DL::Error, DC::SDL ) << "show_image failed to create animation texture for '" << path << "'";
            return std::nullopt;
        }
        frames.emplace_back( std::move( texture ) );
    }

    return image_viewer_content {
        .frame_size = point( anim->w, anim->h ),
        .spec = build_animation_spec( opts, anim->w, anim->h, anim->count, std::move( durations ) ),
        .frames = std::move( frames ),
    };
}

auto load_static_content( const std::string &path, const show_image_options &opts ) ->
std::optional<image_viewer_content>
{
    SDL_Surface_Ptr surface;
    try {
        surface = load_image( path.c_str() );
    } catch( const std::exception &err ) {
        DebugLog( DL::Error, DC::SDL ) << "show_image failed to load '" << path << "': " << err.what();
        return std::nullopt;
    }

    auto texture = CreateTextureFromSurface( get_sdl_renderer(), surface );
    if( !texture ) {
        DebugLog( DL::Error, DC::SDL ) << "show_image failed to create texture for '" << path << "'";
        return std::nullopt;
    }

    auto frames = std::vector<SDL_Texture_Ptr> {};
    frames.emplace_back( std::move( texture ) );
    return image_viewer_content {
        .frame_size = point( surface->w, surface->h ),
        .spec = build_animation_spec( opts, surface->w, surface->h, 1, {} ),
        .frames = std::move( frames ),
    };
}

auto load_image_viewer_content( const std::string &path, const show_image_options &opts ) ->
std::optional<image_viewer_content>
{
    if( uses_spritesheet_animation( opts.animation ) ) {
        if( auto content = load_spritesheet_content( path, opts ) ) {
            return content;
        }
        return std::nullopt;
    }
    if( auto content = load_file_animation_content( path, opts ) ) {
        return content;
    }
    return load_static_content( path, opts );
}

auto draw_image_frame( const image_viewer_content &content, int frame_index,
                       const SDL_Rect &buffer_rect ) -> void
{
    const auto &renderer = get_sdl_renderer();
    const auto render_state_guard = sdl_render_state_guard( renderer );
    SDL_FRect dst_rect{};
    SDL_RectToFRect( &buffer_rect, &dst_rect );

    if( content.uses_spritesheet && content.atlas ) {
        float atlas_w = 0.0f;
        float atlas_h = 0.0f;
        SDL_GetTextureSize( content.atlas.get(), &atlas_w, &atlas_h );
        const auto frame_rect = get_spritesheet_frame_rect( static_cast<int>( atlas_w ),
                                static_cast<int>( atlas_h ), content.spec, frame_index );
        if( !frame_rect ) {
            return;
        }
        const SDL_FRect src_rect {
            static_cast<float>( frame_rect->pos.x ),
            static_cast<float>( frame_rect->pos.y ),
            static_cast<float>( frame_rect->size.x ),
            static_cast<float>( frame_rect->size.y ),
        };
        RenderCopy( renderer, content.atlas, &src_rect, &dst_rect );
        return;
    }

    if( frame_index < 0 || frame_index >= static_cast<int>( content.frames.size() ) ||
        !content.frames[frame_index] ) {
        return;
    }
    RenderCopy( renderer, content.frames[frame_index], nullptr, &dst_rect );
}

auto run_image_viewer_modal( const std::string &path, const show_image_options &opts ) -> bool
{
    auto loaded = load_image_viewer_content( path, opts );
    if( !loaded ) {
        return false;
    }
    auto content = std::move( *loaded );

    const int playback_frame_duration = opts.animation.frame_duration > 0 ? opts.animation.frame_duration :
                                        default_frame_duration_ms;
    auto anim_state = image_animation_state {};
    auto last_tick = SDL_GetTicks();

    ui_adaptor ui;
    ui.on_screen_resize( [&]( ui_adaptor & ui ) {
        ui.position_from_window( catacurses::stdscr );
    } );
    ui.mark_resize();
    ui.on_redraw( [&]( ui_adaptor & /*ui*/ ) {
        const auto dest = get_image_dest_rect( {
            .image_size = content.frame_size,
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
        draw_image_frame( content, anim_state.frame_index, *buffer_rect );
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
        const auto now = SDL_GetTicks();
        const auto elapsed = now - last_tick;
        last_tick = now;
        if( elapsed > 0 ) {
            advance_image_animation( anim_state, content.spec, static_cast<int>( elapsed ),
                                     playback_frame_duration );
        }

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

auto uses_spritesheet_animation( const image_animation_options &opts ) -> bool
{
    return opts.frame_width > 0 && opts.frame_height > 0 && opts.frame_count > 1;
}

auto load_image_animation_options( const JsonObject &jo, image_animation_options &opts ) -> void
{
    if( jo.has_object( "animation" ) ) {
        JsonObject source = jo.get_object( "animation" );
        assign( source, "frame_width", opts.frame_width );
        assign( source, "frame_height", opts.frame_height );
        assign( source, "frame_count", opts.frame_count );
        assign( source, "frame_duration", opts.frame_duration );
        assign( source, "columns", opts.columns );
        if( source.has_bool( "loop" ) ) {
            opts.loop = source.get_bool( "loop" );
        }
        return;
    }

    assign( jo, "frame_width", opts.frame_width );
    assign( jo, "frame_height", opts.frame_height );
    assign( jo, "frame_count", opts.frame_count );
    assign( jo, "frame_duration", opts.frame_duration );
    assign( jo, "columns", opts.columns );
    if( jo.has_bool( "loop" ) ) {
        opts.loop = jo.get_bool( "loop" );
    }
}

auto compute_spritesheet_columns( int sheet_width, const image_animation_spec &spec ) -> int
{
    if( spec.frame_width <= 0 || sheet_width <= 0 ) {
        return 0;
    }
    if( spec.columns > 0 ) {
        return spec.columns;
    }
    return std::max( 1, sheet_width / spec.frame_width );
}

auto get_spritesheet_frame_rect( int sheet_width, int sheet_height, const image_animation_spec &spec,
                                 int frame_index ) -> std::optional<spritesheet_frame_rect>
{
    if( frame_index < 0 || frame_index >= spec.frame_count || spec.frame_width <= 0 ||
        spec.frame_height <= 0 ) {
        return std::nullopt;
    }

    const auto columns = compute_spritesheet_columns( sheet_width, spec );
    if( columns <= 0 ) {
        return std::nullopt;
    }

    const auto col = frame_index % columns;
    const auto row = frame_index / columns;
    const auto origin = point( col * spec.frame_width, row * spec.frame_height );
    if( origin.x + spec.frame_width > sheet_width || origin.y + spec.frame_height > sheet_height ) {
        return std::nullopt;
    }

    return spritesheet_frame_rect {
        .pos = origin,
        .size = point( spec.frame_width, spec.frame_height )
    };
}

auto advance_image_animation( image_animation_state &state, const image_animation_spec &spec,
                              int elapsed_ms, int default_frame_duration_ms ) -> void
{
    if( spec.frame_count <= 1 || elapsed_ms <= 0 ) {
        return;
    }

    auto remaining = elapsed_ms;
    while( remaining > 0 ) {
        const int duration = frame_duration_for( spec, state.frame_index, default_frame_duration_ms );
        if( duration <= 0 ) {
            return;
        }

        const int until_next = duration - state.ms_into_frame;
        if( remaining < until_next ) {
            state.ms_into_frame += remaining;
            return;
        }

        remaining -= until_next;
        state.ms_into_frame = 0;

        if( state.frame_index + 1 < spec.frame_count ) {
            state.frame_index++;
        } else if( spec.loop ) {
            state.frame_index = 0;
        } else {
            state.frame_index = spec.frame_count - 1;
            state.ms_into_frame = duration - 1;
            return;
        }
    }
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

    load_image_animation_options( jo, animation );
    if( uses_spritesheet_animation( animation ) ) {
        if( animation.frame_width <= 0 ) {
            jo.throw_error( "frame_width must be greater than 0 for spritesheet animation", "frame_width" );
        }
        if( animation.frame_height <= 0 ) {
            jo.throw_error( "frame_height must be greater than 0 for spritesheet animation", "frame_height" );
        }
        if( animation.frame_count <= 1 ) {
            jo.throw_error( "frame_count must be greater than 1 for spritesheet animation", "frame_count" );
        }
        if( animation.frame_count > max_animation_frames ) {
            jo.throw_error( string_format( "frame_count must be %d or less", max_animation_frames ),
                            "frame_count" );
        }
        if( animation.frame_duration < 0 ) {
            jo.throw_error( "frame_duration must be 0 or greater", "frame_duration" );
        }
        if( animation.columns < 0 ) {
            jo.throw_error( "columns must be 0 or greater", "columns" );
        }
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
    .scale = scale,
    .animation = animation
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

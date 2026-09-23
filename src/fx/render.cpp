#if defined(TILES)

#include "fx/render.h"

#include "cata_tiles.h"
#include "compute/gpu_platform.h"
#include "coordinates.h"
#include "debug.h"
#include "fx/present_grade.h"
#include "fx/system.h"
#include "options.h"
#include "path_info.h"
#include "weather/weather.h"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_render.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

struct present_uniforms {
    float grade0[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
    float tint_rgb[4] = { 1.0f, 1.0f, 1.0f, 0.0f };
    float flash_rgb[4] = { 1.0f, 1.0f, 1.0f, 0.0f };
};

auto uniforms_from_grade( const present_grade &grade ) -> present_uniforms
{
    return present_uniforms{
        .grade0 = { grade.color_scale, grade.tint_strength, grade.flash_strength, 0.0f },
        .tint_rgb = { grade.tint_r, grade.tint_g, grade.tint_b, 0.0f },
        .flash_rgb = { grade.flash_r, grade.flash_g, grade.flash_b, 0.0f },
    };
}

auto grade_needs_shader( const present_grade &grade ) -> bool
{
    return grade.tint_strength > 0.0f || grade.flash_strength > 0.0f ||
           std::abs( grade.color_scale - 1.0f ) > 0.001f;
}

SDL_GPUShader *s_fragment = nullptr;
SDL_GPURenderState *s_state = nullptr;
SDL_Renderer *s_state_renderer = nullptr;
bool s_init_attempted = false;
bool s_logged_fallback = false;

auto read_blob( const std::string &path ) -> std::vector<std::byte>
{
    auto ifs = std::ifstream( path, std::ios::binary | std::ios::ate );
    if( !ifs ) {
        return {};
    }
    const auto size = static_cast<std::size_t>( ifs.tellg() );
    ifs.seekg( 0 );
    auto buf = std::vector<std::byte>( size );
    ifs.read( reinterpret_cast<char *>( buf.data() ), static_cast<std::streamsize>( size ) );
    return buf;
}

auto preferred_fmt( const SDL_GPUShaderFormat fmts ) -> SDL_GPUShaderFormat
{
    if( fmts & SDL_GPU_SHADERFORMAT_DXIL ) {
        return SDL_GPU_SHADERFORMAT_DXIL;
    }
    if( fmts & SDL_GPU_SHADERFORMAT_SPIRV ) {
        return SDL_GPU_SHADERFORMAT_SPIRV;
    }
    if( fmts & SDL_GPU_SHADERFORMAT_MSL ) {
        return SDL_GPU_SHADERFORMAT_MSL;
    }
    return SDL_GPU_SHADERFORMAT_INVALID;
}

auto preferred_ext( const SDL_GPUShaderFormat fmts ) -> std::string_view
{
    if( fmts & SDL_GPU_SHADERFORMAT_DXIL ) {
        return ".dxil";
    }
    if( fmts & SDL_GPU_SHADERFORMAT_SPIRV ) {
        return ".spv";
    }
    if( fmts & SDL_GPU_SHADERFORMAT_MSL ) {
        return ".msl";
    }
    return {};
}

auto release_present( SDL_Renderer *const renderer ) -> void
{
    if( s_state != nullptr ) {
        if( renderer != nullptr ) {
            SDL_SetGPURenderState( renderer, nullptr );
        }
        SDL_DestroyGPURenderState( s_state );
        s_state = nullptr;
    }
    if( s_fragment != nullptr ) {
        auto *device = renderer != nullptr ? SDL_GetGPURendererDevice( renderer ) : nullptr;
        if( device != nullptr ) {
            SDL_ReleaseGPUShader( device, s_fragment );
        }
        s_fragment = nullptr;
    }
    s_state_renderer = nullptr;
}

auto try_init_present( SDL_Renderer *const renderer ) -> bool
{
    if( s_init_attempted && s_state_renderer == renderer ) {
        return s_state != nullptr;
    }
    s_init_attempted = true;
    release_present( s_state_renderer );
    s_state_renderer = renderer;

    // SDL_GPURenderState only works on the GPU renderer. Lighting compute uses a
    // separate SDL_GPUDevice created after the window renderer, so they do not share.
    auto *const device = SDL_GetGPURendererDevice( renderer );
    if( device == nullptr ) {
        if( !s_logged_fallback ) {
            s_logged_fallback = true;
            DebugLog( DL::Info, DC::Main )
                    << "GRAPHIC_FX: present shaders fall back to plain blit "
                    "(SDL_Renderer is not GPU-backed; compute device stays separate)";
        }
        return false;
    }

    const auto fmts = SDL_GetGPUShaderFormats( device );
    const auto fmt = preferred_fmt( fmts );
    const auto ext = preferred_ext( fmts );
    if( fmt == SDL_GPU_SHADERFORMAT_INVALID || ext.empty() ) {
        DebugLog( DL::Warn, DC::Main ) << "GRAPHIC_FX: no supported present shader format";
        return false;
    }

    const auto path = PATH_INFO::shaders() + "present_passthrough_fragment" + std::string{ ext };
    const auto blob = read_blob( path );
    if( blob.empty() ) {
        DebugLog( DL::Warn, DC::Main ) << "GRAPHIC_FX: present shader blob missing: " << path;
        return false;
    }

    const auto shader_info = SDL_GPUShaderCreateInfo{
        .code_size = blob.size(),
        .code = reinterpret_cast<const Uint8 *>( blob.data() ),
        .entrypoint = cata_gpu::compute_shader_entrypoint( fmt ),
        .format = fmt,
        .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
        .num_samplers = 1,
        .num_storage_textures = 0,
        .num_storage_buffers = 0,
        .num_uniform_buffers = 1,
        .props = 0
    };
    s_fragment = SDL_CreateGPUShader( device, &shader_info );
    if( s_fragment == nullptr ) {
        DebugLog( DL::Warn, DC::Main ) << "GRAPHIC_FX: CreateGPUShader failed: " << SDL_GetError();
        return false;
    }

    const auto state_info = SDL_GPURenderStateCreateInfo{
        .fragment_shader = s_fragment,
        .num_sampler_bindings = 0,
        .sampler_bindings = nullptr,
        .num_storage_textures = 0,
        .storage_textures = nullptr,
        .num_storage_buffers = 0,
        .storage_buffers = nullptr,
        .props = 0
    };
    s_state = SDL_CreateGPURenderState( renderer, &state_info );
    if( s_state == nullptr ) {
        DebugLog( DL::Warn, DC::Main ) << "GRAPHIC_FX: CreateGPURenderState failed: " << SDL_GetError();
        SDL_ReleaseGPUShader( device, s_fragment );
        s_fragment = nullptr;
        return false;
    }

    auto uniforms = uniforms_from_grade( present_grade{} );
    SDL_SetGPURenderStateFragmentUniforms( s_state, 0, &uniforms, sizeof( uniforms ) );
    s_state_renderer = renderer;
    DebugLog( DL::Info, DC::Main ) << "GRAPHIC_FX: present grade shader ready";
    return true;
}

auto fade_alpha( const fx_particle &p ) -> std::uint8_t
{
    const auto t = std::clamp( 1.0f - ( p.age_s / std::max( p.life_s, 0.001f ) ), 0.0f, 1.0f );
    return static_cast<std::uint8_t>( std::clamp( static_cast<int>( p.a * t ), 0, 255 ) );
}

auto append_quad( std::vector<SDL_Vertex> &verts, std::vector<int> &indices,
                  const SDL_FPoint &center, const float size, const SDL_FColor &color ) -> void
{
    const auto half = size * 0.5f;
    const auto base = static_cast<int>( verts.size() );
    verts.push_back( SDL_Vertex{ .position = { center.x - half, center.y - half }, .color = color, .tex_coord = { 0.0f, 0.0f } } );
    verts.push_back( SDL_Vertex{ .position = { center.x + half, center.y - half }, .color = color, .tex_coord = { 1.0f, 0.0f } } );
    verts.push_back( SDL_Vertex{ .position = { center.x + half, center.y + half }, .color = color, .tex_coord = { 1.0f, 1.0f } } );
    verts.push_back( SDL_Vertex{ .position = { center.x - half, center.y + half }, .color = color, .tex_coord = { 0.0f, 1.0f } } );
    indices.push_back( base );
    indices.push_back( base + 1 );
    indices.push_back( base + 2 );
    indices.push_back( base );
    indices.push_back( base + 2 );
    indices.push_back( base + 3 );
}

} // namespace

namespace cata_fx
{

auto draw_world( const SDL_Renderer_Ptr &renderer, const cata_tiles &tiles ) -> void
{
    if( !enabled() || !renderer || cata_fx::particles().empty() ) {
        return;
    }

    const auto tile_w = static_cast<float>( tiles.get_tile_width() );
    const auto tile_h = static_cast<float>( tiles.get_tile_height() );

    auto to_screen = [&]( const fx_particle & p ) -> SDL_FPoint {
        const auto ix = static_cast<int>( std::floor( p.x ) );
        const auto iy = static_cast<int>( std::floor( p.y ) );
        const auto base = tiles.player_to_screen( point_bub_ms( ix, iy ) );
        const auto fx = p.x - static_cast<float>( ix );
        const auto fy = p.y - static_cast<float>( iy );
        return SDL_FPoint{
            static_cast<float>( base.x ) + fx * tile_w + tile_w * 0.5f,
            static_cast<float>( base.y ) + fy * tile_h + tile_h * 0.5f
        };
    };

    static auto verts_blend = std::vector<SDL_Vertex> {};
    static auto idx_blend = std::vector<int> {};
    static auto verts_add = std::vector<SDL_Vertex> {};
    static auto idx_add = std::vector<int> {};
    verts_blend.clear();
    idx_blend.clear();
    verts_add.clear();
    idx_add.clear();
    verts_blend.reserve( cata_fx::particles().size() * 4 );
    idx_blend.reserve( cata_fx::particles().size() * 6 );

    for( const auto &p : cata_fx::particles() ) {
        const auto color = SDL_FColor{
            static_cast<float>( p.r ) / 255.0f,
            static_cast<float>( p.g ) / 255.0f,
            static_cast<float>( p.b ) / 255.0f,
            static_cast<float>( fade_alpha( p ) ) / 255.0f
        };
        const auto size = p.size_px * tiles.screen_tile_zoom();
        if( p.blend == fx_blend::additive ) {
            append_quad( verts_add, idx_add, to_screen( p ), size, color );
        } else {
            append_quad( verts_blend, idx_blend, to_screen( p ), size, color );
        }
    }

    auto draw_batch = [&]( std::vector<SDL_Vertex> &verts, std::vector<int> &indices,
    const SDL_BlendMode mode ) {
        if( verts.empty() ) {
            return;
        }
        SDL_SetRenderDrawBlendMode( renderer.get(), mode );
        SDL_RenderGeometry( renderer.get(), nullptr, verts.data(), static_cast<int>( verts.size() ),
                            indices.data(), static_cast<int>( indices.size() ) );
    };

    draw_batch( verts_blend, idx_blend, SDL_BLENDMODE_BLEND );
    draw_batch( verts_add, idx_add, SDL_BLENDMODE_ADD );
    SDL_SetRenderDrawBlendMode( renderer.get(), SDL_BLENDMODE_BLEND );
}

auto bind_present_shader( SDL_Renderer *const renderer ) -> bool
{
    if( renderer == nullptr || !enabled() ) {
        return false;
    }
    if( get_options().has_option( "GRAPHIC_FX" ) && !get_option<bool>( "GRAPHIC_FX" ) ) {
        return false;
    }

    static auto flash_last = std::chrono::steady_clock::now();
    const auto flash_now = std::chrono::steady_clock::now();
    const auto flash_dt = std::chrono::duration<float>( flash_now - flash_last ).count();
    flash_last = flash_now;
    advance_lightning_screen_flash( std::clamp( flash_dt, 0.0f, 0.1f ) );

    const auto grade = compute_present_grade( present_grade_input{
        .weather = category_enabled( "FX_SCREEN_TINT" ) ? get_weather().weather_id : weather_type_id{},
        .lightning_flash_frac = category_enabled( "FX_LIGHTNING" ) ? lightning_screen_flash_frac() : 0.0f,
    } );
    if( !grade_needs_shader( grade ) ) {
        return false;
    }
    if( !try_init_present( renderer ) || s_state == nullptr ) {
        return false;
    }
    const auto uniforms = uniforms_from_grade( grade );
    SDL_SetGPURenderStateFragmentUniforms( s_state, 0, &uniforms, sizeof( uniforms ) );
    return SDL_SetGPURenderState( renderer, s_state );
}

auto unbind_present_shader( SDL_Renderer *const renderer ) -> void
{
    if( renderer == nullptr ) {
        return;
    }
    SDL_SetGPURenderState( renderer, nullptr );
}

auto shutdown_present() -> void
{
    release_present( s_state_renderer );
    s_init_attempted = false;
}

} // namespace cata_fx

#endif // TILES

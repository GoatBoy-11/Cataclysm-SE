#pragma once

#if defined(TILES)

class cata_tiles;
struct SDL_Renderer;

#include "sdl_wrappers.h"

namespace cata_fx
{

/// Batched SDL_RenderGeometry of the live particle pool, in screen space.
auto draw_world( const SDL_Renderer_Ptr &renderer, const cata_tiles &tiles ) -> void;

/// Bind the present-time fragment shader for the next copy. False means plain blit.
auto bind_present_shader( SDL_Renderer *renderer ) -> bool;
auto unbind_present_shader( SDL_Renderer *renderer ) -> void;
auto shutdown_present() -> void;

} // namespace cata_fx

#endif // TILES

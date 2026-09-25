// Copyright © 2026 CCP ehf.

#pragma once

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include <cstdint>

namespace TrinityALImpl
{

// Starts SDL3's video subsystem once for the process: the desktop's X11 or Wayland session, or SDL's offscreen driver
// when there is none (containers, CI), whose Vulkan surfaces are headless. SDL_VIDEO_DRIVER overrides the choice. SDL
// is told to leave signals and quitting to the engine. False if no video driver could start.
bool InitializeWindowSystem();

// The SDL display (SDL_DisplayID) showing an adapter's output. Vulkan adapters are GPUs rather than outputs, so every
// adapter maps to the primary display. 0 without a window system.
uint32_t GetAdapterDisplay( uint32_t adapter );

}

#endif

// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

// SDL3 windows for the Vulkan backend on Linux. SDL uses the desktop (X11 or Wayland) when there is one and otherwise
// its offscreen driver, whose Vulkan surfaces are VK_EXT_headless_surface, so the swapchain tests also run in containers
// and CI. SDL_VIDEO_DRIVER overrides the choice as usual.
#if TRINITY_PLATFORM == TRINITY_VULKAN && defined( __linux__ )

#include "RenderWindow.h"
#include "WithWindowFixture.h"

#include <SDL3/SDL.h>

bool InitTestVideo()
{
	static bool initialized = false;
	static bool attempted = false;
	if( attempted )
	{
		return initialized;
	}
	attempted = true;
	if( !getenv( "SDL_VIDEO_DRIVER" ) && !getenv( "DISPLAY" ) && !getenv( "WAYLAND_DISPLAY" ) )
	{
		SDL_SetHint( SDL_HINT_VIDEO_DRIVER, "offscreen" ); // no desktop: skip probing X11/Wayland
	}
	initialized = SDL_InitSubSystem( SDL_INIT_VIDEO );
	if( !initialized && !getenv( "SDL_VIDEO_DRIVER" ) )
	{
		SDL_SetHint( SDL_HINT_VIDEO_DRIVER, "offscreen" );
		initialized = SDL_InitSubSystem( SDL_INIT_VIDEO );
	}
	if( !initialized )
	{
		fprintf( stderr, "TrinityALTest: no SDL video (%s); swapchain tests run without windows\n", SDL_GetError() );
	}
	return initialized;
}

RenderWindow::RenderWindow( uint32_t width, uint32_t height ) :
	m_handle( 0 )
{
	if( InitTestVideo() )
	{
		SDL_Window* window = SDL_CreateWindow( "TrinityALTest", int( width ), int( height ), SDL_WINDOW_VULKAN );
		if( window )
		{
			SDL_SyncWindow( window );
		}
		m_handle = Tr2WindowHandle( reinterpret_cast<uintptr_t>( window ) );
	}
}

RenderWindow::~RenderWindow()
{
	if( m_handle )
	{
		SDL_DestroyWindow( reinterpret_cast<SDL_Window*>( m_handle ) );
	}
}

uint32_t RenderWindow::GetClientWidth() const
{
	int width = 0, height = 0;
	if( m_handle )
	{
		SDL_GetWindowSizeInPixels( reinterpret_cast<SDL_Window*>( m_handle ), &width, &height );
	}
	return uint32_t( width );
}

uint32_t RenderWindow::GetClientHeight() const
{
	int width = 0, height = 0;
	if( m_handle )
	{
		SDL_GetWindowSizeInPixels( reinterpret_cast<SDL_Window*>( m_handle ), &width, &height );
	}
	return uint32_t( height );
}

bool RenderWindow::Resize( uint32_t width, uint32_t height )
{
	if( !m_handle )
	{
		return false;
	}
	SDL_Window* window = reinterpret_cast<SDL_Window*>( m_handle );
	if( !SDL_SetWindowSize( window, int( width ), int( height ) ) )
	{
		return false;
	}
	SDL_SyncWindow( window );
	return true;
}

#endif

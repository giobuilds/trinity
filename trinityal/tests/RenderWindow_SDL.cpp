// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

// SDL3 windows for the Vulkan backend on Linux, on the window system the engine uses (TrinityALImpl::
// InitializeWindowSystem): the desktop when there is one, otherwise SDL's offscreen driver with headless Vulkan
// surfaces, so the swapchain tests also run in containers and CI.
#if TRINITY_PLATFORM == TRINITY_VULKAN && defined( __linux__ )

#include "RenderWindow.h"
#include "WithWindowFixture.h"

#include "VulkanWindowSystem.h"

#include <SDL3/SDL.h>

bool InitTestVideo()
{
	static const bool initialized = TrinityALImpl::InitializeWindowSystem();
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

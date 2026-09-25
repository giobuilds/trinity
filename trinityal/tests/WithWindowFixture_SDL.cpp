// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

// The shared test window for the Vulkan backend on Linux (see RenderWindow_SDL.cpp).
#if TRINITY_PLATFORM == TRINITY_VULKAN && defined( __linux__ )

#include "WithWindowFixture.h"
#include "RenderWindow.h"

#include <SDL3/SDL.h>

namespace
{
RenderWindow* s_wnd = nullptr;
}

void WithWindow::SetUpTestCase()
{
	CCP_DELETE s_wnd;
	s_wnd = CCP_NEW( "WithWindowFixture/s_wnd" ) RenderWindow( 640, 480 );
}

void WithWindow::TearDownTestCase()
{
	CCP_DELETE s_wnd;
	s_wnd = nullptr;
}

void WithWindow::BeginLoopProcessing()
{
	if( s_wnd && s_wnd->GetHandle() )
	{
		SDL_ShowWindow( reinterpret_cast<SDL_Window*>( s_wnd->GetHandle() ) );
	}
}

// Interactive mode (--interactive): runs until the window is closed.
bool WithWindow::DoLoopProcessing()
{
	SDL_Event event;
	while( SDL_PollEvent( &event ) )
	{
		if( event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED )
		{
			return false;
		}
	}
	return true;
}

Tr2WindowHandle WithWindow::GetWindowHandle()
{
	return s_wnd ? s_wnd->GetHandle() : Tr2WindowHandle( 0 );
}

RenderWindow* WithWindow::GetWindow()
{
	return s_wnd;
}

#endif

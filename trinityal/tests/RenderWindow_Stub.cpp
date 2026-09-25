// Copyright © 2023 CCP ehf.

#include "StdAfx.h"
// Headless window for the stub backend (the Vulkan backend uses SDL3 windows: RenderWindow_SDL.cpp).
#if !defined( _WIN32 ) && !defined( TRINITY_AL_MOBILE ) && TRINITY_PLATFORM == TRINITY_STUB
#include "RenderWindow.h"
#include "WithWindowFixture.h"

RenderWindow::RenderWindow( uint32_t width, uint32_t height )
{
	// A fake handle encoding the client size; C-style cast because Tr2WindowHandle is a pointer on Windows and an integer on Linux.
	m_handle = (Tr2WindowHandle)( uintptr_t( ( width & 0xffff ) | ( ( height & 0xffff ) << 16 ) ) );
}

RenderWindow::~RenderWindow()
{
}

uint32_t RenderWindow::GetClientWidth() const
{
	return uint32_t( (uintptr_t)m_handle & 0xffff );
}

uint32_t RenderWindow::GetClientHeight() const
{
	return uint32_t( ( (uintptr_t)m_handle >> 16 ) & 0xffff );
}

bool RenderWindow::Resize( uint32_t width, uint32_t height )
{
	return false;
}

#endif

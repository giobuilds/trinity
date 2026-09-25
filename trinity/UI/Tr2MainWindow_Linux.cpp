// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#if defined( __linux__ ) && TRINITY_PLATFORM != TRINITY_VULKAN

#include "Tr2MainWindow.h"
#include "Tr2MouseCursor.h"
#include "TriDevice.h"
#include "Scancodes.h"

// Headless main window for the stub renderer (the Vulkan build uses SDL3 windows: Tr2MainWindow_SDL.cpp). The window is virtual:
// it exists once created, its client area is exactly the requested size (there is no frame), it never has focus and
// it receives no input events. The output handle stays null, which the stub renderer ignores. Display sizes come from
// the render backend's adapter information.


void Tr2MainWindow::SetWindowTitle( const wchar_t* title )
{
	m_title = title;
}

bool Tr2MainWindow::HasFocus() const
{
	return false;
}

bool Tr2MainWindow::HasWindow() const
{
	return m_hasWindow_Linux;
}

void Tr2MainWindow::CreateOSWindow( Tr2MainWindowState::State& )
{
	m_hasWindow_Linux = true;
}

void Tr2MainWindow::DestroyOSWindow()
{
	m_hasWindow_Linux = false;
}

void Tr2MainWindow::AdjustWindow( Tr2MainWindowState::State& )
{
}

Tr2MainWindow::Rect Tr2MainWindow::GetDesktopRect() const
{
	// A single display: the primary adapter's current mode.
	return GetMonitorRect( 0 );
}

Tr2MainWindow::Rect Tr2MainWindow::GetMonitorRect( uint32_t adapter ) const
{
	Tr2DisplayModeInfo mi;
	if( FAILED( Tr2VideoAdapterInfo::GetAdapterDisplayMode( adapter, mi ) ) )
	{
		return {};
	}
	return { 0, 0, int32_t( mi.width ), int32_t( mi.height ) };
}

Tr2MainWindow::Size Tr2MainWindow::GetWindowSize( const Size& clientSize, Tr2WindowMode::Type ) const
{
	return clientSize;
}

Tr2MainWindow::Size Tr2MainWindow::GetClientSize( const Size& windowSize, Tr2WindowMode::Type ) const
{
	return windowSize;
}

bool Tr2MainWindow::SupportsFullscreen() const
{
	return true;
}

void Tr2MainWindow::ClipCursor( int32_t, int32_t, int32_t, int32_t )
{
}

void Tr2MainWindow::UnclipCursor()
{
}

void Tr2MainWindow::SetCursorPos( int32_t, int32_t )
{
}

std::pair<int32_t, int32_t> Tr2MainWindow::GetCursorPos() const
{
	return { 0, 0 };
}

bool Tr2MainWindow::IsKeyToggled( uint32_t ) const
{
	return false;
}

bool Tr2MainWindow::IsKeyPressed( uint32_t keyCode ) const
{
	return KeyboardHelpers::IsPlatformKeyPressed( KeyboardHelpers::PlatformKey( keyCode ) );
}

std::string Tr2MainWindow::GetKeyName( uint32_t keyCode ) const
{
	return KeyboardHelpers::GetAppKeyName( KeyboardHelpers::AppKey( keyCode ) );
}

bool Tr2MainWindow::ProcessMessages()
{
	// No event queue; the window is never closed by the user.
	return true;
}

uintptr_t Tr2MainWindow::GetHwnd() const
{
	return uintptr_t( m_hwnd );
}

void Tr2MainWindow::OnTick( Be::Time, Be::Time, void* )
{
}

void Tr2MainWindow::SanitizeWindowedResolution( Tr2MainWindowState::State& state ) const
{
	if( state.width == 0 && state.height == 0 )
	{
		// use window size that spans default display
		Tr2DisplayModeInfo mi;
		Tr2VideoAdapterInfo::GetAdapterDisplayMode( state.adapter, mi );

		Size size = { mi.width, mi.height };
		size = GetClientSize( size, state.windowMode );

		state.width = size.width;
		state.height = size.height;
	}
	else
	{
		// clamp size between window min size and desktop size
		state.width = std::max( m_minimumSize.width, state.width );
		state.height = std::max( m_minimumSize.height, state.height );

		auto desktop = GetDesktopRect();
		Size desktopSize;
		desktopSize.width = uint32_t( desktop.right - desktop.left );
		desktopSize.height = uint32_t( desktop.bottom - desktop.top );

		state.width = std::min( desktopSize.width, state.width );
		state.height = std::min( desktopSize.height, state.height );
	}
}

void Tr2MainWindow::SanitizeWindowPosition( Tr2MainWindowState::State& state ) const
{
	if( state.windowMode == Tr2WindowMode::FIXED_WINDOW )
	{
		// fixed windows are always at the top left corner of the display
		auto rect = GetMonitorRect( state.adapter );
		state.left = rect.left;
		state.top = rect.top;
	}
	else
	{
		// window should intersect desktop with some margin
		Size size = { state.width, state.height };
		size = GetWindowSize( size, state.windowMode );

		auto margin = int32_t( std::max( std::min( size.width / 2, size.height / 2 ), 50u ) );

		auto desktop = GetDesktopRect();

		state.left = std::max( state.left, int32_t( desktop.left - size.width + margin ) );
		state.top = std::max( state.top, int32_t( desktop.top - size.height + margin ) );
		state.left = std::min( state.left, int32_t( desktop.right - margin ) );
		state.top = std::min( state.top, int32_t( desktop.bottom - margin ) );
	}
}

Tr2MainWindow::Size Tr2MainWindow::GetLargestWindowSize( uint32_t, Tr2WindowMode::Type ) const
{
	auto desktopRect = GetDesktopRect();
	return { uint32_t( desktopRect.right - desktopRect.left ), uint32_t( desktopRect.bottom - desktopRect.top ) };
}

Tr2WindowHandle Tr2MainWindow::GetOutputWindow() const
{
	return m_hwnd;
}

#endif // __linux__ && !TRINITY_VULKAN

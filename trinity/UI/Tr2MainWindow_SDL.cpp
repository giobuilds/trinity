// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#if defined( __linux__ ) && TRINITY_PLATFORM == TRINITY_VULKAN

#include "Tr2MainWindow.h"
#include "Tr2MouseCursor.h"
#include "TriDevice.h"
#include "Scancodes.h"
#include "VulkanWindowSystem.h"

#include "BluePlatformServices.h"

#include <SDL3/SDL.h>

extern CcpMeanStatisticsEntry g_activeFrametimeMean;
extern CcpStdDevStatisticsEntry g_activeFrametimeStdDev;
CCP_STATS_DECLARED_ELSEWHERE( frameTime );

// The main window on Linux: an SDL3 window (X11 or Wayland, or SDL's offscreen driver without a desktop) whose
// SDL_Window* is the Vulkan backend's Tr2WindowHandle. Events are drained on the engine tick, like macOS.
//
// Sizes: trinity's state and callbacks use back-buffer pixels. SDL positions and sizes windows in points; with
// SDL_WINDOW_HIGH_PIXEL_DENSITY the drawable has GetWindowPixelDensity() pixels per point.
// Fullscreen is borderless on the desktop (the only kind Wayland has); the back buffer keeps the requested size and the
// Vulkan backend scales it to the display when presenting. A fixed window is a borderless window at the top left of the
// display.

namespace
{

SDL_Window* ToSdl( Tr2WindowHandle handle )
{
	return reinterpret_cast<SDL_Window*>( handle );
}

std::string ToUtf8( const std::wstring& text )
{
	// wchar_t is UTF-32 on Linux.
	std::string result;
	for( wchar_t wc : text )
	{
		auto c = uint32_t( wc );
		if( c < 0x80 )
		{
			result += char( c );
		}
		else if( c < 0x800 )
		{
			result += char( 0xc0 | ( c >> 6 ) );
			result += char( 0x80 | ( c & 0x3f ) );
		}
		else if( c < 0x10000 )
		{
			result += char( 0xe0 | ( c >> 12 ) );
			result += char( 0x80 | ( ( c >> 6 ) & 0x3f ) );
			result += char( 0x80 | ( c & 0x3f ) );
		}
		else
		{
			result += char( 0xf0 | ( c >> 18 ) );
			result += char( 0x80 | ( ( c >> 12 ) & 0x3f ) );
			result += char( 0x80 | ( ( c >> 6 ) & 0x3f ) );
			result += char( 0x80 | ( c & 0x3f ) );
		}
	}
	return result;
}

// Decodes UTF-8 text input into code points (invalid bytes are skipped).
std::vector<uint32_t> DecodeUtf8( const char* text )
{
	std::vector<uint32_t> result;
	const auto* s = reinterpret_cast<const unsigned char*>( text );
	while( *s )
	{
		uint32_t c = *s;
		int extra = 0;
		if( c >= 0xf0 )
		{
			extra = 3;
		}
		else if( c >= 0xe0 )
		{
			extra = 2;
		}
		else if( c >= 0xc0 )
		{
			extra = 1;
		}
		if( c >= 0x80 && c < 0xc0 )
		{
			++s;
			continue;
		}
		c &= extra ? ( 0x3f >> extra ) : 0x7f;
		++s;
		for( int i = 0; i < extra && ( *s & 0xc0 ) == 0x80; ++i, ++s )
		{
			c = ( c << 6 ) | ( *s & 0x3f );
		}
		result.push_back( c );
	}
	return result;
}

float PixelDensity( SDL_Window* window )
{
	float density = window ? SDL_GetWindowPixelDensity( window ) : 0.0f;
	return density > 0 ? density : 1.0f;
}

// The display's pixel density before a window exists on it.
float DisplayDensity( SDL_DisplayID display )
{
	const SDL_DisplayMode* mode = display ? SDL_GetDesktopDisplayMode( display ) : nullptr;
	return mode && mode->pixel_density > 0 ? mode->pixel_density : 1.0f;
}

int ToPoints( uint32_t pixels, float density )
{
	return std::max( int( pixels / density + 0.5f ), 1 );
}

// Blue's clipboard and message boxes on Linux (BluePlatformServices), registered while this module is loaded.
bool GetClipboardText( std::string& text )
{
	if( !TrinityALImpl::InitializeWindowSystem() )
	{
		return false;
	}
	char* clipboard = SDL_GetClipboardText();
	if( !clipboard )
	{
		return false;
	}
	text = clipboard;
	SDL_free( clipboard );
	return true;
}

bool SetClipboardText( const std::string& text )
{
	return TrinityALImpl::InitializeWindowSystem() && SDL_SetClipboardText( text.c_str() );
}

bool ShowMessageBox( const char* title, const char* message )
{
	// Works without video when a desktop message-box helper exists (SDL falls back to zenity/kdialog on X11/Wayland).
	TrinityALImpl::InitializeWindowSystem();
	return SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, title, message, nullptr );
}

const BluePlatformServices s_platformServices = { GetClipboardText, SetClipboardText, ShowMessageBox };

struct PlatformServicesRegistration
{
	PlatformServicesRegistration()
	{
		BlueSetPlatformServices( &s_platformServices );
	}
	~PlatformServicesRegistration()
	{
		if( BlueGetPlatformServices() == &s_platformServices )
		{
			BlueSetPlatformServices( nullptr );
		}
	}
} s_platformServicesRegistration;

}


void Tr2MainWindow::SetWindowTitle( const wchar_t* title )
{
	m_title = title;
	if( m_hwnd )
	{
		SDL_SetWindowTitle( ToSdl( m_hwnd ), ToUtf8( m_title ).c_str() );
	}
}

bool Tr2MainWindow::HasFocus() const
{
	return m_hwnd && ( SDL_GetWindowFlags( ToSdl( m_hwnd ) ) & SDL_WINDOW_INPUT_FOCUS ) != 0;
}

bool Tr2MainWindow::HasWindow() const
{
	return m_hwnd != 0;
}

void Tr2MainWindow::CreateOSWindow( Tr2MainWindowState::State& state )
{
	if( m_hwnd || !TrinityALImpl::InitializeWindowSystem() )
	{
		return;
	}
	SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
	if( state.windowMode == Tr2WindowMode::WINDOWED )
	{
		flags |= SDL_WINDOW_RESIZABLE;
	}
	const float density = DisplayDensity( TrinityALImpl::GetAdapterDisplay( state.adapter ) );
	SDL_Window* window = SDL_CreateWindow( ToUtf8( m_title ).c_str(), ToPoints( state.width, density ), ToPoints( state.height, density ), flags );
	if( !window )
	{
		CCP_LOGERR( "Tr2MainWindow: SDL_CreateWindow failed: %s", SDL_GetError() );
		return;
	}
	m_hwnd = Tr2WindowHandle( reinterpret_cast<uintptr_t>( window ) );
	AdjustWindow( state );
	SDL_ShowWindow( window );
	SDL_StartTextInput( window );
	SDL_SyncWindow( window );
	if( m_cursor )
	{
		m_cursor->Apply();
	}
}

void Tr2MainWindow::DestroyOSWindow()
{
	if( m_hwnd )
	{
		SDL_Window* window = ToSdl( m_hwnd );
		SDL_StopTextInput( window );
		SDL_DestroyWindow( window );
	}
	m_hwnd = 0;
}

void Tr2MainWindow::AdjustWindow( Tr2MainWindowState::State& state )
{
	if( !m_hwnd )
	{
		return;
	}
	SDL_Window* window = ToSdl( m_hwnd );
	const SDL_DisplayID display = TrinityALImpl::GetAdapterDisplay( state.adapter );

	if( state.windowMode == Tr2WindowMode::FULL_SCREEN )
	{
		SDL_SetWindowFullscreenMode( window, nullptr ); // borderless desktop fullscreen
		SDL_SetWindowFullscreen( window, true );
		SDL_SyncWindow( window );
		return;
	}

	SDL_SetWindowFullscreen( window, false );
	const bool windowed = state.windowMode == Tr2WindowMode::WINDOWED;
	SDL_SetWindowBordered( window, windowed );
	SDL_SetWindowResizable( window, windowed );
	const float density = PixelDensity( window );
	SDL_SetWindowMinimumSize( window, ToPoints( m_minimumSize.width, density ), ToPoints( m_minimumSize.height, density ) );
	if( !windowed || state.showState == Tr2WindowShowState::NORMAL )
	{
		SDL_RestoreWindow( window );
		SDL_SetWindowSize( window, ToPoints( state.width, density ), ToPoints( state.height, density ) );
	}
	if( !windowed )
	{
		SDL_Rect bounds;
		if( display && SDL_GetDisplayBounds( display, &bounds ) )
		{
			SDL_SetWindowPosition( window, bounds.x, bounds.y );
		}
	}
	else
	{
		SDL_SetWindowPosition( window, state.left, state.top ); // Wayland ignores this: the compositor places windows
		if( state.showState == Tr2WindowShowState::MAXIMIZED )
		{
			SDL_MaximizeWindow( window );
		}
		else if( state.showState == Tr2WindowShowState::MINIMIZED )
		{
			SDL_MinimizeWindow( window );
		}
	}
	SDL_SyncWindow( window );
}

Tr2MainWindow::Rect Tr2MainWindow::GetDesktopRect() const
{
	return GetMonitorRect( 0 );
}

// Display bounds in points for positions and in pixels for size, as trinity sizes are back-buffer pixels.
Tr2MainWindow::Rect Tr2MainWindow::GetMonitorRect( uint32_t adapter ) const
{
	SDL_DisplayID display = TrinityALImpl::GetAdapterDisplay( adapter );
	SDL_Rect bounds;
	if( display && SDL_GetDisplayBounds( display, &bounds ) )
	{
		const float density = DisplayDensity( display );
		return { bounds.x, bounds.y, bounds.x + int32_t( bounds.w * density + 0.5f ), bounds.y + int32_t( bounds.h * density + 0.5f ) };
	}
	Tr2DisplayModeInfo mi;
	if( FAILED( Tr2VideoAdapterInfo::GetAdapterDisplayMode( adapter, mi ) ) )
	{
		return {};
	}
	return { 0, 0, int32_t( mi.width ), int32_t( mi.height ) };
}

// The window manager draws decorations on X11 and (with libdecor) on Wayland, outside the client area SDL sizes.
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

void Tr2MainWindow::ClipCursor( int32_t left, int32_t top, int32_t right, int32_t bottom )
{
	if( !m_hwnd || m_state.width == 0 || m_state.height == 0 )
	{
		return;
	}
	SDL_Window* window = ToSdl( m_hwnd );
	int w = 0, h = 0;
	SDL_GetWindowSize( window, &w, &h );
	SDL_Rect rect;
	rect.x = int( double( left ) / m_state.width * w + 0.5 );
	rect.y = int( double( top ) / m_state.height * h + 0.5 );
	rect.w = std::max( int( double( right ) / m_state.width * w + 0.5 ) - rect.x, 1 );
	rect.h = std::max( int( double( bottom ) / m_state.height * h + 0.5 ) - rect.y, 1 );
	SDL_SetWindowMouseRect( window, &rect );
}

void Tr2MainWindow::UnclipCursor()
{
	if( m_hwnd )
	{
		SDL_SetWindowMouseRect( ToSdl( m_hwnd ), nullptr );
	}
}

void Tr2MainWindow::SetCursorPos( int32_t x, int32_t y )
{
	if( !m_hwnd || m_state.width == 0 || m_state.height == 0 )
	{
		return;
	}
	SDL_Window* window = ToSdl( m_hwnd );
	int w = 0, h = 0;
	SDL_GetWindowSize( window, &w, &h );
	SDL_WarpMouseInWindow( window, float( double( x ) / m_state.width * w ), float( double( y ) / m_state.height * h ) );
}

std::pair<int32_t, int32_t> Tr2MainWindow::GetCursorPos() const
{
	if( !m_hwnd )
	{
		return { 0, 0 };
	}
	SDL_Window* window = ToSdl( m_hwnd );
	float x = 0, y = 0;
	if( SDL_GetMouseFocus() == window )
	{
		SDL_GetMouseState( &x, &y );
	}
	else
	{
		// Outside the window: global position relative to it (not available on Wayland, which reports the last position).
		float gx = 0, gy = 0;
		int wx = 0, wy = 0;
		SDL_GetGlobalMouseState( &gx, &gy );
		SDL_GetWindowPosition( window, &wx, &wy );
		x = gx - wx;
		y = gy - wy;
	}
	int w = 0, h = 0;
	SDL_GetWindowSize( window, &w, &h );
	if( w <= 0 || h <= 0 )
	{
		return { 0, 0 };
	}
	return { int32_t( double( x ) / w * m_state.width + 0.5 ), int32_t( double( y ) / h * m_state.height + 0.5 ) };
}

bool Tr2MainWindow::IsKeyToggled( uint32_t keyCode ) const
{
	SDL_Keymod mods = SDL_GetModState();
	switch( keyCode )
	{
	case VK_CAPITAL:
		return ( mods & SDL_KMOD_CAPS ) != 0;
	case VK_NUMLOCK:
		return ( mods & SDL_KMOD_NUM ) != 0;
	case VK_SCROLL:
		return ( mods & SDL_KMOD_SCROLL ) != 0;
	default:
		return false;
	}
}

bool Tr2MainWindow::IsKeyPressed( uint32_t keyCode ) const
{
	return KeyboardHelpers::IsAppKeyPressed( KeyboardHelpers::AppKey( keyCode ) );
}

std::string Tr2MainWindow::GetKeyName( uint32_t keyCode ) const
{
	return KeyboardHelpers::GetAppKeyName( KeyboardHelpers::AppKey( keyCode ) );
}

// Drains SDL's queue; false once the main window's close was allowed by onClose.
bool Tr2MainWindow::ProcessMessages()
{
	if( !m_hwnd )
	{
		return true;
	}
	bool keepRunning = true;
	SDL_Event event;
	while( SDL_PollEvent( &event ) )
	{
		keepRunning &= OnEvent_SDL( event );
	}
	return keepRunning;
}

bool Tr2MainWindow::OnEvent_SDL( const SDL_Event& event )
{
	SDL_Window* window = ToSdl( m_hwnd );
	SDL_Window* target = SDL_GetWindowFromEvent( &event );
	if( target && target != window )
	{
		return true; // another window (e.g. a standalone swapchain's)
	}

	auto toBackBuffer = [&]( float x, float y ) {
		int w = 0, h = 0;
		SDL_GetWindowSize( window, &w, &h );
		return std::make_pair( w > 0 ? int32_t( x / w * m_state.width + 0.5f ) : 0, h > 0 ? int32_t( y / h * m_state.height + 0.5f ) : 0 );
	};

	switch( event.type )
	{
	case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
		bool canClose = true;
		m_onClose.Call( canClose );
		return !canClose;
	}
	case SDL_EVENT_WINDOW_FOCUS_GAINED:
	case SDL_EVENT_WINDOW_FOCUS_LOST: {
		const bool focused = event.type == SDL_EVENT_WINDOW_FOCUS_GAINED;
		if( gTriDev )
		{
			gTriDev->ApplicationActivated( focused ? TriDevice::APP_ACTIVATED : TriDevice::APP_DEACTIVATED );
			gTriDev->SetThrottling( TriDevice::WINDOW_OUT_OF_FOCUS, !focused );
		}
		g_activeFrametimeMean.SetSource( focused ? &g_ccpStatistics_frameTime : nullptr );
		g_activeFrametimeStdDev.SetSource( focused ? &g_ccpStatistics_frameTime : nullptr );
		m_onFocusChange.CallVoid( focused );
		break;
	}
	case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
	case SDL_EVENT_WINDOW_MAXIMIZED:
	case SDL_EVENT_WINDOW_MINIMIZED:
	case SDL_EVENT_WINDOW_RESTORED: {
		if( m_isResizing || m_inSetState || m_state.windowMode == Tr2WindowMode::FULL_SCREEN )
		{
			if( gTriDev && event.type == SDL_EVENT_WINDOW_MINIMIZED )
			{
				gTriDev->SetThrottling( TriDevice::WINDOW_HIDDEN, true );
			}
			else if( gTriDev && event.type == SDL_EVENT_WINDOW_RESTORED )
			{
				gTriDev->SetThrottling( TriDevice::WINDOW_HIDDEN, false );
			}
			break;
		}
		const SDL_WindowFlags flags = SDL_GetWindowFlags( window );
		auto newState = m_state;
		if( flags & SDL_WINDOW_MINIMIZED )
		{
			newState.showState = Tr2WindowShowState::MINIMIZED; // keep the size: the back buffer stays as it was
		}
		else
		{
			int w = 0, h = 0;
			SDL_GetWindowSizeInPixels( window, &w, &h );
			if( w > 0 && h > 0 )
			{
				newState.width = uint32_t( w );
				newState.height = uint32_t( h );
			}
			newState.showState = ( flags & SDL_WINDOW_MAXIMIZED ) ? Tr2WindowShowState::MAXIMIZED : Tr2WindowShowState::NORMAL;
		}
		SetState( false, newState );
		break;
	}
	case SDL_EVENT_WINDOW_MOVED:
		if( !m_isResizing && !m_inSetState && m_state.windowMode == Tr2WindowMode::WINDOWED )
		{
			auto newState = m_state;
			newState.left = event.window.data1;
			newState.top = event.window.data2;
			SetState( false, newState );
		}
		break;
	case SDL_EVENT_WINDOW_OCCLUDED:
	case SDL_EVENT_WINDOW_EXPOSED:
		if( gTriDev )
		{
			gTriDev->SetThrottling( TriDevice::WINDOW_HIDDEN, event.type == SDL_EVENT_WINDOW_OCCLUDED );
		}
		break;
	case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED: {
		const float density = PixelDensity( window );
		SDL_SetWindowMinimumSize( window, ToPoints( m_minimumSize.width, density ), ToPoints( m_minimumSize.height, density ) );
		break;
	}
	case SDL_EVENT_MOUSE_MOTION: {
		auto pos = toBackBuffer( event.motion.x, event.motion.y );
		m_onMouseMove.CallVoid( pos.first, pos.second );
		break;
	}
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP: {
		// Trinity's buttons are Windows': 0 left, 1 right, 2 middle, 3 and 4 the X buttons.
		int32_t button;
		switch( event.button.button )
		{
		case SDL_BUTTON_LEFT:
			button = 0;
			break;
		case SDL_BUTTON_RIGHT:
			button = 1;
			break;
		case SDL_BUTTON_MIDDLE:
			button = 2;
			break;
		case SDL_BUTTON_X1:
			button = 3;
			break;
		case SDL_BUTTON_X2:
			button = 4;
			break;
		default:
			return true;
		}
		auto pos = toBackBuffer( event.button.x, event.button.y );
		if( event.button.down )
		{
			m_onMouseDown.CallVoid( button, pos.first, pos.second );
		}
		else
		{
			m_onMouseUp.CallVoid( button, pos.first, pos.second );
		}
		break;
	}
	case SDL_EVENT_MOUSE_WHEEL: {
		// Windows' WHEEL_DELTA units: 120 per notch; SDL reports notches (fractions on smooth-scrolling devices).
		float y = event.wheel.y * ( event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f );
		auto delta = int32_t( y * 120.0f );
		if( delta != 0 )
		{
			m_onMouseWheel.CallVoid( delta );
		}
		break;
	}
	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_KEY_UP: {
		auto k = KeyboardHelpers::PlatformKeyToAppKey( KeyboardHelpers::PlatformKey( event.key.scancode ) );
		if( k == KeyboardHelpers::INVALID_APP_KEY )
		{
			break;
		}
		// As on the other platforms, left/right modifiers arrive as the generic key.
		if( k == VK_LSHIFT || k == VK_RSHIFT )
		{
			k = VK_SHIFT;
		}
		else if( k == VK_LCONTROL || k == VK_RCONTROL )
		{
			k = VK_CONTROL;
		}
		else if( k == VK_LMENU || k == VK_RMENU )
		{
			k = VK_MENU;
		}
		if( event.key.down )
		{
			m_onKeyDown.CallVoid( int32_t( k ), event.key.repeat ? int32_t( 0x40000000 ) : int32_t( 0 ) );
		}
		else
		{
			m_onKeyUp.CallVoid( int32_t( k ), int32_t( 0 ) );
		}
		break;
	}
	case SDL_EVENT_TEXT_INPUT:
		for( uint32_t c : DecodeUtf8( event.text.text ) )
		{
			m_onChar.CallVoid( int32_t( c ), int32_t( 0 ), false );
		}
		break;
	default:
		break;
	}
	return true;
}

uintptr_t Tr2MainWindow::GetHwnd() const
{
	return uintptr_t( m_hwnd );
}

void Tr2MainWindow::OnTick( Be::Time, Be::Time, void* )
{
	if( !ProcessMessages() )
	{
		BeOS->Terminate();
	}
}

void Tr2MainWindow::SanitizeWindowedResolution( Tr2MainWindowState::State& state ) const
{
	if( state.width == 0 && state.height == 0 )
	{
		// use window size that spans default display
		Tr2DisplayModeInfo mi;
		Tr2VideoAdapterInfo::GetAdapterDisplayMode( state.adapter, mi );
		state.width = mi.width;
		state.height = mi.height;
	}
	else
	{
		// clamp size between window min size and desktop size
		state.width = std::max( m_minimumSize.width, state.width );
		state.height = std::max( m_minimumSize.height, state.height );
		auto largest = GetLargestWindowSize( state.adapter, state.windowMode );
		state.width = std::min( largest.width, state.width );
		state.height = std::min( largest.height, state.height );
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
		// window should intersect desktop with some margin (positions are in points, sizes in pixels)
		SDL_DisplayID display = TrinityALImpl::GetAdapterDisplay( state.adapter );
		const float density = DisplayDensity( display );
		auto width = int32_t( state.width / density ), height = int32_t( state.height / density );
		auto margin = std::max( std::min( width / 2, height / 2 ), 50 );
		SDL_Rect bounds{ 0, 0, 0, 0 };
		if( !display || !SDL_GetDisplayUsableBounds( display, &bounds ) )
		{
			return;
		}
		state.left = std::max( state.left, bounds.x - width + margin );
		state.top = std::max( state.top, bounds.y - height + margin );
		state.left = std::min( state.left, bounds.x + bounds.w - margin );
		state.top = std::min( state.top, bounds.y + bounds.h - margin );
	}
}

// Pixels: the display's usable area for windows, the whole display otherwise.
Tr2MainWindow::Size Tr2MainWindow::GetLargestWindowSize( uint32_t adapter, Tr2WindowMode::Type mode ) const
{
	SDL_DisplayID display = TrinityALImpl::GetAdapterDisplay( adapter );
	SDL_Rect bounds;
	if( display && ( mode == Tr2WindowMode::WINDOWED ? SDL_GetDisplayUsableBounds( display, &bounds ) : SDL_GetDisplayBounds( display, &bounds ) ) )
	{
		const float density = DisplayDensity( display );
		return { uint32_t( bounds.w * density + 0.5f ), uint32_t( bounds.h * density + 0.5f ) };
	}
	auto desktopRect = GetDesktopRect();
	return { uint32_t( desktopRect.right - desktopRect.left ), uint32_t( desktopRect.bottom - desktopRect.top ) };
}

Tr2WindowHandle Tr2MainWindow::GetOutputWindow() const
{
	return m_hwnd;
}

#endif // __linux__ && TRINITY_VULKAN

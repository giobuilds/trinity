// Copyright © 2023 CCP ehf.

#include "StdAfx.h"
#include "Scancodes.h"

#define CODE( vk, desc ) { vk, #vk, desc }


const UIScancode SCANCODES[] = {
	CODE( VK_LBUTTON, "Left mouse button" ),
	CODE( VK_RBUTTON, "Right mouse button" ),
	CODE( VK_CANCEL, "Control-break processing" ),
	CODE( VK_MBUTTON, "Middle mouse button (three-button mouse)" ),
	CODE( VK_XBUTTON1, "Windows 2000/XP: X1 mouse button" ),
	CODE( VK_XBUTTON2, "Windows 2000/XP: X2 mouse button" ),
	CODE( VK_BACK, "BACKSPACE key" ),
	CODE( VK_TAB, "TAB key" ),
	CODE( VK_CLEAR, "CLEAR key" ),
	CODE( VK_RETURN, "ENTER key" ),
	CODE( VK_SHIFT, "SHIFT key" ),
	CODE( VK_CONTROL, "CTRL key" ),
	CODE( VK_MENU, "ALT key" ),
	CODE( VK_PAUSE, "PAUSE key" ),
	CODE( VK_CAPITAL, "CAPS LOCK key" ),
	CODE( VK_KANA, "Input Method Editor (IME) Kana mode" ),
	//CODE(VK_HANGUEL,		"IME Hanguel mode (maintained for compatibility; use VK_HANGUL)"),
	CODE( VK_HANGUL, "IME Hangul mode" ),
	CODE( VK_JUNJA, "IME Junja mode" ),
	CODE( VK_FINAL, "IME final mode" ),
	CODE( VK_HANJA, "IME Hanja mode" ),
	CODE( VK_KANJI, "IME Kanji mode" ),
	CODE( VK_ESCAPE, "ESC key" ),
	CODE( VK_CONVERT, "IME convert" ),
	CODE( VK_NONCONVERT, "IME nonconvert" ),
	CODE( VK_ACCEPT, "IME accept" ),
	CODE( VK_MODECHANGE, "IME mode change request" ),
	CODE( VK_SPACE, "SPACEBAR" ),
	CODE( VK_PRIOR, "PAGE UP key" ),
	CODE( VK_NEXT, "PAGE DOWN key" ),
	CODE( VK_END, "END key" ),
	CODE( VK_HOME, "HOME key" ),
	CODE( VK_LEFT, "LEFT ARROW key" ),
	CODE( VK_UP, "UP ARROW key" ),
	CODE( VK_RIGHT, "RIGHT ARROW key" ),
	CODE( VK_DOWN, "DOWN ARROW key" ),
	CODE( VK_SELECT, "SELECT key" ),
	CODE( VK_PRINT, "PRINT key" ),
	CODE( VK_EXECUTE, "EXECUTE key" ),
	CODE( VK_SNAPSHOT, "PRINT SCREEN key" ),
	CODE( VK_INSERT, "INS key" ),
	CODE( VK_DELETE, "DEL key" ),
	CODE( VK_HELP, "HELP key" ),
	CODE( VK_0, "0 key" ),
	CODE( VK_1, "1 key" ),
	CODE( VK_2, "2 key" ),
	CODE( VK_3, "3 key" ),
	CODE( VK_4, "4 key" ),
	CODE( VK_5, "5 key" ),
	CODE( VK_6, "6 key" ),
	CODE( VK_7, "7 key" ),
	CODE( VK_8, "8 key" ),
	CODE( VK_9, "9 key" ),
	CODE( VK_A, "A key" ),
	CODE( VK_B, "B key" ),
	CODE( VK_C, "C key" ),
	CODE( VK_D, "D key" ),
	CODE( VK_E, "E key" ),
	CODE( VK_F, "F key" ),
	CODE( VK_G, "G key" ),
	CODE( VK_H, "H key" ),
	CODE( VK_I, "I key" ),
	CODE( VK_J, "J key" ),
	CODE( VK_K, "K key" ),
	CODE( VK_L, "L key" ),
	CODE( VK_M, "M key" ),
	CODE( VK_N, "N key" ),
	CODE( VK_O, "O key" ),
	CODE( VK_P, "P key" ),
	CODE( VK_Q, "Q key" ),
	CODE( VK_R, "R key" ),
	CODE( VK_S, "S key" ),
	CODE( VK_T, "T key" ),
	CODE( VK_U, "U key" ),
	CODE( VK_V, "V key" ),
	CODE( VK_W, "W key" ),
	CODE( VK_X, "X key" ),
	CODE( VK_Y, "Y key" ),
	CODE( VK_Z, "Z key" ),
	CODE( VK_LWIN, "Left Windows key (Microsoft Natural keyboard) " ),
	CODE( VK_RWIN, "Right Windows key (Natural keyboard)" ),
	CODE( VK_APPS, "Applications key (Natural keyboard)" ),
	CODE( VK_SLEEP, "Computer Sleep key" ),
	CODE( VK_NUMPAD0, "Numeric keypad 0 key" ),
	CODE( VK_NUMPAD1, "Numeric keypad 1 key" ),
	CODE( VK_NUMPAD2, "Numeric keypad 2 key" ),
	CODE( VK_NUMPAD3, "Numeric keypad 3 key" ),
	CODE( VK_NUMPAD4, "Numeric keypad 4 key" ),
	CODE( VK_NUMPAD5, "Numeric keypad 5 key" ),
	CODE( VK_NUMPAD6, "Numeric keypad 6 key" ),
	CODE( VK_NUMPAD7, "Numeric keypad 7 key" ),
	CODE( VK_NUMPAD8, "Numeric keypad 8 key" ),
	CODE( VK_NUMPAD9, "Numeric keypad 9 key" ),
	CODE( VK_MULTIPLY, "Multiply key" ),
	CODE( VK_ADD, "Add key" ),
	CODE( VK_SEPARATOR, "Separator key" ),
	CODE( VK_SUBTRACT, "Subtract key" ),
	CODE( VK_DECIMAL, "Decimal key" ),
	CODE( VK_DIVIDE, "Divide key" ),
	CODE( VK_F1, "F1 key" ),
	CODE( VK_F2, "F2 key" ),
	CODE( VK_F3, "F3 key" ),
	CODE( VK_F4, "F4 key" ),
	CODE( VK_F5, "F5 key" ),
	CODE( VK_F6, "F6 key" ),
	CODE( VK_F7, "F7 key" ),
	CODE( VK_F8, "F8 key" ),
	CODE( VK_F9, "F9 key" ),
	CODE( VK_F10, "F10 key" ),
	CODE( VK_F11, "F11 key" ),
	CODE( VK_F12, "F12 key" ),
	CODE( VK_F13, "F13 key" ),
	CODE( VK_F14, "F14 key" ),
	CODE( VK_F15, "F15 key" ),
	CODE( VK_F16, "F16 key" ),
	CODE( VK_F17, "F17 key" ),
	CODE( VK_F18, "F18 key" ),
	CODE( VK_F19, "F19 key" ),
	CODE( VK_F20, "F20 key" ),
	CODE( VK_F21, "F21 key" ),
	CODE( VK_F22, "F22 key" ),
	CODE( VK_F23, "F23 key" ),
	CODE( VK_F24, "F24 key" ),
	CODE( VK_NUMLOCK, "NUM LOCK key" ),
	CODE( VK_SCROLL, "SCROLL LOCK key" ),
	CODE( VK_LSHIFT, "Left SHIFT key" ),
	CODE( VK_RSHIFT, "Right SHIFT key" ),
	CODE( VK_LCONTROL, "Left CONTROL key" ),
	CODE( VK_RCONTROL, "Right CONTROL key" ),
	CODE( VK_LMENU, "Left MENU key" ),
	CODE( VK_RMENU, "Right MENU key" ),
	CODE( VK_BROWSER_BACK, "Windows 2000/XP: Browser Back key" ),
	CODE( VK_BROWSER_FORWARD, "Windows 2000/XP: Browser Forward key" ),
	CODE( VK_BROWSER_REFRESH, "Windows 2000/XP: Browser Refresh key" ),
	CODE( VK_BROWSER_STOP, "Windows 2000/XP: Browser Stop key" ),
	CODE( VK_BROWSER_SEARCH, "Windows 2000/XP: Browser Search key " ),
	CODE( VK_BROWSER_FAVORITES, "Windows 2000/XP: Browser Favorites key" ),
	CODE( VK_BROWSER_HOME, "Windows 2000/XP: Browser Start and Home key" ),
	CODE( VK_VOLUME_MUTE, "Windows 2000/XP: Volume Mute key" ),
	CODE( VK_VOLUME_DOWN, "Windows 2000/XP: Volume Down key" ),
	CODE( VK_VOLUME_UP, "Windows 2000/XP: Volume Up key" ),
	CODE( VK_MEDIA_NEXT_TRACK, "Windows 2000/XP: Next Track key" ),
	CODE( VK_MEDIA_PREV_TRACK, "Windows 2000/XP: Previous Track key" ),
	CODE( VK_MEDIA_STOP, "Windows 2000/XP: Stop Media key" ),
	CODE( VK_MEDIA_PLAY_PAUSE, "Windows 2000/XP: Play/Pause Media key" ),
	CODE( VK_LAUNCH_MAIL, "Windows 2000/XP: Start Mail key" ),
	CODE( VK_LAUNCH_MEDIA_SELECT, "Windows 2000/XP: Select Media key" ),
	CODE( VK_LAUNCH_APP1, "Windows 2000/XP: Start Application 1 key" ),
	CODE( VK_LAUNCH_APP2, "Windows 2000/XP: Start Application 2 key" ),
	CODE( VK_OEM_1, "Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the ';:' key" ),
	CODE( VK_OEM_PLUS, "Windows 2000/XP: For any country/region, the '+' key" ),
	CODE( VK_OEM_COMMA, "Windows 2000/XP: For any country/region, the ',' key" ),
	CODE( VK_OEM_MINUS, "Windows 2000/XP: For any country/region, the '-' key" ),
	CODE( VK_OEM_PERIOD, "Windows 2000/XP: For any country/region, the '.' key" ),
	CODE( VK_OEM_2, "Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the '/?' key " ),
	CODE( VK_OEM_3, "Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the '`~' key " ),
	CODE( VK_OEM_4, "Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the '[{' key" ),
	CODE( VK_OEM_5, "Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the '\\|' key" ),
	CODE( VK_OEM_6, "Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the ']}' key" ),
	CODE( VK_OEM_7, "Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the 'single-quote/double-quote' key" ),
	CODE( VK_OEM_8, "Used for miscellaneous characters; it can vary by keyboard." ),
	CODE( VK_OEM_102, "Windows 2000/XP: Either the angle bracket key or the backslash key on the RT 102-key keyboard" ),
	CODE( VK_PROCESSKEY, "Windows 95/98/Me, Windows NT 4.0, Windows 2000/XP: IME PROCESS key" ),
	CODE( VK_PACKET, "Windows 2000/XP: Used to pass Unicode characters as if they were keystrokes. The VK_PACKET key is the low word of a 32-bit Virtual Key value used for non-keyboard input methods. For more information, see Remark in KEYBDINPUT, SendInput, WM_KEYDOWN, and WM_KEYUP" ),
	CODE( VK_ATTN, "Attn key" ),
	CODE( VK_CRSEL, "CrSel key" ),
	CODE( VK_EXSEL, "ExSel key" ),
	CODE( VK_EREOF, "Erase EOF key" ),
	CODE( VK_PLAY, "Play key" ),
	CODE( VK_ZOOM, "Zoom key" ),
	CODE( VK_NONAME, "Reserved for future use" ),
	CODE( VK_PA1, "PA1 key" ),
	CODE( VK_OEM_CLEAR, "Clear key" ),
};

#if BLUE_WITH_PYTHON
void AddScancodesToDict( PyObject* dict )
{
	for( int i = 0; i < sizeof SCANCODES / sizeof SCANCODES[0]; i++ )
	{
		PyObject* value = ToPython( SCANCODES[i].mDIK );
		PyDict_SetItemString( dict, (char*)SCANCODES[i].mName, value );
		Py_DECREF( value );
	}
}
#endif


const UIScancode* GetUIScancode( unsigned char scancode )
{
	for( int i = 0; i < sizeof SCANCODES / sizeof SCANCODES[0]; i++ )
	{
		if( SCANCODES[i].mDIK == scancode )
			return SCANCODES + i;
	}

	return NULL;
}


#ifdef __APPLE__

namespace
{
/* ISO keyboards only*/
enum
{
	kVK_ISO_Section = 0x0A
};

/* JIS keyboards only*/
enum
{
	kVK_JIS_Yen = 0x5D,
	kVK_JIS_Underscore = 0x5E,
	kVK_JIS_KeypadComma = 0x5F,
	kVK_JIS_Eisu = 0x66,
	kVK_JIS_Kana = 0x68
};


struct KeyDesc
{
	uint16_t winCode;
	uint16_t macCode;
	const char* text;
};

const KeyDesc s_keyCodes[] = {
	{ VK_BACK, kVK_Delete, "Backspace" },
	{ VK_TAB, kVK_Tab, "Tab" },
	{ VK_CLEAR, kVK_ANSI_KeypadClear, "Clear" },
	{ VK_RETURN, kVK_Return, "Enter" },
	// VK_PAUSE?
	{ VK_CAPITAL, kVK_CapsLock, "Caps Lock" },
	{ VK_KANA, kVK_JIS_Kana, "Kana" },
	// IME weird things like VK_JUNJA?
	{ VK_ESCAPE, kVK_Escape, "Esc" },
	{ VK_SPACE, kVK_Space, "Space" },
	{ VK_PRIOR, kVK_PageUp, "Page Up" },
	{ VK_NEXT, kVK_PageDown, "Page Down" },
	{ VK_END, kVK_End, "End" },
	{ VK_HOME, kVK_Home, "Home" },
	{ VK_LEFT, kVK_LeftArrow, "Left" },
	{ VK_UP, kVK_UpArrow, "Up" },
	{ VK_RIGHT, kVK_RightArrow, "Right" },
	{ VK_DOWN, kVK_DownArrow, "Down" },
	// VK_SELECT, VK_PRINT, VK_EXECUTE, VK_SNAPSHOT?
	{ VK_INSERT, kVK_Function, "Fn" },
	{ VK_DELETE, kVK_ForwardDelete, "Delete" },

	{ VK_0, kVK_ANSI_0, "0" },
	{ VK_1, kVK_ANSI_1, "1" },
	{ VK_2, kVK_ANSI_2, "2" },
	{ VK_3, kVK_ANSI_3, "3" },
	{ VK_4, kVK_ANSI_4, "4" },
	{ VK_5, kVK_ANSI_5, "5" },
	{ VK_6, kVK_ANSI_6, "6" },
	{ VK_7, kVK_ANSI_7, "7" },
	{ VK_8, kVK_ANSI_8, "8" },
	{ VK_9, kVK_ANSI_9, "9" },

	{ VK_A, kVK_ANSI_A, "A" },
	{ VK_B, kVK_ANSI_B, "B" },
	{ VK_C, kVK_ANSI_C, "C" },
	{ VK_D, kVK_ANSI_D, "D" },
	{ VK_E, kVK_ANSI_E, "E" },
	{ VK_F, kVK_ANSI_F, "F" },
	{ VK_G, kVK_ANSI_G, "G" },
	{ VK_H, kVK_ANSI_H, "H" },
	{ VK_I, kVK_ANSI_I, "I" },
	{ VK_J, kVK_ANSI_J, "J" },
	{ VK_K, kVK_ANSI_K, "K" },
	{ VK_L, kVK_ANSI_L, "L" },
	{ VK_M, kVK_ANSI_M, "M" },
	{ VK_N, kVK_ANSI_N, "N" },
	{ VK_O, kVK_ANSI_O, "O" },
	{ VK_P, kVK_ANSI_P, "P" },
	{ VK_Q, kVK_ANSI_Q, "Q" },
	{ VK_R, kVK_ANSI_R, "R" },
	{ VK_S, kVK_ANSI_S, "S" },
	{ VK_T, kVK_ANSI_T, "T" },
	{ VK_U, kVK_ANSI_U, "U" },
	{ VK_V, kVK_ANSI_V, "V" },
	{ VK_W, kVK_ANSI_W, "W" },
	{ VK_X, kVK_ANSI_X, "X" },
	{ VK_Y, kVK_ANSI_Y, "Y" },
	{ VK_Z, kVK_ANSI_Z, "Z" },

	{ VK_NUMPAD0, kVK_ANSI_Keypad0, "Num 0" },
	{ VK_NUMPAD1, kVK_ANSI_Keypad1, "Num 1" },
	{ VK_NUMPAD2, kVK_ANSI_Keypad2, "Num 2" },
	{ VK_NUMPAD3, kVK_ANSI_Keypad3, "Num 3" },
	{ VK_NUMPAD4, kVK_ANSI_Keypad4, "Num 4" },
	{ VK_NUMPAD5, kVK_ANSI_Keypad5, "Num 5" },
	{ VK_NUMPAD6, kVK_ANSI_Keypad6, "Num 6" },
	{ VK_NUMPAD7, kVK_ANSI_Keypad7, "Num 7" },
	{ VK_NUMPAD8, kVK_ANSI_Keypad8, "Num 8" },
	{ VK_NUMPAD9, kVK_ANSI_Keypad9, "Num 9" },

	{ VK_RETURN, kVK_ANSI_KeypadEnter, "Enter" },

	{ VK_MULTIPLY, kVK_ANSI_KeypadMultiply, "Num *" },
	{ VK_ADD, kVK_ANSI_KeypadPlus, "Num +" },
	// VK_SEPARATOR?
	{ VK_SUBTRACT, kVK_ANSI_KeypadMinus, "Num -" },
	{ VK_DECIMAL, kVK_ANSI_KeypadDecimal, "Num ," },
	{ VK_DIVIDE, kVK_ANSI_KeypadDivide, "Num /" },
	{ VK_F1, kVK_F1, "F1" },
	{ VK_F2, kVK_F2, "F2" },
	{ VK_F3, kVK_F3, "F3" },
	{ VK_F4, kVK_F4, "F4" },
	{ VK_F5, kVK_F5, "F5" },
	{ VK_F6, kVK_F6, "F6" },
	{ VK_F7, kVK_F7, "F7" },
	{ VK_F8, kVK_F8, "F8" },
	{ VK_F9, kVK_F9, "F9" },
	{ VK_F10, kVK_F10, "F10" },
	{ VK_F11, kVK_F11, "F11" },
	{ VK_F12, kVK_F12, "F12" },
	{ VK_F13, kVK_F13, "F13" },
	{ VK_F14, kVK_F14, "F14" },
	{ VK_F15, kVK_F15, "F15" },
	{ VK_F16, kVK_F16, "F16" },
	{ VK_F17, kVK_F17, "F17" },
	{ VK_F18, kVK_F18, "F18" },
	{ VK_F19, kVK_F19, "F19" },
	{ VK_F20, kVK_F20, "F20" },

	{ VK_LSHIFT, kVK_Shift, "Shift" },
	{ VK_RSHIFT, kVK_RightShift, "Shift" },
	{ VK_SHIFT, kVK_Shift, "Shift" },

	{ VK_LCONTROL, kVK_Control, "Ctrl" },
	{ VK_RCONTROL, kVK_RightControl, "Ctrl" },
	{ VK_CONTROL, kVK_Control, "Ctrl" },

	{ VK_LWIN, kVK_Command, "Cmd" },
	{ VK_RWIN, kVK_RightCommand, "Cmd" },

	{ VK_LMENU, kVK_Option, "Alt" },
	{ VK_RMENU, kVK_RightOption, "Alt" },
	{ VK_MENU, kVK_Option, "Alt" },

	{ VK_OEM_1, kVK_ANSI_Semicolon, ";" },
	{ VK_OEM_PLUS, kVK_ANSI_Equal, "=" },
	{ VK_OEM_COMMA, kVK_ANSI_Comma, "," },
	{ VK_OEM_MINUS, kVK_ANSI_Minus, "-" },
	{ VK_OEM_PERIOD, kVK_ANSI_Period, "." },
	{ VK_OEM_2, kVK_ANSI_Slash, "/" },
	{ VK_OEM_3, kVK_ANSI_Grave, "`" },

	{ VK_OEM_4, kVK_ANSI_LeftBracket, "[" },
	{ VK_OEM_5, kVK_ANSI_Backslash, "\\" },
	{ VK_OEM_6, kVK_ANSI_RightBracket, "]" },
	{ VK_OEM_7, kVK_ANSI_Quote, "\'" },
	{ VK_OEM_102, 10, "<>" }, // This is not 100% accurate as this character is either "<>" or "\|" on RT 102-key kbd.
};

bool s_keysDown[256] = { false };

}


namespace KeyboardHelpers
{

const AppKey INVALID_APP_KEY = 0xffff;
const AppKey INVALID_PLATFORM_KEY = 0xffff;

CGKeyCode ApplyKeyboardLayout( CGKeyCode keyCode );
CGKeyCode UnapplyKeyboardLayout( CGKeyCode keyCode );
std::string CreateStringForKey( CGKeyCode keyCode );

PlatformKey AppKeyToPlatformKey( AppKey appCode )
{
	for( auto& desc : s_keyCodes )
	{
		if( desc.winCode == appCode )
		{
			return UnapplyKeyboardLayout( desc.macCode );
		}
	}
	return INVALID_PLATFORM_KEY;
}

AppKey PlatformKeyToAppKey( PlatformKey platformCode )
{
	platformCode = ApplyKeyboardLayout( platformCode );
	for( auto& desc : s_keyCodes )
	{
		if( desc.macCode == platformCode )
		{
			return desc.winCode;
		}
	}
	return INVALID_APP_KEY;
}

std::string GetAppKeyName( AppKey appCode )
{
	for( auto& desc : s_keyCodes )
	{
		if( desc.winCode == appCode )
		{
			auto str = CreateStringForKey( UnapplyKeyboardLayout( desc.macCode ) );
			if( !str.empty() )
			{
				return str;
			}
			return desc.text;
		}
	}
	return "";
}

bool IsAppKeyPressed( AppKey appCode )
{
	return IsPlatformKeyPressed( AppKeyToPlatformKey( appCode ) );
}

bool IsPlatformKeyPressed( PlatformKey platformCode )
{
	if( platformCode > 255 )
	{
		return false;
	}
	return s_keysDown[platformCode];
}

void PlatformKeyChanged( PlatformKey platformCode, bool pressed )
{
	if( platformCode > 255 )
	{
		return;
	}
	s_keysDown[platformCode] = pressed;
}

}

#elif defined( _WIN32 )


namespace KeyboardHelpers
{

const AppKey INVALID_APP_KEY = 0;
const AppKey INVALID_PLATFORM_KEY = 0;

PlatformKey AppKeyToPlatformKey( AppKey appCode )
{
	return appCode;
}

AppKey PlatformKeyToAppKey( PlatformKey platformCode )
{
	return platformCode;
}

std::string GetAppKeyName( AppKey keyCode )
{
	auto scanCode = ::MapVirtualKeyA( keyCode, MAPVK_VK_TO_VSC ) << 16;

	// Set the extended keyboard bit for the following virtual keys
	switch( keyCode )
	{
	case VK_LEFT:
	case VK_UP:
	case VK_RIGHT:
	case VK_DOWN:
	case VK_PRIOR:
	case VK_NEXT:
	case VK_END:
	case VK_HOME:
	case VK_INSERT:
	case VK_DELETE:
	case VK_DIVIDE:
	case VK_NUMLOCK: {
		scanCode |= ( 1 << 24 );
	}
	}

	// Make no distinction between left and right control keys
	scanCode |= ( 1 << 25 );

	const int bufferSize = 64;
	char buffer[bufferSize];

	// Lookup the key name and return true if it succeeds
	if( ::GetKeyNameTextA( scanCode, buffer, bufferSize ) )
	{
		return buffer;
	}
	else
	{
		return "";
	}
}

bool IsAppKeyPressed( AppKey appCode )
{
	return IsPlatformKeyPressed( appCode );
}

bool IsPlatformKeyPressed( PlatformKey platformCode )
{
	return ( GetKeyState( platformCode ) & 0x8000 ) ? 1 : 0;
}

void PlatformKeyChanged( PlatformKey, bool )
{
}

}

#elif defined( __linux__ ) && TRINITY_PLATFORM == TRINITY_VULKAN

// SDL3 (see UI/Tr2MainWindow_SDL.cpp): platform keys are SDL scancodes, which name physical key positions (USB HID
// usages); app keys are Win32 virtual-key codes, mapped by position on a US layout as the macOS table does. Key names
// come from SDL for the current layout; pressed state is SDL's keyboard state.
#include <SDL3/SDL.h>

namespace
{

struct KeyMapping
{
	SDL_Scancode scancode;
	KeyboardHelpers::AppKey vk;
};

const KeyMapping s_keyMap[] = {
	{ SDL_SCANCODE_A, VK_A },
	{ SDL_SCANCODE_B, VK_B },
	{ SDL_SCANCODE_C, VK_C },
	{ SDL_SCANCODE_D, VK_D },
	{ SDL_SCANCODE_E, VK_E },
	{ SDL_SCANCODE_F, VK_F },
	{ SDL_SCANCODE_G, VK_G },
	{ SDL_SCANCODE_H, VK_H },
	{ SDL_SCANCODE_I, VK_I },
	{ SDL_SCANCODE_J, VK_J },
	{ SDL_SCANCODE_K, VK_K },
	{ SDL_SCANCODE_L, VK_L },
	{ SDL_SCANCODE_M, VK_M },
	{ SDL_SCANCODE_N, VK_N },
	{ SDL_SCANCODE_O, VK_O },
	{ SDL_SCANCODE_P, VK_P },
	{ SDL_SCANCODE_Q, VK_Q },
	{ SDL_SCANCODE_R, VK_R },
	{ SDL_SCANCODE_S, VK_S },
	{ SDL_SCANCODE_T, VK_T },
	{ SDL_SCANCODE_U, VK_U },
	{ SDL_SCANCODE_V, VK_V },
	{ SDL_SCANCODE_W, VK_W },
	{ SDL_SCANCODE_X, VK_X },
	{ SDL_SCANCODE_Y, VK_Y },
	{ SDL_SCANCODE_Z, VK_Z },
	{ SDL_SCANCODE_1, VK_1 },
	{ SDL_SCANCODE_2, VK_2 },
	{ SDL_SCANCODE_3, VK_3 },
	{ SDL_SCANCODE_4, VK_4 },
	{ SDL_SCANCODE_5, VK_5 },
	{ SDL_SCANCODE_6, VK_6 },
	{ SDL_SCANCODE_7, VK_7 },
	{ SDL_SCANCODE_8, VK_8 },
	{ SDL_SCANCODE_9, VK_9 },
	{ SDL_SCANCODE_0, VK_0 },
	{ SDL_SCANCODE_RETURN, VK_RETURN },
	{ SDL_SCANCODE_ESCAPE, VK_ESCAPE },
	{ SDL_SCANCODE_BACKSPACE, VK_BACK },
	{ SDL_SCANCODE_TAB, VK_TAB },
	{ SDL_SCANCODE_SPACE, VK_SPACE },
	{ SDL_SCANCODE_MINUS, VK_OEM_MINUS },
	{ SDL_SCANCODE_EQUALS, VK_OEM_PLUS },
	{ SDL_SCANCODE_LEFTBRACKET, VK_OEM_4 },
	{ SDL_SCANCODE_RIGHTBRACKET, VK_OEM_6 },
	{ SDL_SCANCODE_BACKSLASH, VK_OEM_5 },
	{ SDL_SCANCODE_NONUSHASH, VK_OEM_5 },
	{ SDL_SCANCODE_SEMICOLON, VK_OEM_1 },
	{ SDL_SCANCODE_APOSTROPHE, VK_OEM_7 },
	{ SDL_SCANCODE_GRAVE, VK_OEM_3 },
	{ SDL_SCANCODE_COMMA, VK_OEM_COMMA },
	{ SDL_SCANCODE_PERIOD, VK_OEM_PERIOD },
	{ SDL_SCANCODE_SLASH, VK_OEM_2 },
	{ SDL_SCANCODE_NONUSBACKSLASH, VK_OEM_102 },
	{ SDL_SCANCODE_CAPSLOCK, VK_CAPITAL },
	{ SDL_SCANCODE_F1, VK_F1 },
	{ SDL_SCANCODE_F2, VK_F2 },
	{ SDL_SCANCODE_F3, VK_F3 },
	{ SDL_SCANCODE_F4, VK_F4 },
	{ SDL_SCANCODE_F5, VK_F5 },
	{ SDL_SCANCODE_F6, VK_F6 },
	{ SDL_SCANCODE_F7, VK_F7 },
	{ SDL_SCANCODE_F8, VK_F8 },
	{ SDL_SCANCODE_F9, VK_F9 },
	{ SDL_SCANCODE_F10, VK_F10 },
	{ SDL_SCANCODE_F11, VK_F11 },
	{ SDL_SCANCODE_F12, VK_F12 },
	{ SDL_SCANCODE_F13, VK_F13 },
	{ SDL_SCANCODE_F14, VK_F14 },
	{ SDL_SCANCODE_F15, VK_F15 },
	{ SDL_SCANCODE_F16, VK_F16 },
	{ SDL_SCANCODE_F17, VK_F17 },
	{ SDL_SCANCODE_F18, VK_F18 },
	{ SDL_SCANCODE_F19, VK_F19 },
	{ SDL_SCANCODE_F20, VK_F20 },
	{ SDL_SCANCODE_F21, VK_F21 },
	{ SDL_SCANCODE_F22, VK_F22 },
	{ SDL_SCANCODE_F23, VK_F23 },
	{ SDL_SCANCODE_F24, VK_F24 },
	{ SDL_SCANCODE_PRINTSCREEN, VK_SNAPSHOT },
	{ SDL_SCANCODE_SCROLLLOCK, VK_SCROLL },
	{ SDL_SCANCODE_PAUSE, VK_PAUSE },
	{ SDL_SCANCODE_INSERT, VK_INSERT },
	{ SDL_SCANCODE_HOME, VK_HOME },
	{ SDL_SCANCODE_PAGEUP, VK_PRIOR },
	{ SDL_SCANCODE_DELETE, VK_DELETE },
	{ SDL_SCANCODE_END, VK_END },
	{ SDL_SCANCODE_PAGEDOWN, VK_NEXT },
	{ SDL_SCANCODE_RIGHT, VK_RIGHT },
	{ SDL_SCANCODE_LEFT, VK_LEFT },
	{ SDL_SCANCODE_DOWN, VK_DOWN },
	{ SDL_SCANCODE_UP, VK_UP },
	{ SDL_SCANCODE_NUMLOCKCLEAR, VK_NUMLOCK },
	{ SDL_SCANCODE_KP_DIVIDE, VK_DIVIDE },
	{ SDL_SCANCODE_KP_MULTIPLY, VK_MULTIPLY },
	{ SDL_SCANCODE_KP_MINUS, VK_SUBTRACT },
	{ SDL_SCANCODE_KP_PLUS, VK_ADD },
	{ SDL_SCANCODE_KP_ENTER, VK_RETURN },
	{ SDL_SCANCODE_KP_1, VK_NUMPAD1 },
	{ SDL_SCANCODE_KP_2, VK_NUMPAD2 },
	{ SDL_SCANCODE_KP_3, VK_NUMPAD3 },
	{ SDL_SCANCODE_KP_4, VK_NUMPAD4 },
	{ SDL_SCANCODE_KP_5, VK_NUMPAD5 },
	{ SDL_SCANCODE_KP_6, VK_NUMPAD6 },
	{ SDL_SCANCODE_KP_7, VK_NUMPAD7 },
	{ SDL_SCANCODE_KP_8, VK_NUMPAD8 },
	{ SDL_SCANCODE_KP_9, VK_NUMPAD9 },
	{ SDL_SCANCODE_KP_0, VK_NUMPAD0 },
	{ SDL_SCANCODE_KP_PERIOD, VK_DECIMAL },
	{ SDL_SCANCODE_KP_COMMA, VK_SEPARATOR },
	{ SDL_SCANCODE_APPLICATION, VK_APPS },
	{ SDL_SCANCODE_HELP, VK_HELP },
	{ SDL_SCANCODE_SLEEP, VK_SLEEP },
	{ SDL_SCANCODE_MUTE, VK_VOLUME_MUTE },
	{ SDL_SCANCODE_VOLUMEUP, VK_VOLUME_UP },
	{ SDL_SCANCODE_VOLUMEDOWN, VK_VOLUME_DOWN },
	{ SDL_SCANCODE_MEDIA_NEXT_TRACK, VK_MEDIA_NEXT_TRACK },
	{ SDL_SCANCODE_MEDIA_PREVIOUS_TRACK, VK_MEDIA_PREV_TRACK },
	{ SDL_SCANCODE_MEDIA_STOP, VK_MEDIA_STOP },
	{ SDL_SCANCODE_MEDIA_PLAY_PAUSE, VK_MEDIA_PLAY_PAUSE },
	{ SDL_SCANCODE_AC_BACK, VK_BROWSER_BACK },
	{ SDL_SCANCODE_AC_FORWARD, VK_BROWSER_FORWARD },
	{ SDL_SCANCODE_AC_REFRESH, VK_BROWSER_REFRESH },
	{ SDL_SCANCODE_AC_STOP, VK_BROWSER_STOP },
	{ SDL_SCANCODE_AC_SEARCH, VK_BROWSER_SEARCH },
	{ SDL_SCANCODE_AC_BOOKMARKS, VK_BROWSER_FAVORITES },
	{ SDL_SCANCODE_AC_HOME, VK_BROWSER_HOME },
	{ SDL_SCANCODE_INTERNATIONAL1, VK_OEM_102 },
	{ SDL_SCANCODE_LANG1, VK_HANGUL },
	{ SDL_SCANCODE_LANG2, VK_HANJA },
	{ SDL_SCANCODE_LCTRL, VK_LCONTROL },
	{ SDL_SCANCODE_LSHIFT, VK_LSHIFT },
	{ SDL_SCANCODE_LALT, VK_LMENU },
	{ SDL_SCANCODE_LGUI, VK_LWIN },
	{ SDL_SCANCODE_RCTRL, VK_RCONTROL },
	{ SDL_SCANCODE_RSHIFT, VK_RSHIFT },
	{ SDL_SCANCODE_RALT, VK_RMENU },
	{ SDL_SCANCODE_RGUI, VK_RWIN },
};

// Scancode -> VK and back, built once. VK -> scancode keeps the first (main-block) key for duplicates such as Return.
struct KeyTables
{
	KeyboardHelpers::AppKey toApp[SDL_SCANCODE_COUNT] = {};
	SDL_Scancode toPlatform[256] = {};
	KeyTables()
	{
		for( auto& mapping : s_keyMap )
		{
			toApp[mapping.scancode] = mapping.vk;
			if( mapping.vk < 256 && toPlatform[mapping.vk] == SDL_SCANCODE_UNKNOWN )
			{
				toPlatform[mapping.vk] = mapping.scancode;
			}
		}
	}
};

const KeyTables& GetKeyTables()
{
	static const KeyTables tables;
	return tables;
}

bool IsScancodePressed( SDL_Scancode scancode )
{
	int count = 0;
	const bool* state = SDL_GetKeyboardState( &count );
	return state && int( scancode ) < count && state[scancode];
}

}

namespace KeyboardHelpers
{

const AppKey INVALID_APP_KEY = 0;
const AppKey INVALID_PLATFORM_KEY = 0;

PlatformKey AppKeyToPlatformKey( AppKey appCode )
{
	return appCode < 256 ? PlatformKey( GetKeyTables().toPlatform[appCode] ) : INVALID_PLATFORM_KEY;
}

AppKey PlatformKeyToAppKey( PlatformKey platformCode )
{
	return platformCode < SDL_SCANCODE_COUNT ? GetKeyTables().toApp[platformCode] : INVALID_APP_KEY;
}

// The key's label on the current keyboard layout (e.g. "Q" is "A" on AZERTY), as Windows' GetKeyNameText gives it.
std::string GetAppKeyName( AppKey appCode )
{
	switch( appCode )
	{
	case VK_SHIFT:
		appCode = VK_LSHIFT;
		break;
	case VK_CONTROL:
		appCode = VK_LCONTROL;
		break;
	case VK_MENU:
		appCode = VK_LMENU;
		break;
	}
	auto scancode = SDL_Scancode( AppKeyToPlatformKey( appCode ) );
	if( scancode == SDL_SCANCODE_UNKNOWN )
	{
		return "";
	}
	const char* name = SDL_GetKeyName( SDL_GetKeyFromScancode( scancode, SDL_KMOD_NONE, false ) );
	if( !name || !*name )
	{
		name = SDL_GetScancodeName( scancode );
	}
	return name ? name : "";
}

bool IsAppKeyPressed( AppKey appCode )
{
	switch( appCode )
	{
	case VK_SHIFT:
		return IsScancodePressed( SDL_SCANCODE_LSHIFT ) || IsScancodePressed( SDL_SCANCODE_RSHIFT );
	case VK_CONTROL:
		return IsScancodePressed( SDL_SCANCODE_LCTRL ) || IsScancodePressed( SDL_SCANCODE_RCTRL );
	case VK_MENU:
		return IsScancodePressed( SDL_SCANCODE_LALT ) || IsScancodePressed( SDL_SCANCODE_RALT );
	case VK_RETURN:
		return IsScancodePressed( SDL_SCANCODE_RETURN ) || IsScancodePressed( SDL_SCANCODE_KP_ENTER );
	case VK_LBUTTON:
		return ( SDL_GetMouseState( nullptr, nullptr ) & SDL_BUTTON_LMASK ) != 0;
	case VK_RBUTTON:
		return ( SDL_GetMouseState( nullptr, nullptr ) & SDL_BUTTON_RMASK ) != 0;
	case VK_MBUTTON:
		return ( SDL_GetMouseState( nullptr, nullptr ) & SDL_BUTTON_MMASK ) != 0;
	case VK_XBUTTON1:
		return ( SDL_GetMouseState( nullptr, nullptr ) & SDL_BUTTON_X1MASK ) != 0;
	case VK_XBUTTON2:
		return ( SDL_GetMouseState( nullptr, nullptr ) & SDL_BUTTON_X2MASK ) != 0;
	default:
		return IsPlatformKeyPressed( AppKeyToPlatformKey( appCode ) );
	}
}

bool IsPlatformKeyPressed( PlatformKey platformCode )
{
	return platformCode != INVALID_PLATFORM_KEY && IsScancodePressed( SDL_Scancode( platformCode ) );
}

void PlatformKeyChanged( PlatformKey, bool )
{
	// SDL tracks the keyboard state itself.
}

}

#else

// Stub renderer (no windows): platform and app keys are the same codes, key names are unknown,
// and pressed state is tracked from PlatformKeyChanged like the macOS branch does.
namespace
{
bool s_keysDown[256] = { false };
}

namespace KeyboardHelpers
{

const AppKey INVALID_APP_KEY = 0;
const AppKey INVALID_PLATFORM_KEY = 0;

PlatformKey AppKeyToPlatformKey( AppKey appCode )
{
	return appCode;
}

AppKey PlatformKeyToAppKey( PlatformKey platformCode )
{
	return platformCode;
}

std::string GetAppKeyName( AppKey )
{
	return "";
}

bool IsAppKeyPressed( AppKey appCode )
{
	return IsPlatformKeyPressed( appCode );
}

bool IsPlatformKeyPressed( PlatformKey platformCode )
{
	return platformCode < 256 && s_keysDown[platformCode];
}

void PlatformKeyChanged( PlatformKey platformCode, bool pressed )
{
	if( platformCode < 256 )
	{
		s_keysDown[platformCode] = pressed;
	}
}

}
#endif

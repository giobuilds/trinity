// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "VulkanWindowSystem.h"
#include "ALLog.h"

#include <SDL3/SDL.h>

#include <cstdlib>

namespace TrinityALImpl
{

bool InitializeWindowSystem()
{
	static bool attempted = false;
	static bool initialized = false;
	if( attempted )
	{
		return initialized;
	}
	attempted = true;

	SDL_SetHint( SDL_HINT_NO_SIGNAL_HANDLERS, "1" ); // blue handles SIGINT/SIGTERM
	SDL_SetHint( SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, "0" ); // the main window's onClose decides
	SDL_SetAppMetadata( "Carbon", nullptr, "com.fenriscreations.carbon" );
	auto isSet = []( const char* name ) {
		const char* value = getenv( name );
		return value && *value;
	};
	const bool desktop = isSet( "DISPLAY" ) || isSet( "WAYLAND_DISPLAY" );
	const bool driverChosen = isSet( "SDL_VIDEO_DRIVER" );
	if( !driverChosen && !desktop )
	{
		SDL_SetHint( SDL_HINT_VIDEO_DRIVER, "offscreen" );
	}
	initialized = SDL_InitSubSystem( SDL_INIT_VIDEO );
	if( !initialized && !driverChosen && desktop )
	{
		CCP_AL_LOGWARN( "SDL video failed on the desktop (%s); falling back to offscreen windows", SDL_GetError() );
		SDL_SetHint( SDL_HINT_VIDEO_DRIVER, "offscreen" );
		initialized = SDL_InitSubSystem( SDL_INIT_VIDEO );
	}
	if( initialized )
	{
		CCP_AL_LOG( "SDL %d.%d.%d video driver: %s", SDL_VERSIONNUM_MAJOR( SDL_GetVersion() ), SDL_VERSIONNUM_MINOR( SDL_GetVersion() ), SDL_VERSIONNUM_MICRO( SDL_GetVersion() ), SDL_GetCurrentVideoDriver() );
	}
	else
	{
		CCP_AL_LOGERR( "SDL video could not start: %s", SDL_GetError() );
	}
	return initialized;
}

uint32_t GetAdapterDisplay( uint32_t )
{
	if( !InitializeWindowSystem() )
	{
		return 0;
	}
	return SDL_GetPrimaryDisplay();
}

}

#endif

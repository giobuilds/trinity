// Copyright © 2023 CCP ehf.

#include "StdAfx.h"

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "Tr2VideoAdapterInfoALVulkan.h"
#include "Tr2AdapterStructures.h"
#include "VulkanDevice.h"
#include "VulkanFormats.h"
#include "VulkanWindowSystem.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <vector>

using namespace Tr2RenderContextEnum;
using TrinityALImpl::VulkanInstance;

namespace
{

const TrinityALImpl::VulkanAdapter* GetAdapter( unsigned index )
{
	VulkanInstance* instance = VulkanInstance::Get();
	if( !instance || index >= instance->GetAdapters().size() )
	{
		return nullptr;
	}
	return &instance->GetAdapters()[index];
}

bool SupportsFormatFeatures( unsigned adapterIndex, PixelFormat format, VkFormatFeatureFlags features )
{
	auto adapter = GetAdapter( adapterIndex );
	VkFormat vkFormat = TrinityALImpl::ToVkFormat( format );
	if( !adapter || vkFormat == VK_FORMAT_UNDEFINED )
	{
		return false;
	}
	VkFormatProperties properties;
	vkGetPhysicalDeviceFormatProperties( adapter->physicalDevice, vkFormat, &properties );
	return ( properties.optimalTilingFeatures & features ) == features;
}

}

ALResult Tr2VideoAdapterInfo::GetAdapterCount( unsigned& count )
{
	VulkanInstance* instance = VulkanInstance::Get();
	count = instance ? unsigned( instance->GetAdapters().size() ) : 0;
	return S_OK;
}

ALResult Tr2VideoAdapterInfo::GetAdapterInfo( unsigned adapterIndex, Tr2AdapterInfo& info )
{
	if( !GetAdapter( adapterIndex ) )
	{
		return E_INVALIDARG;
	}
	VulkanInstance::Get()->FillAdapterInfo( adapterIndex, info );
	return S_OK;
}

ALResult Tr2VideoAdapterInfo::GetAdapterMonitor( unsigned adapterIndex, void*& monitor )
{
	// The SDL_DisplayID of the display the adapter shows on (see GetAdapterDisplay).
	monitor = reinterpret_cast<void*>( uintptr_t( TrinityALImpl::GetAdapterDisplay( adapterIndex ) ) );
	return GetAdapter( adapterIndex ) ? S_OK : E_INVALIDARG;
}

namespace
{

void FillModeInfo( const SDL_DisplayMode& sdlMode, Tr2DisplayModeInfo& mode )
{
	// SDL3 mode sizes are in points; pixel_density converts them to pixels.
	const float density = sdlMode.pixel_density > 0 ? sdlMode.pixel_density : 1.0f;
	mode.format = PIXEL_FORMAT_B8G8R8A8_UNORM;
	mode.width = uint32_t( sdlMode.w * density + 0.5f );
	mode.height = uint32_t( sdlMode.h * density + 0.5f );
	mode.refreshRateNumerator = sdlMode.refresh_rate_numerator ? uint32_t( sdlMode.refresh_rate_numerator ) : uint32_t( sdlMode.refresh_rate + 0.5f );
	mode.refreshRateDenominator = sdlMode.refresh_rate_numerator ? uint32_t( sdlMode.refresh_rate_denominator ) : 1;
	mode.scaling = DISPLAY_SCALING_UNSPECIFIED;
	mode.scanlineOrdering = SCANLINE_ORDER_UNSPECIFIED;
}

// Fullscreen is borderless on the desktop (the back buffer is scaled to the display when presenting), so the modes are
// the display's desktop mode plus the sizes SDL lists for it, largest first, without duplicate sizes.
std::vector<Tr2DisplayModeInfo> GetDisplayModes( unsigned adapterIndex )
{
	std::vector<Tr2DisplayModeInfo> modes;
	SDL_DisplayID display = TrinityALImpl::GetAdapterDisplay( adapterIndex );
	if( !display )
	{
		return modes;
	}
	if( const SDL_DisplayMode* desktop = SDL_GetDesktopDisplayMode( display ) )
	{
		Tr2DisplayModeInfo mode;
		FillModeInfo( *desktop, mode );
		modes.push_back( mode );
	}
	int count = 0;
	if( SDL_DisplayMode** list = SDL_GetFullscreenDisplayModes( display, &count ) )
	{
		for( int i = 0; i < count; ++i )
		{
			Tr2DisplayModeInfo mode;
			FillModeInfo( *list[i], mode );
			bool duplicate = false;
			for( auto& existing : modes )
			{
				duplicate |= existing.width == mode.width && existing.height == mode.height;
			}
			if( !duplicate )
			{
				modes.push_back( mode );
			}
		}
		SDL_free( list );
	}
	return modes;
}

}

// The desktop mode of the adapter's display; 1920x1080 at 60 Hz when there is no window system at all.
ALResult Tr2VideoAdapterInfo::GetAdapterDisplayMode( unsigned adapterIndex, Tr2DisplayModeInfo& mode )
{
	if( !GetAdapter( adapterIndex ) )
	{
		return E_INVALIDARG;
	}
	SDL_DisplayID display = TrinityALImpl::GetAdapterDisplay( adapterIndex );
	if( const SDL_DisplayMode* desktop = display ? SDL_GetDesktopDisplayMode( display ) : nullptr )
	{
		FillModeInfo( *desktop, mode );
		return S_OK;
	}
	mode.format = PIXEL_FORMAT_B8G8R8A8_UNORM;
	mode.width = 1920;
	mode.height = 1080;
	mode.refreshRateNumerator = 60;
	mode.refreshRateDenominator = 1;
	mode.scaling = DISPLAY_SCALING_UNSPECIFIED;
	mode.scanlineOrdering = SCANLINE_ORDER_UNSPECIFIED;
	return S_OK;
}

ALResult Tr2VideoAdapterInfo::GetAdapterModeCount( unsigned adapterIndex, Tr2RenderContextEnum::PixelFormat backBufferFormat, unsigned& count )
{
	count = 0;
	if( !GetAdapter( adapterIndex ) )
	{
		return E_INVALIDARG;
	}
	if( SupportsBackBufferFormat( adapterIndex, backBufferFormat ) )
	{
		count = std::max( unsigned( GetDisplayModes( adapterIndex ).size() ), 1u );
	}
	return S_OK;
}

ALResult Tr2VideoAdapterInfo::GetAdapterMode( unsigned adapterIndex, Tr2RenderContextEnum::PixelFormat backBufferFormat, unsigned modeIndex, Tr2DisplayModeInfo& mode )
{
	if( !SupportsBackBufferFormat( adapterIndex, backBufferFormat ) )
	{
		return E_INVALIDARG;
	}
	auto modes = GetDisplayModes( adapterIndex );
	if( modes.empty() && modeIndex == 0 )
	{
		CR_RETURN_HR( GetAdapterDisplayMode( adapterIndex, mode ) );
	}
	else if( modeIndex < modes.size() )
	{
		mode = modes[modeIndex];
	}
	else
	{
		return E_INVALIDARG;
	}
	mode.format = backBufferFormat;
	return S_OK;
}

ALResult Tr2VideoAdapterInfo::GetAdapterMaxTextureWidth( unsigned adapterIndex, unsigned& maxWidth )
{
	auto adapter = GetAdapter( adapterIndex );
	if( !adapter )
	{
		return E_INVALIDARG;
	}
	maxWidth = adapter->properties.limits.maxImageDimension2D;
	return S_OK;
}

bool Tr2VideoAdapterInfo::SupportsBackBufferFormat( unsigned adapterIndex, Tr2RenderContextEnum::PixelFormat backBufferFormat )
{
	// Presentation support is a surface query (phase 3); here: usable as a blendable colour attachment.
	return SupportsFormatFeatures( adapterIndex, backBufferFormat, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT );
}

bool Tr2VideoAdapterInfo::SupportsRenderTargetFormat( unsigned adapterIndex, Tr2RenderContextEnum::PixelFormat format )
{
	return SupportsFormatFeatures( adapterIndex, format, TrinityALImpl::IsDepthFormat( format ) ? VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT );
}

bool Tr2VideoAdapterInfo::AreAdaptersDifferent( unsigned adapter1, unsigned adapter2 )
{
	auto a = GetAdapter( adapter1 );
	auto b = GetAdapter( adapter2 );
	if( !a || !b )
	{
		return adapter1 != adapter2;
	}
	return memcmp( a->idProperties.deviceUUID, b->idProperties.deviceUUID, VK_UUID_SIZE ) != 0;
}

ALResult Tr2VideoAdapterInfo::RefreshData()
{
	return S_OK;
}


#endif

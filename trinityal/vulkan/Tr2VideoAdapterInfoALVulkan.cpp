// Copyright © 2023 CCP ehf.

#include "StdAfx.h"

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "Tr2VideoAdapterInfoALVulkan.h"
#include "Tr2AdapterStructures.h"
#include "VulkanDevice.h"
#include "VulkanFormats.h"

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
	// Monitors come from the window system (SDL3, phase 3); Vulkan adapters are not tied to one.
	monitor = nullptr;
	return GetAdapter( adapterIndex ) ? S_OK : E_INVALIDARG;
}

// Display modes need the window system (SDL3 display enumeration, phase 3). Until then every adapter reports one
// 1920x1080 desktop mode.
ALResult Tr2VideoAdapterInfo::GetAdapterDisplayMode( unsigned adapterIndex, Tr2DisplayModeInfo& mode )
{
	if( !GetAdapter( adapterIndex ) )
	{
		return E_INVALIDARG;
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
	count = SupportsBackBufferFormat( adapterIndex, backBufferFormat ) ? 1 : 0;
	return GetAdapter( adapterIndex ) ? S_OK : E_INVALIDARG;
}

ALResult Tr2VideoAdapterInfo::GetAdapterMode( unsigned adapterIndex, Tr2RenderContextEnum::PixelFormat backBufferFormat, unsigned modeIndex, Tr2DisplayModeInfo& mode )
{
	if( modeIndex != 0 || !SupportsBackBufferFormat( adapterIndex, backBufferFormat ) )
	{
		return E_INVALIDARG;
	}
	CR_RETURN_HR( GetAdapterDisplayMode( adapterIndex, mode ) );
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

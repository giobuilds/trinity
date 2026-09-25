// Copyright © 2023 CCP ehf.

#include "StdAfx.h"

#if TRINITY_PLATFORM == TRINITY_VULKAN

#include "Tr2SwapChainALVulkan.h"
#include "Tr2RenderContextVulkan.h"
#include "Tr2TextureALVulkan.h"
#include "VulkanSwapchain.h"
#include "ALLog.h"

namespace TrinityALImpl
{

Tr2SwapChainAL::Tr2SwapChainAL()
{
}

Tr2SwapChainAL::~Tr2SwapChainAL()
{
	Destroy();
}

ALResult Tr2SwapChainAL::Create( Tr2WindowHandle windowHandle, Tr2RenderContextAL& renderContext )
{
	if( !renderContext.IsValid() )
	{
		return E_INVALIDARG;
	}
	Destroy();
	auto swapchain = std::make_unique<VulkanSwapchain>();
	// Extra windows present without waiting for vblank so they do not throttle the main window.
	if( !swapchain->Create( renderContext.GetVulkanDeviceShared(), windowHandle, false ) )
	{
		return E_FAIL;
	}
	uint32_t width = 0, height = 0;
	swapchain->GetWindowPixelSize( width, height );
	CR_RETURN_HR( m_backBuffer.Create(
		Tr2BitmapDimensions( std::max( width, 1u ), std::max( height, 1u ), 1, Tr2RenderContextEnum::PIXEL_FORMAT_B8G8R8A8_UNORM ),
		Tr2GpuUsage::RENDER_TARGET | Tr2GpuUsage::SHADER_RESOURCE,
		renderContext ) );
	m_swapchain = std::move( swapchain );
	return S_OK;
}

void Tr2SwapChainAL::Destroy()
{
	m_swapchain.reset();
	m_backBuffer = ::Tr2TextureAL();
}

bool Tr2SwapChainAL::IsValid() const
{
	return m_swapchain && m_backBuffer.IsValid();
}

ALResult Tr2SwapChainAL::Present( Tr2RenderContextAL& )
{
	if( !IsValid() )
	{
		return E_INVALIDCALL;
	}
	return m_swapchain->Present( *m_backBuffer.TrinityALImpl_GetObject() ) ? S_OK : E_FAIL;
}

uint32_t Tr2SwapChainAL::GetWidth() const
{
	return m_backBuffer.GetWidth();
}

uint32_t Tr2SwapChainAL::GetHeight() const
{
	return m_backBuffer.GetHeight();
}

void Tr2SwapChainAL::Describe( Tr2DeviceResourceDescriptionAL& ) const
{
}

ALResult Tr2SwapChainAL::SetName( const char* name )
{
	m_name = name ? name : "";
	return S_OK;
}
}

#endif // TRINITY_PLATFORM==TRINITY_VULKAN

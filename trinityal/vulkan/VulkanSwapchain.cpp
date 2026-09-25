// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "VulkanSwapchain.h"
#include "VulkanDevice.h"
#include "Tr2TextureALVulkan.h"
#include "ALLog.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <algorithm>

namespace
{

SDL_Window* ToSdlWindow( Tr2WindowHandle window )
{
	return reinterpret_cast<SDL_Window*>( window );
}

}

namespace TrinityALImpl
{

VkSemaphore VulkanSwapchain::SemaphorePool::Get()
{
	if( !free.empty() )
	{
		VkSemaphore semaphore = free.back();
		free.pop_back();
		return semaphore;
	}
	VkSemaphoreCreateInfo info{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	VkSemaphore semaphore = VK_NULL_HANDLE;
	vkCreateSemaphore( device, &info, nullptr, &semaphore );
	return semaphore;
}

void VulkanSwapchain::SemaphorePool::Put( VkSemaphore semaphore )
{
	if( closed )
	{
		vkDestroySemaphore( device, semaphore, nullptr );
	}
	else
	{
		free.push_back( semaphore );
	}
}

VulkanSwapchain::~VulkanSwapchain()
{
	Destroy();
}

bool VulkanSwapchain::Create( const std::shared_ptr<VulkanDevice>& device, Tr2WindowHandle window, bool vsync )
{
	Destroy();
	if( !window || !device->SupportsSwapchains() )
	{
		return false;
	}
	SDL_Window* sdlWindow = ToSdlWindow( window );
	if( !( SDL_GetWindowFlags( sdlWindow ) & SDL_WINDOW_VULKAN ) )
	{
		CCP_AL_LOGERR( "Vulkan: the window was not created with SDL_WINDOW_VULKAN" );
		return false;
	}
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	if( !SDL_Vulkan_CreateSurface( sdlWindow, VulkanInstance::Get()->GetHandle(), nullptr, &surface ) )
	{
		CCP_AL_LOGERR( "Vulkan: SDL_Vulkan_CreateSurface failed: %s", SDL_GetError() );
		return false;
	}
	VkBool32 supported = VK_FALSE;
	vkGetPhysicalDeviceSurfaceSupportKHR( device->GetPhysicalDevice(), device->GetQueueFamily(), surface, &supported );
	if( !supported )
	{
		CCP_AL_LOGERR( "Vulkan: %s cannot present to this window", device->GetAdapter().properties.deviceName );
		vkDestroySurfaceKHR( VulkanInstance::Get()->GetHandle(), surface, nullptr );
		return false;
	}
	m_device = device;
	m_window = window;
	m_surface = surface;
	m_vsync = vsync;
	m_acquireSemaphores = std::make_shared<SemaphorePool>();
	m_acquireSemaphores->device = device->GetHandle();
	if( !CreateSwapchain() )
	{
		Destroy();
		return false;
	}
	return true;
}

void VulkanSwapchain::Destroy()
{
	if( m_device )
	{
		// The presentation engine may still hold images and semaphores; nothing may use them once the queue is idle.
		m_device->Submit();
		m_device->WaitIdle();
		DestroySwapchain();
		for( auto semaphore : m_acquireSemaphores->free )
		{
			vkDestroySemaphore( m_device->GetHandle(), semaphore, nullptr );
		}
		m_acquireSemaphores->free.clear();
		m_acquireSemaphores->closed = true;
		if( m_surface )
		{
			vkDestroySurfaceKHR( VulkanInstance::Get()->GetHandle(), m_surface, nullptr );
		}
	}
	m_surface = VK_NULL_HANDLE;
	m_acquireSemaphores.reset();
	m_device.reset();
	m_window = 0;
}

void VulkanSwapchain::SetVsync( bool vsync )
{
	if( vsync != m_vsync )
	{
		m_vsync = vsync;
		m_outOfDate = true; // the present mode is fixed per swapchain
	}
}

bool VulkanSwapchain::GetWindowPixelSize( uint32_t& width, uint32_t& height ) const
{
	int w = 0, h = 0;
	if( !m_window || !SDL_GetWindowSizeInPixels( ToSdlWindow( m_window ), &w, &h ) )
	{
		return false;
	}
	width = uint32_t( std::max( w, 0 ) );
	height = uint32_t( std::max( h, 0 ) );
	return true;
}

void VulkanSwapchain::DestroySwapchain()
{
	VkDevice device = m_device->GetHandle();
	for( auto semaphore : m_renderFinished )
	{
		vkDestroySemaphore( device, semaphore, nullptr );
	}
	m_renderFinished.clear();
	m_images.clear();
	if( m_swapchain )
	{
		vkDestroySwapchainKHR( device, m_swapchain, nullptr );
	}
	m_swapchain = VK_NULL_HANDLE;
}

bool VulkanSwapchain::CreateSwapchain()
{
	VkPhysicalDevice physicalDevice = m_device->GetPhysicalDevice();
	VkSurfaceCapabilitiesKHR capabilities;
	if( vkGetPhysicalDeviceSurfaceCapabilitiesKHR( physicalDevice, m_surface, &capabilities ) != VK_SUCCESS )
	{
		return false;
	}
	VkExtent2D extent = capabilities.currentExtent;
	if( extent.width == 0xffffffff )
	{
		// The surface takes its size from the swapchain (Wayland): use the window's drawable size.
		GetWindowPixelSize( extent.width, extent.height );
		extent.width = std::clamp( extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width );
		extent.height = std::clamp( extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height );
	}

	// Recreating: the old swapchain's images may still be in use.
	VkSwapchainKHR oldSwapchain = m_swapchain;
	if( oldSwapchain )
	{
		m_device->Submit();
		m_device->WaitIdle();
	}
	for( auto semaphore : m_renderFinished )
	{
		vkDestroySemaphore( m_device->GetHandle(), semaphore, nullptr );
	}
	m_renderFinished.clear();
	m_images.clear();
	m_swapchain = VK_NULL_HANDLE;
	m_extent = extent;
	m_outOfDate = false;
	if( extent.width == 0 || extent.height == 0 )
	{
		// Minimised: no swapchain until the window has a size again.
		if( oldSwapchain )
		{
			vkDestroySwapchainKHR( m_device->GetHandle(), oldSwapchain, nullptr );
		}
		return true;
	}

	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR( physicalDevice, m_surface, &formatCount, nullptr );
	std::vector<VkSurfaceFormatKHR> formats( formatCount );
	vkGetPhysicalDeviceSurfaceFormatsKHR( physicalDevice, m_surface, &formatCount, formats.data() );
	if( formats.empty() )
	{
		return false;
	}
	// SDR: the back buffer holds sRGB-encoded values in a UNORM format, as D3D's does, so the swapchain is UNORM too.
	VkSurfaceFormatKHR format = formats[0];
	for( VkFormat preferred : { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM } )
	{
		auto found = std::find_if( formats.begin(), formats.end(), [preferred]( const VkSurfaceFormatKHR& f ) { return f.format == preferred && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; } );
		if( found != formats.end() )
		{
			format = *found;
			break;
		}
	}

	uint32_t modeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR( physicalDevice, m_surface, &modeCount, nullptr );
	std::vector<VkPresentModeKHR> modes( modeCount );
	vkGetPhysicalDeviceSurfacePresentModesKHR( physicalDevice, m_surface, &modeCount, modes.data() );
	m_presentMode = VK_PRESENT_MODE_FIFO_KHR;
	if( !m_vsync )
	{
		for( VkPresentModeKHR preferred : { VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_MAILBOX_KHR } )
		{
			if( std::find( modes.begin(), modes.end(), preferred ) != modes.end() )
			{
				m_presentMode = preferred;
				break;
			}
		}
	}

	VkSwapchainCreateInfoKHR info{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	info.surface = m_surface;
	info.minImageCount = capabilities.minImageCount + 1;
	if( capabilities.maxImageCount )
	{
		info.minImageCount = std::min( info.minImageCount, capabilities.maxImageCount );
	}
	info.imageFormat = format.format;
	info.imageColorSpace = format.colorSpace;
	info.imageExtent = extent;
	info.imageArrayLayers = 1;
	info.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | ( capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT );
	info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.preTransform = capabilities.currentTransform;
	info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	if( !( capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR ) )
	{
		for( VkCompositeAlphaFlagBitsKHR alpha : { VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR } )
		{
			if( capabilities.supportedCompositeAlpha & alpha )
			{
				info.compositeAlpha = alpha;
				break;
			}
		}
	}
	info.presentMode = m_presentMode;
	info.clipped = VK_TRUE;
	info.oldSwapchain = oldSwapchain;
	VkResult result = vkCreateSwapchainKHR( m_device->GetHandle(), &info, nullptr, &m_swapchain );
	if( oldSwapchain )
	{
		vkDestroySwapchainKHR( m_device->GetHandle(), oldSwapchain, nullptr );
	}
	if( result != VK_SUCCESS )
	{
		CCP_AL_LOGERR( "Vulkan: vkCreateSwapchainKHR (%ux%u) failed: %s", extent.width, extent.height, VkResultToString( result ) );
		m_swapchain = VK_NULL_HANDLE;
		return false;
	}
	m_format = format.format;
	if( const char* verbose = getenv( "CARBON_VULKAN_VERBOSE" ); verbose && atoi( verbose ) )
	{
		fprintf( stderr, "Vulkan: swapchain %ux%u, format %d, present mode %d, %u images (min %u), SDL video driver %s\n", extent.width, extent.height, int( format.format ), int( m_presentMode ), info.minImageCount, capabilities.minImageCount, SDL_GetCurrentVideoDriver() );
	}

	uint32_t imageCount = 0;
	vkGetSwapchainImagesKHR( m_device->GetHandle(), m_swapchain, &imageCount, nullptr );
	m_images.resize( imageCount );
	vkGetSwapchainImagesKHR( m_device->GetHandle(), m_swapchain, &imageCount, m_images.data() );
	for( uint32_t i = 0; i < imageCount; ++i )
	{
		VkSemaphoreCreateInfo semaphoreInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		VkSemaphore semaphore = VK_NULL_HANDLE;
		vkCreateSemaphore( m_device->GetHandle(), &semaphoreInfo, nullptr, &semaphore );
		m_renderFinished.push_back( semaphore );
	}
	return true;
}

bool VulkanSwapchain::Present( Tr2TextureAL& source )
{
	if( !m_surface )
	{
		return false;
	}
	// Not every surface reports a resize as out of date (X11, headless), so compare with the window as well.
	uint32_t width = 0, height = 0;
	if( GetWindowPixelSize( width, height ) && ( width != m_extent.width || height != m_extent.height ) )
	{
		m_outOfDate = true;
	}
	if( m_outOfDate || !m_swapchain )
	{
		if( !CreateSwapchain() )
		{
			return false;
		}
	}
	if( !m_swapchain )
	{
		return m_device->Submit() == VK_SUCCESS; // minimised
	}

	VkSemaphore acquired = m_acquireSemaphores->Get();
	uint32_t index = 0;
	VkResult result = vkAcquireNextImageKHR( m_device->GetHandle(), m_swapchain, UINT64_MAX, acquired, VK_NULL_HANDLE, &index );
	if( result == VK_ERROR_OUT_OF_DATE_KHR )
	{
		m_acquireSemaphores->Put( acquired );
		if( !CreateSwapchain() )
		{
			return false;
		}
		if( !m_swapchain )
		{
			return m_device->Submit() == VK_SUCCESS;
		}
		acquired = m_acquireSemaphores->Get();
		result = vkAcquireNextImageKHR( m_device->GetHandle(), m_swapchain, UINT64_MAX, acquired, VK_NULL_HANDLE, &index );
	}
	if( result == VK_SUBOPTIMAL_KHR )
	{
		m_outOfDate = true; // still presentable; recreate before the next frame
	}
	else if( result != VK_SUCCESS )
	{
		CCP_AL_LOGERR( "Vulkan: vkAcquireNextImageKHR failed: %s", VkResultToString( result ) );
		m_acquireSemaphores->Put( acquired );
		return false;
	}

	// Blit the back buffer into the image: everything before it is finished writing (full barrier), the image goes
	// UNDEFINED -> TRANSFER_DST -> PRESENT_SRC.
	m_device->RecordFullBarrier();
	VkCommandBuffer commandBuffer = m_device->GetCommandBuffer();
	VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
	barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT; // after the acquire semaphore's wait
	barrier.srcAccessMask = VK_ACCESS_2_NONE;
	barrier.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
	barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = m_images[index];
	barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	dependency.imageMemoryBarrierCount = 1;
	dependency.pImageMemoryBarriers = &barrier;
	vkCmdPipelineBarrier2( commandBuffer, &dependency );

	const auto& desc = source.GetDesc();
	VkImageBlit region{};
	region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	region.srcOffsets[1] = { int32_t( desc.GetWidth() ), int32_t( desc.GetHeight() ), 1 };
	region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	region.dstOffsets[1] = { int32_t( m_extent.width ), int32_t( m_extent.height ), 1 };
	const bool sameSize = desc.GetWidth() == m_extent.width && desc.GetHeight() == m_extent.height;
	vkCmdBlitImage( commandBuffer, source.GetVkImage(), VK_IMAGE_LAYOUT_GENERAL, m_images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, sameSize ? VK_FILTER_NEAREST : VK_FILTER_LINEAR );

	barrier.srcStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
	barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE; // the present waits on the semaphore
	barrier.dstAccessMask = VK_ACCESS_2_NONE;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	vkCmdPipelineBarrier2( commandBuffer, &dependency );

	m_device->AddSubmitWait( acquired, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT );
	m_device->AddSubmitSignal( m_renderFinished[index] );
	if( m_device->Submit() != VK_SUCCESS )
	{
		return false;
	}
	// The acquire semaphore is free again once that submission has completed.
	auto pool = m_acquireSemaphores;
	m_device->ReleaseLater( [pool, acquired] { pool->Put( acquired ); } );

	VkPresentInfoKHR present{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	present.waitSemaphoreCount = 1;
	present.pWaitSemaphores = &m_renderFinished[index];
	present.swapchainCount = 1;
	present.pSwapchains = &m_swapchain;
	present.pImageIndices = &index;
	result = vkQueuePresentKHR( m_device->GetQueue(), &present );
	if( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR )
	{
		m_outOfDate = true;
	}
	else if( result != VK_SUCCESS )
	{
		CCP_AL_LOGERR( "Vulkan: vkQueuePresentKHR failed: %s", VkResultToString( result ) );
		return false;
	}
	return true;
}

}

#endif

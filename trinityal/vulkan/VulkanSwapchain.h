// Copyright © 2026 CCP ehf.

#pragma once

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "VulkanIncludes.h"

#include <memory>
#include <vector>

namespace TrinityALImpl
{
class VulkanDevice;
class Tr2TextureAL;

// A window's VkSurfaceKHR and swapchain. Trinity renders into its own back-buffer texture (stable for the engine and
// readable for screenshots); Present blits that texture into the next swapchain image and queues it for display, so
// format differences and a window caught mid-resize are handled by the blit.
//
// Tr2WindowHandle on Linux is the SDL_Window* (created with SDL_WINDOW_VULKAN) cast to an integer.
class VulkanSwapchain
{
public:
	~VulkanSwapchain();

	// vsync: FIFO; otherwise IMMEDIATE, else MAILBOX, else FIFO.
	bool Create( const std::shared_ptr<VulkanDevice>& device, Tr2WindowHandle window, bool vsync );
	void Destroy();
	bool IsValid() const
	{
		return m_surface != VK_NULL_HANDLE;
	}

	void SetVsync( bool vsync );
	Tr2WindowHandle GetWindow() const
	{
		return m_window;
	}
	// The window's drawable size in pixels (what a back buffer for it should be).
	bool GetWindowPixelSize( uint32_t& width, uint32_t& height ) const;

	// Submits the recorded work and presents source (mip 0, layer 0). A minimised window (zero extent) skips the
	// presentation but still submits. False if the device or surface failed.
	bool Present( Tr2TextureAL& source );

private:
	bool CreateSwapchain();
	void DestroySwapchain();

	struct SemaphorePool
	{
		VkDevice device = VK_NULL_HANDLE;
		std::vector<VkSemaphore> free;
		bool closed = false;
		VkSemaphore Get();
		void Put( VkSemaphore semaphore );
	};

	std::shared_ptr<VulkanDevice> m_device;
	Tr2WindowHandle m_window = 0;
	VkSurfaceKHR m_surface = VK_NULL_HANDLE;
	VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
	VkFormat m_format = VK_FORMAT_UNDEFINED;
	VkExtent2D m_extent{ 0, 0 };
	VkPresentModeKHR m_presentMode = VK_PRESENT_MODE_FIFO_KHR;
	bool m_vsync = true;
	bool m_outOfDate = false;
	std::vector<VkImage> m_images;
	std::vector<VkSemaphore> m_renderFinished; // per image: signalled by the submit, waited on by the present
	std::shared_ptr<SemaphorePool> m_acquireSemaphores;
};

}

#endif

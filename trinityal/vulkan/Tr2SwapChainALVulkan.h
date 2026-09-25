// Copyright © 2023 CCP ehf.

#pragma once

#if TRINITY_PLATFORM == TRINITY_VULKAN

#include "../include/Tr2SwapChainAL.h"
#include "../include/Tr2TextureAL.h"

#include <memory>
#include <string>

namespace TrinityALImpl
{
class VulkanSwapchain;

// A swapchain for an extra window: a back buffer sized to the window when created, blitted to the window on Present.
class Tr2SwapChainAL : public Tr2DeviceResourceAL<Tr2SwapChainAL>
{
public:
	Tr2SwapChainAL();
	~Tr2SwapChainAL();
	ALResult Create( Tr2WindowHandle windowHandle, Tr2RenderContextAL& renderContext );
	void Destroy();

	bool IsValid() const;

	ALResult Present( Tr2RenderContextAL& renderContext );

	uint32_t GetWidth() const;
	uint32_t GetHeight() const;

	Tr2ALMemoryType GetMemoryClass() const
	{
		return AL_MEMORY_VIDEO;
	}

	void Describe( Tr2DeviceResourceDescriptionAL& description ) const;
	ALResult SetName( const char* name );

	::Tr2TextureAL m_backBuffer;

private:
	std::unique_ptr<VulkanSwapchain> m_swapchain;
	std::string m_name;
};

}

#endif

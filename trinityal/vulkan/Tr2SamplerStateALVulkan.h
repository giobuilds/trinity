// Copyright © 2023 CCP ehf.

#pragma once

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "../include/Tr2SamplerStateAL.h"
#include "VulkanIncludes.h"

#include <memory>

namespace TrinityALImpl
{
class VulkanDevice;

class Tr2SamplerStateAL : public Tr2DeviceResourceAL<Tr2SamplerStateAL>
{
public:
	Tr2SamplerStateAL();
	~Tr2SamplerStateAL();

	ALResult Create( const Tr2SamplerDescription& description, Tr2RenderContextAL& renderContext );
	void Destroy();

	uint32_t GetIndexInHeap() const;

	bool IsValid() const;

	Tr2ALMemoryType GetMemoryClass() const
	{
		return AL_MEMORY_MANAGED;
	}

	void Describe( Tr2DeviceResourceDescriptionAL& description ) const;
	ALResult SetName( const char* name );

	VkSampler GetVkSampler() const
	{
		return m_sampler;
	}

private:
	std::shared_ptr<VulkanDevice> m_device;
	VkSampler m_sampler = VK_NULL_HANDLE;
};
}

#endif

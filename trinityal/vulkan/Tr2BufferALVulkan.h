// Copyright © 2023 CCP ehf.

#pragma once

#if TRINITY_PLATFORM == TRINITY_VULKAN

#include "../include/Tr2BufferAL.h"
#include "VulkanIncludes.h"

namespace TrinityALImpl
{

class VulkanDevice;

// Memory placement by CPU usage:
// - NONE: device-local, filled from a staging copy (initial data is required unless the buffer is a UAV; without it the
//   buffer starts zeroed).
// - READ and/or WRITE: host-visible and persistently mapped (random access when readable, write-combined otherwise).
// Maps synchronize with the GPU conservatively (all submitted work) until per-resource use tracking exists;
// NON_SYNCRONIZED_WRITE skips that, as the flag promises.
class Tr2BufferAL : public Tr2DeviceResourceAL<Tr2BufferAL>
{
public:
	~Tr2BufferAL();

	ALResult Create(
		const Tr2BufferDescriptionAL& desc,
		const void* initialData,
		Tr2PrimaryRenderContextAL& renderContext );

	void Destroy();

	bool IsValid() const;

	Tr2ALMemoryType GetMemoryClass() const;

	const Tr2BufferDescriptionAL& GetDesc() const;

	ALResult MapForReading( const void*& data, Tr2RenderContextAL& renderContext );
	ALResult MapForReading( const void*& data, uint32_t offset, uint32_t size, Tr2RenderContextAL& renderContext );
	void UnmapForReading( Tr2RenderContextAL& renderContext );

	ALResult MapForWriting( void*& data, Tr2RenderContextAL& renderContext );
	void UnmapForWriting( Tr2RenderContextAL& renderContext );

	ALResult UpdateBuffer( uint32_t offset, uint32_t size, const void* data, Tr2RenderContextAL& renderContext );

	void Describe( Tr2DeviceResourceDescriptionAL& description ) const;

	ALResult SetName( const char* name );

	uint32_t GetSrvIndexInHeap() const;
	uint32_t GetUavIndexInHeap() const;

	VkBuffer GetVkBuffer() const
	{
		return m_vkBuffer;
	}
	VkDeviceSize GetByteSize() const
	{
		return VkDeviceSize( m_desc.count ) * m_desc.stride;
	}

private:
	std::shared_ptr<VulkanDevice> m_device;
	VkBuffer m_vkBuffer = VK_NULL_HANDLE;
	VmaAllocation m_allocation = VK_NULL_HANDLE;
	void* m_mapped = nullptr;
	Tr2BufferDescriptionAL m_desc;

	friend class Tr2RenderContextAL;
	friend class Tr2PrimaryRenderContextAL;
};

}

#endif

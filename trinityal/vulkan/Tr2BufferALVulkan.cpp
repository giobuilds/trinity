// Copyright © 2023 CCP ehf.

#include "StdAfx.h"

#if TRINITY_PLATFORM == TRINITY_VULKAN

#include "Tr2BufferALVulkan.h"
#include "Tr2RenderContextVulkan.h"
#include "VulkanDevice.h"
#include "VulkanFormats.h"
#include "ALLog.h"

namespace
{

template <typename T>
bool HasFlag( T value, T flag )
{
	return ( value & flag ) == flag;
}

VkBufferUsageFlags UsageFlags( const Tr2BufferDescriptionAL& desc )
{
	// Any buffer can be a copy source/destination (initial data, UpdateBuffer, CopySubBuffer).
	VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const bool typed = desc.format != Tr2RenderContextEnum::PIXEL_FORMAT_UNKNOWN;
	if( HasFlag( desc.gpuUsage, Tr2GpuUsage::VERTEX_BUFFER ) )
	{
		usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	}
	if( HasFlag( desc.gpuUsage, Tr2GpuUsage::INDEX_BUFFER ) )
	{
		usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	}
	// Shaders see a buffer as typed (Buffer<T>: texel buffer) or structured/raw (storage buffer), whatever it was
	// created as, so a typed buffer gets both.
	if( HasFlag( desc.gpuUsage, Tr2GpuUsage::SHADER_RESOURCE ) )
	{
		usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | ( typed ? VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT : 0 );
	}
	if( HasFlag( desc.gpuUsage, Tr2GpuUsage::UNORDERED_ACCESS ) )
	{
		usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | ( typed ? VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT : 0 );
	}
	if( HasFlag( desc.gpuUsage, Tr2GpuUsage::DRAW_INDIRECT_ARGS ) )
	{
		usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	}
	return usage;
}

}

namespace TrinityALImpl
{

Tr2BufferAL::~Tr2BufferAL()
{
	Destroy();
}

ALResult Tr2BufferAL::Create(
	const Tr2BufferDescriptionAL& desc,
	const void* initialData,
	Tr2PrimaryRenderContextAL& renderContext )
{
	Destroy();

	if( desc.count == 0 || desc.stride == 0 )
	{
		return E_INVALIDARG;
	}
	if( !renderContext.IsValid() )
	{
		return E_INVALIDCALL;
	}
	// Immutable (neither CPU- nor GPU-writable) buffers need their contents up front, as on D3D12.
	const bool immutable = !HasFlag( desc.cpuUsage, Tr2CpuUsage::WRITE ) && !HasFlag( desc.gpuUsage, Tr2GpuUsage::UNORDERED_ACCESS );
	if( immutable && !initialData )
	{
		return E_INVALIDARG;
	}
	if( HasFlag( desc.cpuUsage, Tr2CpuUsage::READ ) && HasFlag( desc.cpuUsage, Tr2CpuUsage::WRITE_OFTEN ) )
	{
		return E_INVALIDARG;
	}

	m_device = renderContext.GetVulkanDeviceShared();
	m_desc = desc;

	const bool hostVisible = desc.cpuUsage != Tr2CpuUsage::NONE;
	VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.size = GetByteSize();
	bufferInfo.usage = UsageFlags( desc );

	VmaAllocationCreateInfo allocationInfo{};
	if( hostVisible )
	{
		allocationInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		allocationInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
		if( HasFlag( desc.cpuUsage, Tr2CpuUsage::READ ) )
		{
			allocationInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU; // cached host memory for readback
		}
	}
	else
	{
		allocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	}

	VmaAllocationInfo info{};
	VkResult result = vmaCreateBuffer( m_device->GetAllocator(), &bufferInfo, &allocationInfo, &m_vkBuffer, &m_allocation, &info );
	if( result != VK_SUCCESS )
	{
		CCP_AL_LOGERR( "Vulkan: buffer of %llu bytes failed: %s", (unsigned long long)bufferInfo.size, VkResultToString( result ) );
		m_vkBuffer = VK_NULL_HANDLE;
		m_allocation = VK_NULL_HANDLE;
		m_device.reset();
		return E_OUTOFMEMORY;
	}
	m_mapped = info.pMappedData;

	if( initialData )
	{
		if( m_mapped )
		{
			memcpy( m_mapped, initialData, size_t( GetByteSize() ) );
			vmaFlushAllocation( m_device->GetAllocator(), m_allocation, 0, VK_WHOLE_SIZE );
		}
		else if( !m_device->UploadToBuffer( m_vkBuffer, 0, initialData, GetByteSize() ) )
		{
			Destroy();
			return E_OUTOFMEMORY;
		}
	}
	else if( m_mapped )
	{
		memset( m_mapped, 0, size_t( GetByteSize() ) );
		vmaFlushAllocation( m_device->GetAllocator(), m_allocation, 0, VK_WHOLE_SIZE );
	}
	else
	{
		// Start from zeros, as D3D12 committed resources do.
		m_device->RecordFullBarrier();
		vkCmdFillBuffer( m_device->GetCommandBuffer(), m_vkBuffer, 0, VK_WHOLE_SIZE, 0 );
		m_device->RecordFullBarrier();
	}
	return S_OK;
}

void Tr2BufferAL::Destroy()
{
	if( m_texelView )
	{
		VkDevice device = m_device->GetHandle();
		VkBufferView view = m_texelView;
		m_device->ReleaseLater( [device, view] { vkDestroyBufferView( device, view, nullptr ); } );
		m_texelView = VK_NULL_HANDLE;
	}
	if( m_vkBuffer )
	{
		VmaAllocator allocator = m_device->GetAllocator();
		VkBuffer buffer = m_vkBuffer;
		VmaAllocation allocation = m_allocation;
		m_device->ReleaseLater( [allocator, buffer, allocation] { vmaDestroyBuffer( allocator, buffer, allocation ); } );
	}
	m_vkBuffer = VK_NULL_HANDLE;
	m_allocation = VK_NULL_HANDLE;
	m_mapped = nullptr;
	m_desc.count = 0;
	m_device.reset();
}

bool Tr2BufferAL::IsValid() const
{
	return m_vkBuffer != VK_NULL_HANDLE;
}

Tr2ALMemoryType Tr2BufferAL::GetMemoryClass() const
{
	return AL_MEMORY_MANAGED;
}

const Tr2BufferDescriptionAL& Tr2BufferAL::GetDesc() const
{
	return m_desc;
}

ALResult Tr2BufferAL::MapForReading( const void*& data, Tr2RenderContextAL& renderContext )
{
	return MapForReading( data, 0, uint32_t( GetByteSize() ), renderContext );
}

ALResult Tr2BufferAL::MapForReading( const void*& data, uint32_t offset, uint32_t size, Tr2RenderContextAL& renderContext )
{
	data = nullptr;
	if( !renderContext.IsValid() || !IsValid() )
	{
		return E_INVALIDCALL;
	}
	if( size == 0 || VkDeviceSize( offset ) + size > GetByteSize() )
	{
		return E_INVALIDARG;
	}
	if( !HasFlag( m_desc.cpuUsage, Tr2CpuUsage::READ ) || !m_mapped )
	{
		return E_INVALIDCALL;
	}
	m_device->SynchronizeForCpuAccess();
	vmaInvalidateAllocation( m_device->GetAllocator(), m_allocation, offset, size );
	data = static_cast<const uint8_t*>( m_mapped ) + offset;
	return S_OK;
}

void Tr2BufferAL::UnmapForReading( Tr2RenderContextAL& )
{
}

ALResult Tr2BufferAL::MapForWriting( void*& data, Tr2RenderContextAL& renderContext )
{
	data = nullptr;
	if( !renderContext.IsValid() || !IsValid() )
	{
		return E_INVALIDCALL;
	}
	if( !HasFlag( m_desc.cpuUsage, Tr2CpuUsage::WRITE ) || !m_mapped )
	{
		return E_INVALIDCALL;
	}
	if( !HasFlag( m_desc.cpuUsage, Tr2CpuUsage::NON_SYNCRONIZED_WRITE ) )
	{
		// The GPU may still read the current contents.
		m_device->SynchronizeForCpuAccess();
	}
	data = m_mapped;
	return S_OK;
}

void Tr2BufferAL::UnmapForWriting( Tr2RenderContextAL& )
{
	if( m_allocation )
	{
		vmaFlushAllocation( m_device->GetAllocator(), m_allocation, 0, VK_WHOLE_SIZE );
	}
}

ALResult Tr2BufferAL::UpdateBuffer( uint32_t offset, uint32_t size, const void* data, Tr2RenderContextAL& renderContext )
{
	if( !renderContext.IsValid() || !IsValid() )
	{
		return E_INVALIDCALL;
	}
	if( VkDeviceSize( offset ) + size > GetByteSize() )
	{
		return E_INVALIDARG;
	}
	if( !HasFlag( m_desc.cpuUsage, Tr2CpuUsage::WRITE ) || HasFlag( m_desc.cpuUsage, Tr2CpuUsage::WRITE_OFTEN ) )
	{
		return E_INVALIDCALL;
	}
	if( size == 0 )
	{
		return S_OK;
	}
	// Ordered with the command stream, like UpdateSubresource: earlier draws see the old contents.
	return m_device->UploadToBuffer( m_vkBuffer, offset, data, size ) ? S_OK : E_OUTOFMEMORY;
}

VkBufferView Tr2BufferAL::GetTexelView()
{
	if( m_texelView || !m_vkBuffer || m_desc.format == Tr2RenderContextEnum::PIXEL_FORMAT_UNKNOWN )
	{
		return m_texelView;
	}
	VkBufferViewCreateInfo info{ VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
	info.buffer = m_vkBuffer;
	info.format = ToVkFormat( m_desc.format );
	info.range = VK_WHOLE_SIZE;
	if( info.format == VK_FORMAT_UNDEFINED )
	{
		return VK_NULL_HANDLE;
	}
	VkResult result = vkCreateBufferView( m_device->GetHandle(), &info, nullptr, &m_texelView );
	if( result != VK_SUCCESS )
	{
		CCP_AL_LOGERR( "Vulkan: vkCreateBufferView failed: %s", VkResultToString( result ) );
		m_texelView = VK_NULL_HANDLE;
	}
	return m_texelView;
}

uint32_t Tr2BufferAL::GetSrvIndexInHeap() const
{
	return 0xffffffff; // no bindless heap (SupportsBindlessTextures() is false)
}

uint32_t Tr2BufferAL::GetUavIndexInHeap() const
{
	return 0xffffffff;
}

void Tr2BufferAL::Describe( Tr2DeviceResourceDescriptionAL& ) const
{
}

ALResult Tr2BufferAL::SetName( const char* name )
{
	if( m_device && m_vkBuffer )
	{
		m_device->SetObjectName( VK_OBJECT_TYPE_BUFFER, uint64_t( m_vkBuffer ), name );
	}
	return S_OK;
}

}

#endif

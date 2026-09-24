// Copyright © 2023 CCP ehf.

#include "StdAfx.h"
#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "Tr2TextureALVulkan.h"
#include "Tr2RenderContextVulkan.h"
#include "VulkanFormats.h"
#include "ALLog.h"

#include <algorithm>
#include <vector>

using namespace Tr2RenderContextEnum;

namespace
{

uint32_t BlockDimension( PixelFormat format )
{
	return IsCompressedFormat( format ) ? 4 : 1;
}

uint32_t ElementBytes( PixelFormat format )
{
	return IsCompressedFormat( format ) ? GetBlockByteSize( format ) : GetBytesPerPixel( format );
}

// Where a region of one subresource lies in the image and how it is laid out, tightly packed, in a buffer. Boxes are in
// trinity's convention: compressed mip sizes are rounded up to whole blocks (GetMipWidth), which can exceed the image's
// true mip size; the copy extent is clamped to the true size, which Vulkan accepts for partial blocks at the edge.
struct Footprint
{
	VkOffset3D offset{};
	VkExtent3D extent{};
	uint32_t rowBytes = 0;
	uint32_t rows = 0; // rows of blocks
	uint32_t slices = 0;
	uint32_t rowTexels = 0; // bufferRowLength
	uint32_t imageRows = 0; // bufferImageHeight

	VkDeviceSize GetSize() const
	{
		return VkDeviceSize( rowBytes ) * rows * slices;
	}
	bool IsEmpty() const
	{
		return extent.width == 0 || extent.height == 0 || extent.depth == 0;
	}
};

Footprint GetFootprint( const Tr2BitmapDimensions& desc, uint32_t mip, const Tr2TextureCoordBox* box )
{
	uint32_t left = 0, top = 0, front = 0;
	uint32_t right = desc.GetMipWidth( mip ), bottom = desc.GetMipHeight( mip ), back = desc.GetMipDepth( mip );
	if( box )
	{
		left = box->left;
		top = box->top;
		front = box->front;
		right = box->right;
		bottom = box->bottom;
		back = box->back;
	}
	const uint32_t trueWidth = std::max( desc.GetWidth() >> mip, 1u );
	const uint32_t trueHeight = desc.GetType() == TEX_TYPE_1D ? 1u : std::max( desc.GetHeight() >> mip, 1u );
	const uint32_t trueDepth = desc.GetMipDepth( mip );

	auto clampedSize = []( uint32_t start, uint32_t end, uint32_t size ) {
		end = std::min( end, size );
		return end > start ? end - start : 0u;
	};

	const uint32_t block = BlockDimension( desc.GetFormat() );
	Footprint f;
	f.offset = { int32_t( left ), int32_t( top ), int32_t( front ) };
	f.extent = { clampedSize( left, right, trueWidth ), clampedSize( top, bottom, trueHeight ), clampedSize( front, back, trueDepth ) };
	const uint32_t blocksX = right > left ? ( right - left + block - 1 ) / block : 0;
	const uint32_t blocksY = bottom > top ? ( bottom - top + block - 1 ) / block : 0;
	f.rowBytes = blocksX * ElementBytes( desc.GetFormat() );
	f.rows = blocksY;
	f.slices = back > front ? back - front : 0;
	f.rowTexels = blocksX * block;
	f.imageRows = blocksY * block;
	return f;
}

// Buffer <-> image copies address one aspect; depth/stencil textures are never CPU-accessible, so depth is the one.
VkImageAspectFlags BufferCopyAspect( VkImageAspectFlags aspect )
{
	return ( aspect & VK_IMAGE_ASPECT_DEPTH_BIT ) ? VkImageAspectFlags( VK_IMAGE_ASPECT_DEPTH_BIT ) : aspect;
}

VkBufferImageCopy BufferImageRegion( const Footprint& f, VkDeviceSize bufferOffset, VkImageAspectFlags aspect, uint32_t mip, uint32_t layer )
{
	VkBufferImageCopy region{};
	region.bufferOffset = bufferOffset;
	region.bufferRowLength = f.rowTexels;
	region.bufferImageHeight = f.imageRows;
	region.imageSubresource = { BufferCopyAspect( aspect ), mip, layer, 1 };
	region.imageOffset = f.offset;
	region.imageExtent = f.extent;
	return region;
}

// Copies rows of a region from memory with the given pitches into a tightly packed destination.
void PackRows( uint8_t* dst, const Footprint& f, const void* src, uint32_t pitch, uint32_t slicePitch )
{
	if( pitch == 0 )
	{
		pitch = f.rowBytes;
	}
	if( slicePitch == 0 )
	{
		slicePitch = pitch * f.rows;
	}
	const uint32_t copyBytes = std::min( pitch, f.rowBytes );
	for( uint32_t z = 0; z < f.slices; ++z )
	{
		const uint8_t* slice = static_cast<const uint8_t*>( src ) + size_t( z ) * slicePitch;
		for( uint32_t y = 0; y < f.rows; ++y )
		{
			memcpy( dst, slice + size_t( y ) * pitch, copyBytes );
			dst += f.rowBytes;
		}
	}
}

VkDeviceSize AlignUp( VkDeviceSize value, VkDeviceSize alignment )
{
	return ( value + alignment - 1 ) / alignment * alignment;
}

VkImageUsageFlags ImageUsage( Tr2GpuUsage::Type gpuUsage )
{
	VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if( HasFlag( gpuUsage, Tr2GpuUsage::SHADER_RESOURCE ) )
	{
		usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
	}
	if( HasFlag( gpuUsage, Tr2GpuUsage::RENDER_TARGET ) )
	{
		usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}
	if( HasFlag( gpuUsage, Tr2GpuUsage::DEPTH_STENCIL ) )
	{
		usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	}
	if( HasFlag( gpuUsage, Tr2GpuUsage::UNORDERED_ACCESS ) )
	{
		usage |= VK_IMAGE_USAGE_STORAGE_BIT;
	}
	return usage;
}

uint32_t LayerCount( const Tr2BitmapDimensions& desc )
{
	return desc.GetType() == TEX_TYPE_3D ? 1u : std::max( desc.GetArraySize(), 1u );
}

}

namespace TrinityALImpl
{
Tr2TextureAL::Tr2TextureAL() :
	m_gpuUsage( Tr2GpuUsage::NONE ),
	m_cpuUsage( Tr2CpuUsage::NONE )
{
}

Tr2TextureAL::~Tr2TextureAL()
{
	Destroy();
}

ALResult Tr2TextureAL::Create( const Tr2BitmapDimensions& desc, const Tr2MsaaDesc& msaa, Tr2GpuUsage::Type gpuUsage, Tr2CpuUsage::Type cpuUsage, Tr2SubresourceData* initialData, Tr2PrimaryRenderContextAL& renderContext )
{
	Destroy();

	if( HasBufferFlags( gpuUsage ) )
	{
		return E_INVALIDARG;
	}

	if( !renderContext.IsValid() )
	{
		return E_FAIL;
	}
	if( msaa.samples > 1 )
	{
		if( HasFlag( gpuUsage, Tr2GpuUsage::UNORDERED_ACCESS ) )
		{
			return E_INVALIDARG;
		}
		if( cpuUsage != Tr2CpuUsage::NONE )
		{
			return E_INVALIDARG;
		}
		if( desc.GetType() != TEX_TYPE_2D )
		{
			return E_INVALIDARG;
		}
	}
	if( desc.GetType() != TEX_TYPE_2D )
	{
		if( desc.GetType() == TEX_TYPE_CUBE )
		{
			if( desc.GetArraySize() != 6 )
			{
				return E_INVALIDARG;
			}
		}
		else if( desc.GetArraySize() > 1 )
		{
			return E_INVALIDARG;
		}
	}
	if( desc.GetType() != TEX_TYPE_2D && HasFlag( gpuUsage, Tr2GpuUsage::DEPTH_STENCIL ) )
	{
		return E_INVALIDARG;
	}
	if( msaa.samples > 1 && desc.GetTrueMipCount() > 1 )
	{
		return E_INVALIDARG;
	}
	if( HasFlag( gpuUsage, Tr2GpuUsage::RENDER_TARGET ) && HasFlag( cpuUsage, Tr2CpuUsage::WRITE ) )
	{
		return E_INVALIDARG;
	}
	if( HasFlag( gpuUsage, Tr2GpuUsage::DEPTH_STENCIL ) && cpuUsage != Tr2CpuUsage::NONE )
	{
		return E_INVALIDARG;
	}
	if( HasFlag( gpuUsage, Tr2GpuUsage::DEPTH_STENCIL ) && desc.GetTrueMipCount() > 1 )
	{
		return E_INVALIDARG;
	}
	if( desc.GetType() == TEX_TYPE_3D && cpuUsage != Tr2CpuUsage::NONE )
	{
		return E_INVALIDARG;
	}
	if( !IsWritable( gpuUsage ) && !HasFlag( cpuUsage, Tr2CpuUsage::WRITE ) && !initialData )
	{
		return E_INVALIDARG;
	}
	if( desc.GetWidth() == 0 || desc.GetHeight() == 0 )
	{
		return E_INVALIDARG;
	}
	if( msaa.samples > 64 || ( msaa.samples & ( msaa.samples - 1 ) ) != 0 )
	{
		return E_INVALIDARG; // Vulkan sample counts are powers of two
	}

	auto device = renderContext.GetVulkanDeviceShared();
	const PixelFormat format = desc.GetFormat();
	const VkFormat vkFormat = device->GetImageFormat( format );
	if( vkFormat == VK_FORMAT_UNDEFINED )
	{
		CCP_AL_LOGERR( "Vulkan: texture format %d has no Vulkan equivalent", int( format ) );
		return E_INVALIDARG;
	}
	const bool depth = IsDepthFormat( format );

	VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	switch( desc.GetType() )
	{
	case TEX_TYPE_1D:
		imageInfo.imageType = VK_IMAGE_TYPE_1D;
		imageInfo.extent = { desc.GetWidth(), 1, 1 };
		break;
	case TEX_TYPE_3D:
		imageInfo.imageType = VK_IMAGE_TYPE_3D;
		imageInfo.extent = { desc.GetWidth(), desc.GetHeight(), std::max( desc.GetDepth(), 1u ) };
		break;
	case TEX_TYPE_2D:
	case TEX_TYPE_CUBE:
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent = { desc.GetWidth(), desc.GetHeight(), 1 };
		break;
	default:
		return E_INVALIDARG;
	}
	imageInfo.format = vkFormat;
	imageInfo.mipLevels = desc.GetTrueMipCount();
	imageInfo.arrayLayers = LayerCount( desc );
	imageInfo.samples = msaa.samples > 1 ? VkSampleCountFlagBits( msaa.samples ) : VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = ImageUsage( gpuUsage );
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if( desc.GetType() == TEX_TYPE_CUBE )
	{
		imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	}

	// Colour images can be viewed with another format of their family (typeless textures, sRGB views of UNORM data).
	// When the only other format is the sRGB twin, say so, which keeps compression (e.g. AMD DCC) enabled.
	VkFormat viewFormats[2] = { vkFormat, VK_FORMAT_UNDEFINED };
	VkImageFormatListCreateInfo formatList{ VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO };
	if( !depth )
	{
		imageInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
		const PixelFormat srgb = MakeSrgb( format );
		if( srgb != format && MakeTypeless( format ) != format && ToVkFormat( srgb ) != VK_FORMAT_UNDEFINED )
		{
			viewFormats[1] = ToVkFormat( srgb );
			formatList.viewFormatCount = 2;
			formatList.pViewFormats = viewFormats;
			imageInfo.pNext = &formatList;
		}
	}

	VkImageFormatProperties properties{};
	VkResult result = vkGetPhysicalDeviceImageFormatProperties( device->GetPhysicalDevice(), vkFormat, imageInfo.imageType, imageInfo.tiling, imageInfo.usage, imageInfo.flags, &properties );
	if( result != VK_SUCCESS || imageInfo.mipLevels > properties.maxMipLevels || imageInfo.arrayLayers > properties.maxArrayLayers ||
		imageInfo.extent.width > properties.maxExtent.width || imageInfo.extent.height > properties.maxExtent.height ||
		imageInfo.extent.depth > properties.maxExtent.depth || !( properties.sampleCounts & imageInfo.samples ) )
	{
		CCP_AL_LOGERR( "Vulkan: format %d is not supported for a %ux%ux%u texture with usage 0x%x and %u samples", int( format ), imageInfo.extent.width, imageInfo.extent.height, imageInfo.extent.depth, imageInfo.usage, unsigned( imageInfo.samples ) );
		return E_INVALIDARG;
	}

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	result = vmaCreateImage( device->GetAllocator(), &imageInfo, &allocationInfo, &m_image, &m_allocation, nullptr );
	if( result != VK_SUCCESS )
	{
		CCP_AL_LOGERR( "Vulkan: %ux%u texture failed: %s", imageInfo.extent.width, imageInfo.extent.height, VkResultToString( result ) );
		m_image = VK_NULL_HANDLE;
		m_allocation = VK_NULL_HANDLE;
		return E_OUTOFMEMORY;
	}

	m_device = device;
	m_desc = desc;
	m_gpuUsage = gpuUsage;
	m_cpuUsage = cpuUsage;
	m_msaa = msaa;
	m_vkFormat = vkFormat;
	m_aspect = AspectMask( format );

	RecordInitialLayout();
	if( initialData && !UploadInitialData( initialData ) )
	{
		Destroy();
		return E_OUTOFMEMORY;
	}
	return S_OK;
}

void Tr2TextureAL::RecordInitialLayout()
{
	VkCommandBuffer commandBuffer = m_device->GetCommandBuffer();

	VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
	barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	barrier.srcAccessMask = VK_ACCESS_2_NONE;
	barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = m_image;
	barrier.subresourceRange = { m_aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
	VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	dependency.imageMemoryBarrierCount = 1;
	dependency.pImageMemoryBarriers = &barrier;
	vkCmdPipelineBarrier2( commandBuffer, &dependency );

	// Start from zeros, as D3D12 committed resources do, rather than whatever the memory held. Compressed images cannot be
	// cleared; they are either given initial data or written by the CPU before use.
	const VkImageSubresourceRange range = barrier.subresourceRange;
	if( m_aspect & ( VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT ) )
	{
		VkClearDepthStencilValue clear{ 0.0f, 0 };
		vkCmdClearDepthStencilImage( commandBuffer, m_image, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range );
		m_device->RecordFullBarrier();
	}
	else if( !IsCompressedFormat( m_desc.GetFormat() ) )
	{
		VkClearColorValue clear{};
		vkCmdClearColorImage( commandBuffer, m_image, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range );
		m_device->RecordFullBarrier();
	}
}

bool Tr2TextureAL::UploadInitialData( const Tr2SubresourceData* initialData )
{
	const uint32_t mipCount = m_desc.GetTrueMipCount();
	const uint32_t layerCount = LayerCount( m_desc );

	std::vector<VkBufferImageCopy> regions;
	std::vector<std::pair<Footprint, const Tr2SubresourceData*>> sources;
	VkDeviceSize total = 0;
	for( uint32_t layer = 0; layer < layerCount; ++layer )
	{
		for( uint32_t mip = 0; mip < mipCount; ++mip )
		{
			const Tr2SubresourceData& data = initialData[layer * mipCount + mip];
			Footprint f = GetFootprint( m_desc, mip, nullptr );
			if( !data.m_sysMem || f.IsEmpty() )
			{
				continue;
			}
			regions.push_back( BufferImageRegion( f, total, m_aspect, mip, layer ) );
			sources.emplace_back( f, &data );
			total = AlignUp( total + f.GetSize(), 16 );
		}
	}
	if( regions.empty() )
	{
		return true;
	}

	VulkanDevice::StagingBuffer staging;
	if( !m_device->CreateStagingBuffer( total, false, staging ) )
	{
		return false;
	}
	for( size_t i = 0; i < regions.size(); ++i )
	{
		const Tr2SubresourceData& data = *sources[i].second;
		PackRows( static_cast<uint8_t*>( staging.mapped ) + regions[i].bufferOffset, sources[i].first, data.m_sysMem, data.m_sysMemPitch, data.m_sysMemSlicePitch );
	}
	vmaFlushAllocation( m_device->GetAllocator(), staging.allocation, 0, VK_WHOLE_SIZE );

	VkCommandBuffer commandBuffer = m_device->GetCommandBuffer();
	m_device->RecordFullBarrier();
	vkCmdCopyBufferToImage( commandBuffer, staging.buffer, m_image, VK_IMAGE_LAYOUT_GENERAL, uint32_t( regions.size() ), regions.data() );
	m_device->RecordFullBarrier();
	m_device->ReleaseStagingBuffer( staging );
	return true;
}

ALResult Tr2TextureAL::OpenShared( uintptr_t, Tr2GpuUsage::Type, Tr2PrimaryRenderContextAL& )
{
	return E_FAIL;
}

void Tr2TextureAL::Destroy()
{
	if( m_device )
	{
		m_device->ReleaseStagingBuffer( m_readStaging );
		m_device->ReleaseStagingBuffer( m_writeStaging );
		if( m_image )
		{
			VmaAllocator allocator = m_device->GetAllocator();
			VkImage image = m_image;
			VmaAllocation allocation = m_allocation;
			m_device->ReleaseLater( [allocator, image, allocation] { vmaDestroyImage( allocator, image, allocation ); } );
		}
	}
	m_image = VK_NULL_HANDLE;
	m_allocation = VK_NULL_HANDLE;
	m_vkFormat = VK_FORMAT_UNDEFINED;
	m_aspect = 0;
	m_device.reset();
	m_writeRegion = Tr2TextureSubresource();
	m_desc = Tr2BitmapDimensions();
	m_msaa = Tr2MsaaDesc();
	m_gpuUsage = Tr2GpuUsage::NONE;
	m_cpuUsage = Tr2CpuUsage::NONE;
}

bool Tr2TextureAL::IsValid() const
{
	return m_image != VK_NULL_HANDLE;
}

Tr2ALMemoryType Tr2TextureAL::GetMemoryClass() const
{
	return AL_MEMORY_VIDEO;
}

const Tr2BitmapDimensions& Tr2TextureAL::GetDesc() const
{
	return m_desc;
}

const Tr2MsaaDesc& Tr2TextureAL::GetMsaaDesc() const
{
	return m_msaa;
}

Tr2GpuUsage::Type Tr2TextureAL::GetGpuUsage() const
{
	return m_gpuUsage;
}

Tr2CpuUsage::Type Tr2TextureAL::GetCpuUsage() const
{
	return m_cpuUsage;
}

// Like the other platforms, the whole subresource is returned; a box in the region is ignored.
ALResult Tr2TextureAL::MapForReading( const Tr2TextureSubresource& region, bool synchronize, const void*& data, uint32_t& pitch, Tr2RenderContextAL& renderContext )
{
	data = nullptr;
	if( !HasFlag( m_cpuUsage, Tr2CpuUsage::READ ) )
	{
		return E_INVALIDCALL;
	}

	if( !IsValid() || !renderContext.IsValid() )
	{
		return E_FAIL;
	}
	if( !region.IsValidForBitmap( m_desc ) )
	{
		return E_INVALIDARG;
	}
	if( !region.IsSingleSubresource() )
	{
		return E_INVALIDARG;
	}

	const Footprint f = GetFootprint( m_desc, region.m_startMipLevel, nullptr );
	m_device->ReleaseStagingBuffer( m_readStaging );
	if( !m_device->CreateStagingBuffer( f.GetSize(), true, m_readStaging ) )
	{
		return E_OUTOFMEMORY;
	}

	VkBufferImageCopy copy = BufferImageRegion( f, 0, m_aspect, region.m_startMipLevel, region.m_startFace );
	m_device->RecordFullBarrier();
	vkCmdCopyImageToBuffer( m_device->GetCommandBuffer(), m_image, VK_IMAGE_LAYOUT_GENERAL, m_readStaging.buffer, 1, &copy );

	if( synchronize )
	{
		m_device->SynchronizeForCpuAccess();
		vmaInvalidateAllocation( m_device->GetAllocator(), m_readStaging.allocation, 0, VK_WHOLE_SIZE );
	}
	else
	{
		// The caller synchronizes (e.g. with a fence) before reading; the copy only has to be on its way.
		m_device->Submit();
	}

	pitch = f.rowBytes;
	data = m_readStaging.mapped;
	return S_OK;
}

void Tr2TextureAL::UnmapForReading( Tr2RenderContextAL& )
{
	if( m_device )
	{
		m_device->ReleaseStagingBuffer( m_readStaging );
	}
}

// The buffer handed out covers the region (its box, or the whole subresource) with rows of pitch bytes.
ALResult Tr2TextureAL::MapForWriting( const Tr2TextureSubresource& region, void*& data, uint32_t& pitch, Tr2RenderContextAL& renderContext )
{
	data = nullptr;

	if( !HasFlag( m_cpuUsage, Tr2CpuUsage::WRITE ) )
	{
		return E_INVALIDCALL;
	}
	if( !IsValid() || !renderContext.IsValid() )
	{
		return E_FAIL;
	}
	if( !region.IsValidForBitmap( m_desc ) )
	{
		return E_INVALIDARG;
	}
	if( !region.IsSingleSubresource() )
	{
		return E_INVALIDARG;
	}
	if( region.HasBox() && IsCompressedFormat( m_desc.GetFormat() ) )
	{
		return E_INVALIDARG;
	}

	const Footprint f = GetFootprint( m_desc, region.m_startMipLevel, region.HasBox() ? &region.m_box : nullptr );
	m_device->ReleaseStagingBuffer( m_writeStaging );
	if( !m_device->CreateStagingBuffer( f.GetSize(), false, m_writeStaging ) )
	{
		return E_OUTOFMEMORY;
	}
	m_writeRegion = region;

	pitch = f.rowBytes;
	data = m_writeStaging.mapped;
	return S_OK;
}

void Tr2TextureAL::UnmapForWriting( Tr2RenderContextAL& )
{
	if( !m_writeStaging.buffer )
	{
		return;
	}
	const Footprint f = GetFootprint( m_desc, m_writeRegion.m_startMipLevel, m_writeRegion.HasBox() ? &m_writeRegion.m_box : nullptr );
	vmaFlushAllocation( m_device->GetAllocator(), m_writeStaging.allocation, 0, VK_WHOLE_SIZE );
	if( !f.IsEmpty() )
	{
		VkBufferImageCopy copy = BufferImageRegion( f, 0, m_aspect, m_writeRegion.m_startMipLevel, m_writeRegion.m_startFace );
		m_device->RecordFullBarrier();
		vkCmdCopyBufferToImage( m_device->GetCommandBuffer(), m_writeStaging.buffer, m_image, VK_IMAGE_LAYOUT_GENERAL, 1, &copy );
		m_device->RecordFullBarrier();
	}
	m_device->ReleaseStagingBuffer( m_writeStaging );
	m_writeRegion = Tr2TextureSubresource();
}

ALResult Tr2TextureAL::UpdateSubresource( const Tr2TextureSubresource& region, const void* source, uint32_t pitch, uint32_t slicePitch, Tr2RenderContextAL& renderContext )
{
	if( HasFlag( m_cpuUsage, Tr2CpuUsage::WRITE_OFTEN ) )
	{
		return E_INVALIDCALL;
	}
	if( !HasFlag( m_cpuUsage, Tr2CpuUsage::WRITE ) && !IsWritable( m_gpuUsage ) )
	{
		return E_INVALIDCALL;
	}

	if( !IsValid() || !renderContext.IsValid() )
	{
		return E_INVALIDCALL;
	}

	if( !region.IsValidForBitmap( m_desc ) )
	{
		return E_INVALIDARG;
	}
	if( !region.IsSingleSubresource() )
	{
		return E_INVALIDARG;
	}
	if( !source )
	{
		return E_INVALIDARG;
	}

	const Footprint f = GetFootprint( m_desc, region.m_startMipLevel, region.HasBox() ? &region.m_box : nullptr );
	if( f.IsEmpty() )
	{
		return S_OK;
	}
	VulkanDevice::StagingBuffer staging;
	if( !m_device->CreateStagingBuffer( f.GetSize(), false, staging ) )
	{
		return E_OUTOFMEMORY;
	}
	PackRows( static_cast<uint8_t*>( staging.mapped ), f, source, pitch, slicePitch );
	vmaFlushAllocation( m_device->GetAllocator(), staging.allocation, 0, VK_WHOLE_SIZE );

	VkBufferImageCopy copy = BufferImageRegion( f, 0, m_aspect, region.m_startMipLevel, region.m_startFace );
	m_device->RecordFullBarrier();
	vkCmdCopyBufferToImage( m_device->GetCommandBuffer(), staging.buffer, m_image, VK_IMAGE_LAYOUT_GENERAL, 1, &copy );
	m_device->RecordFullBarrier();
	m_device->ReleaseStagingBuffer( staging );
	return S_OK;
}

ALResult Tr2TextureAL::CopySubresourceRegion( const Tr2TextureSubresource& destSubresource, Tr2TextureAL& source, const Tr2TextureSubresource& sourceSubresource, Tr2RenderContextAL& renderContext )
{
	if( !IsValid() || !renderContext.IsValid() )
	{
		return E_INVALIDCALL;
	}
	if( !source.IsValid() )
	{
		return E_INVALIDARG;
	}
	if( !HasFlag( m_cpuUsage, Tr2CpuUsage::WRITE ) && !IsWritable( m_gpuUsage ) )
	{
		return E_INVALIDCALL;
	}
	if( source.m_msaa.samples != m_msaa.samples || source.m_aspect != m_aspect )
	{
		return E_INVALIDARG;
	}

	// Copies whose source and destination sizes differ are cut to the overlap; a full copy is the same with every face
	// and mip.
	Tr2TextureSubresource src = sourceSubresource;
	Tr2TextureSubresource dst = destSubresource;

	if( !Crop( src, source.m_desc, dst, m_desc ) )
	{
		return E_FAIL;
	}

	const uint32_t mipCount = std::min( src.GetMipCount(), dst.GetMipCount() );
	const uint32_t faceCount = std::min( src.GetFaceCount(), dst.GetFaceCount() );
	std::vector<VkImageCopy> regions;
	for( uint32_t mip = 0; mip != mipCount; ++mip )
	{
		const Footprint srcFootprint = GetFootprint( source.m_desc, src.m_startMipLevel + mip, &src.m_box );
		const Footprint dstFootprint = GetFootprint( m_desc, dst.m_startMipLevel + mip, &dst.m_box );
		const VkExtent3D extent = {
			std::min( srcFootprint.extent.width, dstFootprint.extent.width ),
			std::min( srcFootprint.extent.height, dstFootprint.extent.height ),
			std::min( srcFootprint.extent.depth, dstFootprint.extent.depth ),
		};
		if( extent.width && extent.height && extent.depth )
		{
			VkImageCopy copy{};
			copy.srcSubresource = { m_aspect, src.m_startMipLevel + mip, src.m_startFace, faceCount };
			copy.srcOffset = srcFootprint.offset;
			copy.dstSubresource = { m_aspect, dst.m_startMipLevel + mip, dst.m_startFace, faceCount };
			copy.dstOffset = dstFootprint.offset;
			copy.extent = extent;
			regions.push_back( copy );
		}
		if( mip + 1 != mipCount )
		{
			AdvanceMip( src, source.m_desc, mip );
			AdvanceMip( dst, m_desc, mip );
		}
	}
	if( regions.empty() )
	{
		return S_OK;
	}

	m_device->RecordFullBarrier();
	vkCmdCopyImage( m_device->GetCommandBuffer(), source.m_image, VK_IMAGE_LAYOUT_GENERAL, m_image, VK_IMAGE_LAYOUT_GENERAL, uint32_t( regions.size() ), regions.data() );
	m_device->RecordFullBarrier();
	return S_OK;
}

ALResult Tr2TextureAL::GenerateMipMaps( Tr2RenderContextAL& renderContext )
{
	if( !HasFlag( m_gpuUsage, Tr2GpuUsage::RENDER_TARGET ) || !HasFlag( m_gpuUsage, Tr2GpuUsage::SHADER_RESOURCE ) )
	{
		return E_INVALIDCALL;
	}
	if( !IsValid() || !renderContext.IsValid() )
	{
		return E_INVALIDCALL;
	}

	VkFormatProperties properties;
	vkGetPhysicalDeviceFormatProperties( m_device->GetPhysicalDevice(), m_vkFormat, &properties );
	const VkFormatFeatureFlags blit = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
	if( ( properties.optimalTilingFeatures & blit ) != blit )
	{
		return E_FAIL;
	}
	const VkFilter filter = ( properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT ) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;

	VkCommandBuffer commandBuffer = m_device->GetCommandBuffer();
	const uint32_t layers = LayerCount( m_desc );
	auto size = []( uint32_t value, uint32_t mip ) { return int32_t( std::max( value >> mip, 1u ) ); };
	const uint32_t height = m_desc.GetType() == TEX_TYPE_1D ? 1 : m_desc.GetHeight();
	for( uint32_t mip = 1; mip < m_desc.GetTrueMipCount(); ++mip )
	{
		m_device->RecordFullBarrier();
		VkImageBlit region{};
		region.srcSubresource = { m_aspect, mip - 1, 0, layers };
		region.srcOffsets[1] = { size( m_desc.GetWidth(), mip - 1 ), size( height, mip - 1 ), int32_t( m_desc.GetMipDepth( mip - 1 ) ) };
		region.dstSubresource = { m_aspect, mip, 0, layers };
		region.dstOffsets[1] = { size( m_desc.GetWidth(), mip ), size( height, mip ), int32_t( m_desc.GetMipDepth( mip ) ) };
		vkCmdBlitImage( commandBuffer, m_image, VK_IMAGE_LAYOUT_GENERAL, m_image, VK_IMAGE_LAYOUT_GENERAL, 1, &region, filter );
	}
	m_device->RecordFullBarrier();
	return S_OK;
}

ALResult Tr2TextureAL::Resolve( Tr2TextureAL& destination, Tr2RenderContextAL& renderContext )
{
	if( m_msaa.samples <= 1 )
	{
		return destination.CopySubresourceRegion( Tr2TextureSubresource(), *this, Tr2TextureSubresource(), renderContext );
	}

	if( !IsValid() || !renderContext.IsValid() )
	{
		return E_INVALIDCALL;
	}
	if( !destination.IsValid() )
	{
		return E_INVALIDARG;
	}
	if( !HasFlag( destination.m_cpuUsage, Tr2CpuUsage::WRITE ) && !IsWritable( destination.m_gpuUsage ) )
	{
		return E_INVALIDARG;
	}
	if( m_desc.GetWidth() != destination.m_desc.GetWidth() || m_desc.GetHeight() != destination.m_desc.GetHeight() )
	{
		return E_INVALIDARG;
	}
	if( m_desc.GetFormat() != destination.m_desc.GetFormat() )
	{
		return E_INVALIDARG;
	}
	if( destination.m_msaa.samples > 1 )
	{
		return E_INVALIDARG;
	}
	if( m_aspect != VK_IMAGE_ASPECT_COLOR_BIT )
	{
		// vkCmdResolveImage is colour-only; depth resolves need a render pass (dynamic rendering, step 2d).
		return E_FAIL;
	}

	VkImageResolve region{};
	region.srcSubresource = { m_aspect, 0, 0, LayerCount( m_desc ) };
	region.dstSubresource = { m_aspect, 0, 0, LayerCount( m_desc ) };
	region.extent = { m_desc.GetWidth(), m_desc.GetHeight(), 1 };
	m_device->RecordFullBarrier();
	vkCmdResolveImage( m_device->GetCommandBuffer(), m_image, VK_IMAGE_LAYOUT_GENERAL, destination.m_image, VK_IMAGE_LAYOUT_GENERAL, 1, &region );
	m_device->RecordFullBarrier();
	return S_OK;
}

uintptr_t Tr2TextureAL::GetSharedHandle() const
{
	return 0;
}

uint32_t Tr2TextureAL::GetSrvIndexInHeap( ColorSpace ) const
{
	return 0xffffffff; // no bindless heap (SupportsBindlessTextures() is false)
}

uint32_t Tr2TextureAL::GetUavIndexInHeap( uint32_t ) const
{
	return 0xffffffff;
}

void Tr2TextureAL::Describe( Tr2DeviceResourceDescriptionAL& ) const
{
}

ALResult Tr2TextureAL::SetName( const char* name )
{
	m_name = name ? name : "";
	if( m_device && m_image )
	{
		m_device->SetObjectName( VK_OBJECT_TYPE_IMAGE, uint64_t( m_image ), name );
	}
	return S_OK;
}

const char* Tr2TextureAL::GetName() const
{
	return m_name.empty() ? nullptr : m_name.c_str();
}
}

#endif

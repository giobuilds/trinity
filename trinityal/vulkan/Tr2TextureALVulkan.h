// Copyright © 2023 CCP ehf.

#pragma once

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "../include/Tr2TextureAL.h"
#include "../Tr2HalHelperStructures.h"
#include "VulkanDevice.h"

#include <memory>
#include <string>
#include <vector>

namespace TrinityALImpl
{

// A VkImage with its own VMA allocation. Images live in VK_IMAGE_LAYOUT_GENERAL for their whole life (one transition
// from UNDEFINED at creation), so every use (sampling, storage, attachment, copies) is valid without layout tracking;
// ordering comes from the device's full barriers until per-resource state tracking lands. CPU access goes through
// staging buffers: MapForReading copies the subresource out and waits, MapForWriting hands out a region-sized buffer that
// UnmapForWriting copies in, ordered with the command stream.
class Tr2TextureAL : public Tr2DeviceResourceAL<Tr2TextureAL>
{
public:
	Tr2TextureAL();
	~Tr2TextureAL();

	ALResult Create( const Tr2BitmapDimensions& desc, const Tr2MsaaDesc& msaa, Tr2GpuUsage::Type gpuUsage, Tr2CpuUsage::Type cpuUsage, Tr2SubresourceData* initialData, Tr2PrimaryRenderContextAL& renderContext );
	ALResult OpenShared( uintptr_t handle, Tr2GpuUsage::Type gpuUsage, Tr2PrimaryRenderContextAL& renderContext );
	void Destroy();

	bool IsValid() const;
	Tr2ALMemoryType GetMemoryClass() const;
	const Tr2BitmapDimensions& GetDesc() const;
	const Tr2MsaaDesc& GetMsaaDesc() const;
	Tr2GpuUsage::Type GetGpuUsage() const;
	Tr2CpuUsage::Type GetCpuUsage() const;

	ALResult MapForReading( const Tr2TextureSubresource& region, const void*& data, uint32_t& pitch, Tr2RenderContextAL& renderContext )
	{
		return MapForReading( region, true, data, pitch, renderContext );
	}
	ALResult MapForReading( const Tr2TextureSubresource& region, bool synchronize, const void*& data, uint32_t& pitch, Tr2RenderContextAL& renderContext );
	void UnmapForReading( Tr2RenderContextAL& renderContext );
	ALResult MapForWriting( const Tr2TextureSubresource& region, void*& data, uint32_t& pitch, Tr2RenderContextAL& renderContext );
	void UnmapForWriting( Tr2RenderContextAL& renderContext );

	ALResult UpdateSubresource( const Tr2TextureSubresource& region, const void* source, uint32_t pitch, uint32_t slicePitch, Tr2RenderContextAL& renderContext );
	ALResult CopySubresourceRegion( const Tr2TextureSubresource& destSubresource, Tr2TextureAL& source, const Tr2TextureSubresource& sourceSubresource, Tr2RenderContextAL& renderContext );
	ALResult GenerateMipMaps( Tr2RenderContextAL& renderContext );
	ALResult Resolve( Tr2TextureAL& destination, Tr2RenderContextAL& renderContext );
	uintptr_t GetSharedHandle() const;
	void Describe( Tr2DeviceResourceDescriptionAL& description ) const;
	ALResult SetName( const char* name );
	const char* GetName() const;

	uint32_t GetSrvIndexInHeap( Tr2RenderContextEnum::ColorSpace colorSpace = Tr2RenderContextEnum::COLOR_SPACE_LINEAR ) const;
	uint32_t GetUavIndexInHeap( uint32_t mip ) const;

	VkImage GetVkImage() const
	{
		return m_image;
	}
	VkFormat GetVkFormat() const
	{
		return m_vkFormat;
	}
	VkImageAspectFlags GetAspectMask() const
	{
		return m_aspect;
	}

	// Views, created on first use and kept until the texture is destroyed. VK_NULL_HANDLE when the texture cannot be
	// seen that way (e.g. a volume texture through a 2D register).
	VkImageView GetShaderView( VkImageViewType type, Tr2RenderContextEnum::ColorSpace colorSpace );
	VkImageView GetStorageView( VkImageViewType type, uint32_t mip );
	VkImageView GetAttachmentView( uint32_t slice, bool srgb );

private:
	struct ViewKey
	{
		VkImageViewType type;
		VkFormat format;
		VkImageAspectFlags aspect;
		uint32_t baseMip, mipCount, baseLayer, layerCount;
		bool operator==( const ViewKey& other ) const
		{
			return memcmp( this, &other, sizeof( *this ) ) == 0;
		}
	};
	VkImageView GetView( const ViewKey& key );
	// Clamps a view request to what this image allows; false if the view type does not fit the image.
	bool MakeViewKey( VkImageViewType type, VkFormat format, VkImageAspectFlags aspect, uint32_t baseMip, uint32_t mipCount, ViewKey& key ) const;

	void RecordInitialLayout();
	bool UploadInitialData( const Tr2SubresourceData* initialData );

	Tr2BitmapDimensions m_desc;
	Tr2MsaaDesc m_msaa;
	Tr2GpuUsage::Type m_gpuUsage;
	Tr2CpuUsage::Type m_cpuUsage;

	std::shared_ptr<VulkanDevice> m_device;
	VkImage m_image = VK_NULL_HANDLE;
	VmaAllocation m_allocation = VK_NULL_HANDLE;
	VkFormat m_vkFormat = VK_FORMAT_UNDEFINED;
	VkImageAspectFlags m_aspect = 0;
	std::string m_name;
	std::vector<std::pair<ViewKey, VkImageView>> m_views;

	// The buffer handed out by the current map, and for writes the region to copy it into.
	VulkanDevice::StagingBuffer m_readStaging;
	VulkanDevice::StagingBuffer m_writeStaging;
	Tr2TextureSubresource m_writeRegion;
};
}


#endif

// Copyright © 2023 CCP ehf.

#pragma once

#if TRINITY_PLATFORM == TRINITY_VULKAN

#include "../include/Tr2ResourceSetAL.h"
#include "VulkanIncludes.h"

#include <vector>

namespace TrinityALImpl
{
class Tr2TextureAL;

// The SRVs, UAVs and samplers of a description, resolved to Vulkan descriptors for the bindings of the program it was
// created for. The render context writes them, with the bound constant buffers, into a descriptor set at draw time.
// Resources the description leaves empty (or that do not fit the register) become null descriptors.
class Tr2ResourceSetAL : public Tr2DeviceResourceAL<Tr2ResourceSetAL>
{
public:
	struct Descriptor
	{
		uint32_t binding;
		VkDescriptorType type;
		VkDescriptorImageInfo image; // sampler; image views are looked up per program (GetImageView)
		VkDescriptorBufferInfo buffer;
		VkBufferView texelBuffer;
		TrinityALImpl::Tr2TextureAL* texture;
		Tr2RenderContextEnum::ColorSpace colorSpace;
		uint32_t mip;
	};

	// The view of a descriptor's texture for a binding of the bound program, which may declare another image type than
	// the program the set was created for (a set left bound across a program change). VK_NULL_HANDLE if it does not fit.
	static VkImageView GetImageView( const Descriptor& descriptor, VkDescriptorType type, VkImageViewType viewType );

	Tr2ResourceSetAL();
	~Tr2ResourceSetAL();

	ALResult Create( const Tr2ResourceSetDescriptionAL& description, const ::Tr2ShaderProgramAL& program, Tr2PrimaryRenderContextAL& renderContext );
	bool IsValid() const;
	void Destroy();
	Tr2ALMemoryType GetMemoryClass() const;
	void Describe( Tr2DeviceResourceDescriptionAL& description ) const;
	ALResult SetName( const char* name );

	// Sorted by binding.
	const std::vector<Descriptor>& GetDescriptors() const
	{
		return m_descriptors;
	}
	const Descriptor* Find( uint32_t binding ) const;

private:
	std::vector<Descriptor> m_descriptors;
	// Keep what the descriptors refer to alive.
	std::vector<::Tr2TextureAL> m_textures;
	std::vector<::Tr2BufferAL> m_buffers;
	std::vector<::Tr2SamplerStateAL> m_samplers;
	bool m_isValid;
};
}

#endif

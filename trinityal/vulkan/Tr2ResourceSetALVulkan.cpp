// Copyright © 2023 CCP ehf.

#include "StdAfx.h"

#if TRINITY_PLATFORM == TRINITY_VULKAN

#include "Tr2ResourceSetALVulkan.h"
#include "Tr2ShaderProgramALVulkan.h"
#include "Tr2TextureALVulkan.h"
#include "Tr2BufferALVulkan.h"
#include "Tr2SamplerStateALVulkan.h"
#include "Tr2PrimaryRenderContextVulkan.h"

#include <algorithm>

namespace TrinityALImpl
{

Tr2ResourceSetAL::Tr2ResourceSetAL() :
	m_isValid( false )
{
}

Tr2ResourceSetAL::~Tr2ResourceSetAL()
{
	Destroy();
}

ALResult Tr2ResourceSetAL::Create( const Tr2ResourceSetDescriptionAL& description, const ::Tr2ShaderProgramAL& program, Tr2PrimaryRenderContextAL& renderContext )
{
	Destroy();
	if( !renderContext.IsValid() )
	{
		return E_INVALIDCALL;
	}
	if( !program.IsValid() )
	{
		return E_INVALIDARG;
	}

	const Tr2RegisterMapAL& map = description.m_registerMap;
	for( const auto& binding : program.m_program->GetBindings() )
	{
		if( binding.registerClass == SpirvBinding::CONSTANT_BUFFER || binding.immutableSampler || binding.registerIndex >= Tr2RegisterMapAL::MAX_RESOURCES_IN_STAGE )
		{
			continue;
		}
		Descriptor descriptor{};
		descriptor.binding = binding.binding;
		descriptor.type = binding.type;
		descriptor.image.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		descriptor.buffer.range = VK_WHOLE_SIZE; // a null buffer descriptor must have offset 0 and the whole range

		// Registers of a descriptor array beyond the first are not addressable in a description (the register map keys
		// resources by base register), so only element 0 is written.
		const Tr2ResourceSetDescriptionAL::Resource* resource = nullptr;
		if( binding.registerClass == SpirvBinding::SRV )
		{
			uint32_t index = map.srvs[binding.stage][binding.registerIndex];
			resource = index < map.srvCount ? &description.m_srv[index] : nullptr;
		}
		else if( binding.registerClass == SpirvBinding::UAV )
		{
			uint32_t index = map.uavs[binding.stage][binding.registerIndex];
			resource = index < map.uavCount ? &description.m_uav[index] : nullptr;
		}
		else if( binding.registerClass == SpirvBinding::SAMPLER )
		{
			uint32_t index = map.samplers[binding.stage][binding.registerIndex];
			if( index < map.samplerCount && description.m_samplers[index].type == Tr2ResourceSetDescriptionAL::Sampler::SAMPLER && description.m_samplers[index].sampler.IsValid() )
			{
				auto& sampler = description.m_samplers[index].sampler;
				descriptor.image.sampler = sampler.m_sampler->GetVkSampler();
				m_samplers.push_back( sampler );
			}
			m_descriptors.push_back( descriptor );
			continue;
		}

		if( resource && resource->type == Tr2ResourceSetDescriptionAL::Resource::TEXTURE && resource->texture.IsValid() )
		{
			descriptor.texture = resource->texture.m_texture.get();
			descriptor.colorSpace = binding.registerClass == SpirvBinding::SRV ? resource->colorSpace : Tr2RenderContextEnum::COLOR_SPACE_LINEAR;
			descriptor.mip = binding.registerClass == SpirvBinding::UAV ? resource->mip : 0;
			// Create the views now so drawing only looks them up.
			GetImageView( descriptor, binding.type, binding.viewType );
			m_textures.push_back( resource->texture );
		}
		else if( resource && resource->type == Tr2ResourceSetDescriptionAL::Resource::BUFFER && resource->buffer.IsValid() )
		{
			auto buffer = resource->buffer.m_buffer.get();
			if( binding.type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER || binding.type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER )
			{
				descriptor.texelBuffer = buffer->GetTexelView();
			}
			else if( binding.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER || binding.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER )
			{
				descriptor.buffer = { buffer->GetVkBuffer(), 0, VK_WHOLE_SIZE };
			}
			if( descriptor.texelBuffer || descriptor.buffer.buffer )
			{
				m_buffers.push_back( resource->buffer );
			}
		}
		m_descriptors.push_back( descriptor );
	}
	std::sort( m_descriptors.begin(), m_descriptors.end(), []( const Descriptor& a, const Descriptor& b ) { return a.binding < b.binding; } );
	m_isValid = true;
	return S_OK;
}

VkImageView Tr2ResourceSetAL::GetImageView( const Descriptor& descriptor, VkDescriptorType type, VkImageViewType viewType )
{
	if( !descriptor.texture )
	{
		return VK_NULL_HANDLE;
	}
	if( type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE )
	{
		return descriptor.texture->GetShaderView( viewType, descriptor.colorSpace );
	}
	if( type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE )
	{
		return descriptor.texture->GetStorageView( viewType, descriptor.mip );
	}
	return VK_NULL_HANDLE;
}

const Tr2ResourceSetAL::Descriptor* Tr2ResourceSetAL::Find( uint32_t binding ) const
{
	auto found = std::lower_bound( m_descriptors.begin(), m_descriptors.end(), binding, []( const Descriptor& d, uint32_t b ) { return d.binding < b; } );
	return found != m_descriptors.end() && found->binding == binding ? &*found : nullptr;
}

bool Tr2ResourceSetAL::IsValid() const
{
	return m_isValid;
}

void Tr2ResourceSetAL::Destroy()
{
	// Views belong to the textures and buffers, which keep them until they are destroyed themselves.
	m_descriptors.clear();
	m_textures.clear();
	m_buffers.clear();
	m_samplers.clear();
	m_isValid = false;
}

Tr2ALMemoryType Tr2ResourceSetAL::GetMemoryClass() const
{
	return AL_MEMORY_MANAGED;
}

void Tr2ResourceSetAL::Describe( Tr2DeviceResourceDescriptionAL& ) const
{
}

ALResult Tr2ResourceSetAL::SetName( const char* )
{
	return S_OK;
}
}

#endif

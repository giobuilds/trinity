// Copyright © 2026 CCP ehf.

#pragma once

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "VulkanIncludes.h"
#include "../Tr2RenderContextEnum.h"

#include <string>
#include <vector>

namespace TrinityALImpl
{

// Shaders are compiled with one binding space per D3D register class: descriptor set = register space, binding =
// register + 0 (b), 32 (s), 64 (t) or 128 (u). D3D gives every stage its own registers, Vulkan shares bindings between
// the stages of a pipeline, so at load time each stage's bindings move to their own range of one descriptor set:
//   set 0, binding = stage * STAGE_BINDING_STRIDE + space * SPACE_BINDING_STRIDE + (register + class offset)
namespace SpirvBinding
{
static const uint32_t CB_OFFSET = 0;
static const uint32_t SAMPLER_OFFSET = 32;
static const uint32_t SRV_OFFSET = 64;
static const uint32_t UAV_OFFSET = 128;
static const uint32_t SPACE_BINDING_STRIDE = 256;
static const uint32_t STAGE_BINDING_STRIDE = 1024;

enum Class
{
	CONSTANT_BUFFER,
	SAMPLER,
	SRV,
	UAV,
};

inline uint32_t Make( Tr2RenderContextEnum::ShaderType stage, uint32_t space, uint32_t localBinding )
{
	return uint32_t( stage ) * STAGE_BINDING_STRIDE + space * SPACE_BINDING_STRIDE + localBinding;
}
inline Tr2RenderContextEnum::ShaderType Stage( uint32_t binding )
{
	return Tr2RenderContextEnum::ShaderType( binding / STAGE_BINDING_STRIDE );
}
inline uint32_t Space( uint32_t binding )
{
	return binding % STAGE_BINDING_STRIDE / SPACE_BINDING_STRIDE;
}
inline Class GetClass( uint32_t binding )
{
	uint32_t local = binding % SPACE_BINDING_STRIDE;
	if( local >= UAV_OFFSET )
	{
		return UAV;
	}
	if( local >= SRV_OFFSET )
	{
		return SRV;
	}
	if( local >= SAMPLER_OFFSET )
	{
		return SAMPLER;
	}
	return CONSTANT_BUFFER;
}
inline uint32_t Register( uint32_t binding )
{
	static const uint32_t offsets[] = { CB_OFFSET, SAMPLER_OFFSET, SRV_OFFSET, UAV_OFFSET };
	return binding % SPACE_BINDING_STRIDE - offsets[GetClass( binding )];
}
}

enum class SpirvNumericType
{
	FLOAT,
	SINT,
	UINT,
};

struct SpirvResource
{
	uint32_t binding = 0; // after RemapBindings, in set 0
	VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
	uint32_t count = 1; // 0: runtime-sized array
	VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM; // images only
	bool multisampled = false;
};

struct SpirvVertexInput
{
	std::string semantic; // upper case, without the index
	uint32_t semanticIndex = 0;
	uint32_t location = 0;
	SpirvNumericType numericType = SpirvNumericType::FLOAT;
};

struct SpirvReflection
{
	std::vector<SpirvResource> resources;
	std::vector<SpirvVertexInput> vertexInputs; // vertex shaders only
};

// Moves every DescriptorSet/Binding decoration of a module to the stage's range (see SpirvBinding). Fails on bindings
// outside the compile-time convention (space >= 4 or local binding >= 256).
bool RemapSpirvBindings( std::vector<uint32_t>& words, Tr2RenderContextEnum::ShaderType stage, std::string& error );

// Descriptor bindings (with the descriptor type the module declares) and, for vertex shaders, the input locations by
// semantic (dxc names stage inputs "in.var.<SEMANTIC>").
bool ReflectSpirv( const std::vector<uint32_t>& words, SpirvReflection& reflection, std::string& error );

}

#endif

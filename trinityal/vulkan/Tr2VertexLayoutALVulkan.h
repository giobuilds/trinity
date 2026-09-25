// Copyright © 2023 CCP ehf.

#pragma once

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "../include/Tr2VertexLayoutAL.h"
#include "../Tr2VertexDefinition.h"
#include "VulkanIncludes.h"

#include <string>
#include <vector>

namespace TrinityALImpl
{

// The vertex definition as Vulkan attribute formats per semantic. Pipelines match these to the vertex shader's inputs by
// semantic (see Tr2ShaderProgramAL); strides come from SetStreamSource at draw time.
class Tr2VertexLayoutAL : public Tr2DeviceResourceAL<Tr2VertexLayoutAL>
{
public:
	static const uint32_t MAX_STREAMS = 4;

	struct Element
	{
		const char* semantic; // upper case, as SpirvVertexInput
		uint32_t semanticIndex;
		VkFormat format;
		uint32_t stream;
		uint32_t offset;
	};

	Tr2VertexLayoutAL()
	{
	}

	ALResult Create( const Tr2VertexDefinition& definition,
					 Tr2RenderContextAL& renderContext );

	bool IsValid() const
	{
		return m_valid;
	}

	void Destroy();

	Tr2ALMemoryType GetMemoryClass() const
	{
		return AL_MEMORY_MANAGED;
	}

	void Describe( Tr2DeviceResourceDescriptionAL& description ) const;
	ALResult SetName( const char* name );

	const Element* Find( const std::string& semantic, uint32_t semanticIndex ) const;
	uint32_t GetStreamMask() const
	{
		return m_streamMask;
	}
	bool IsInstanceStream( uint32_t stream ) const
	{
		return ( m_instanceStreamMask & ( 1u << stream ) ) != 0;
	}
	uint32_t GetStepRate( uint32_t stream ) const
	{
		return m_stepRates[stream];
	}
	// Unique per Create, for pipeline cache keys (addresses can be reused).
	uint64_t GetId() const
	{
		return m_id;
	}

private:
	std::vector<Element> m_elements;
	uint32_t m_streamMask = 0;
	uint32_t m_instanceStreamMask = 0;
	uint32_t m_stepRates[MAX_STREAMS] = {};
	bool m_valid = false;
	uint64_t m_id = 0;
	std::string m_name;
};
}

#endif

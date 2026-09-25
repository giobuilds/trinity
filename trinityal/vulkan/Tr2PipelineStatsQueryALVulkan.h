// Copyright © 2023 CCP ehf.

#pragma once

#if TRINITY_PLATFORM == TRINITY_VULKAN

#include "../include/Tr2PipelineStatsQueryAL.h"
#include "VulkanQuery.h"

#include <string>

namespace TrinityALImpl
{

// Vulkan's eleven pipeline statistics, in D3D12_QUERY_DATA_PIPELINE_STATISTICS order (they match one to one).
class Tr2PipelineStatsDataAL
{
public:
	static const size_t VALUE_COUNT = 11;
	uint64_t values[VALUE_COUNT] = {};
};

class Tr2PipelineStatsQueryAL : public Tr2DeviceResourceAL<Tr2PipelineStatsQueryAL>
{
public:
	Tr2PipelineStatsQueryAL();
	~Tr2PipelineStatsQueryAL();

	// Fails when the device has no pipelineStatisticsQuery feature.
	ALResult Create( Tr2PrimaryRenderContextAL& renderContext );
	bool IsValid() const;
	void Destroy();

	ALResult Begin( Tr2RenderContextAL& renderContext );
	ALResult End( Tr2RenderContextAL& renderContext );
	ALResult GetStats( Tr2PipelineStatsDataAL& data, Tr2RenderContextAL& renderContext );

	static size_t GetValueCount( const Tr2PipelineStatsDataAL& data );
	static const char* GetLabel( const Tr2PipelineStatsDataAL& data, size_t index );
	static const char* GetDescription( const Tr2PipelineStatsDataAL& data, size_t index );
	static ::Tr2PipelineStatsQueryAL::Value GetValue( const Tr2PipelineStatsDataAL& data, size_t index );

	Tr2ALMemoryType GetMemoryClass() const
	{
		return AL_MEMORY_MANAGED;
	}

	void Describe( Tr2DeviceResourceDescriptionAL& description ) const;
	ALResult SetName( const char* name );

private:
	VulkanQuery m_query;
	std::string m_name;

	Tr2PipelineStatsQueryAL( const Tr2PipelineStatsQueryAL& ) = delete;
	Tr2PipelineStatsQueryAL& operator=( const Tr2PipelineStatsQueryAL& ) = delete;
};
}

#endif

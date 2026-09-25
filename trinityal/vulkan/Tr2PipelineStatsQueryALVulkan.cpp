// Copyright © 2023 CCP ehf.

#include "StdAfx.h"

#if TRINITY_PLATFORM == TRINITY_VULKAN

#include "Tr2PipelineStatsQueryALVulkan.h"
#include "Tr2PrimaryRenderContextVulkan.h"
#include "VulkanDevice.h"

namespace
{

struct Field
{
	const char* label;
	const char* description;
};

// Same labels and descriptions as the DX12 backend; Vulkan writes the results in this (bit) order.
const Field s_fields[] = {
	{ "IAVertices", "Number of vertices read by input assembler" },
	{ "IAPrimitives", "Number of primitives read by the input assembler" },
	{ "VSInvocations", "Number of vertex shader invocations" },
	{ "GSInvocations", "Number of geometry shader invocations" },
	{ "GSPrimitives", "Number of geometry shader output primitives" },
	{ "CInvocations", "Number of primitives that were sent to the rasterizer. When the rasterizer is disabled, this will not increment." },
	{ "CPrimitives", "Number of primitives that were rendered. This may be larger or smaller than CInvocations because after a primitive is "
					 "clipped sometimes it is either broken up into more than one primitive or completely culled." },
	{ "PSInvocations", "Number of pixel shader invocations" },
	{ "HSInvocations", "Number of hull shader invocations" },
	{ "DSInvocations", "Number of domain shader invocations" },
	{ "CSInvocations", "Number of compute shader invocations" },
};
static_assert( sizeof( s_fields ) / sizeof( s_fields[0] ) == TrinityALImpl::Tr2PipelineStatsDataAL::VALUE_COUNT, "field count" );

const VkQueryPipelineStatisticFlags s_statistics =
	VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
	VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
	VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
	VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT |
	VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT |
	VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
	VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
	VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
	VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT |
	VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT |
	VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;

}

namespace TrinityALImpl
{

Tr2PipelineStatsQueryAL::Tr2PipelineStatsQueryAL()
{
}

Tr2PipelineStatsQueryAL::~Tr2PipelineStatsQueryAL()
{
	Destroy();
}

ALResult Tr2PipelineStatsQueryAL::Create( Tr2PrimaryRenderContextAL& renderContext )
{
	Destroy();
	if( !renderContext.IsValid() )
	{
		return E_INVALIDARG;
	}
	auto device = renderContext.GetVulkanDeviceShared();
	if( !device->GetEnabledFeatures().pipelineStatisticsQuery )
	{
		return E_FAIL;
	}
	return m_query.Create( device, VK_QUERY_TYPE_PIPELINE_STATISTICS, s_statistics, Tr2PipelineStatsDataAL::VALUE_COUNT ) ? S_OK : E_FAIL;
}

bool Tr2PipelineStatsQueryAL::IsValid() const
{
	return m_query.IsValid();
}

void Tr2PipelineStatsQueryAL::Destroy()
{
	m_query.Destroy();
}

ALResult Tr2PipelineStatsQueryAL::Begin( Tr2RenderContextAL& )
{
	if( !m_query.IsValid() )
	{
		return E_INVALIDCALL;
	}
	return m_query.Begin( 0 ) ? S_OK : E_INVALIDCALL;
}

ALResult Tr2PipelineStatsQueryAL::End( Tr2RenderContextAL& )
{
	if( !m_query.IsValid() )
	{
		return E_INVALIDCALL;
	}
	return m_query.End() ? S_OK : E_INVALIDCALL;
}

ALResult Tr2PipelineStatsQueryAL::GetStats( Tr2PipelineStatsDataAL& data, Tr2RenderContextAL& )
{
	if( !m_query.IsValid() )
	{
		return E_INVALIDCALL;
	}
	switch( m_query.GetResults( data.values, false ) )
	{
	case VulkanQuery::READY:
		return S_OK;
	case VulkanQuery::NOT_READY:
		return S_FALSE;
	default:
		return E_FAIL;
	}
}

size_t Tr2PipelineStatsQueryAL::GetValueCount( const Tr2PipelineStatsDataAL& )
{
	return Tr2PipelineStatsDataAL::VALUE_COUNT;
}

const char* Tr2PipelineStatsQueryAL::GetLabel( const Tr2PipelineStatsDataAL&, size_t index )
{
	return index < Tr2PipelineStatsDataAL::VALUE_COUNT ? s_fields[index].label : "";
}

const char* Tr2PipelineStatsQueryAL::GetDescription( const Tr2PipelineStatsDataAL&, size_t index )
{
	return index < Tr2PipelineStatsDataAL::VALUE_COUNT ? s_fields[index].description : "";
}

::Tr2PipelineStatsQueryAL::Value Tr2PipelineStatsQueryAL::GetValue( const Tr2PipelineStatsDataAL& data, size_t index )
{
	return index < Tr2PipelineStatsDataAL::VALUE_COUNT ? data.values[index] : 0;
}

void Tr2PipelineStatsQueryAL::Describe( Tr2DeviceResourceDescriptionAL& description ) const
{
	description["type"] = "Tr2PipelineStatsQueryAL";
	description["name"] = m_name;
}

ALResult Tr2PipelineStatsQueryAL::SetName( const char* name )
{
	m_name = name ? name : "";
	return S_OK;
}
}

#endif

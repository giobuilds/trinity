// Copyright © 2023 CCP ehf.

#include "StdAfx.h"

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "Tr2VertexLayoutALVulkan.h"
#include "Tr2VertexDefinition.h"
#include "Tr2RenderContextVulkan.h"
#include "ALLog.h"

#include <atomic>

using namespace Tr2RenderContextEnum;

namespace
{

// HLSL semantic names, as DX12's input layouts use them.
const char* s_usageNames[] = {
	"POSITION",
	"COLOR",
	"NORMAL",
	"TANGENT",
	"BINORMAL",
	"TEXCOORD",
	"BLENDINDICES",
	"BLENDWEIGHT",
};
static_assert( sizeof( s_usageNames ) / sizeof( s_usageNames[0] ) == Tr2VertexDefinition::NUM_USAGE_CODE, "Usage name count mismatch" );

VkFormat ToVkVertexFormat( Tr2VertexDefinition::DataType dataType )
{
#define VD_CASE( x, y )          \
	case Tr2VertexDefinition::x: \
		return y;
	switch( dataType )
	{
		VD_CASE( BYTE_1, VK_FORMAT_R8_SINT );
		VD_CASE( BYTE_2, VK_FORMAT_R8G8_SINT );
		VD_CASE( BYTE_3, VK_FORMAT_R8G8B8_SINT );
		VD_CASE( BYTE_4, VK_FORMAT_R8G8B8A8_SINT );
		VD_CASE( UBYTE_1, VK_FORMAT_R8_UINT );
		VD_CASE( UBYTE_2, VK_FORMAT_R8G8_UINT );
		VD_CASE( UBYTE_3, VK_FORMAT_R8G8B8_UINT );
		VD_CASE( UBYTE_4, VK_FORMAT_R8G8B8A8_UINT );
		VD_CASE( BYTE_1_NORM, VK_FORMAT_R8_SNORM );
		VD_CASE( BYTE_2_NORM, VK_FORMAT_R8G8_SNORM );
		VD_CASE( BYTE_3_NORM, VK_FORMAT_R8G8B8_SNORM );
		VD_CASE( BYTE_4_NORM, VK_FORMAT_R8G8B8A8_SNORM );
		VD_CASE( UBYTE_1_NORM, VK_FORMAT_R8_UNORM );
		VD_CASE( UBYTE_2_NORM, VK_FORMAT_R8G8_UNORM );
		VD_CASE( UBYTE_3_NORM, VK_FORMAT_R8G8B8_UNORM );
		VD_CASE( UBYTE_4_NORM, VK_FORMAT_R8G8B8A8_UNORM );
		VD_CASE( SHORT_1, VK_FORMAT_R16_SINT );
		VD_CASE( SHORT_2, VK_FORMAT_R16G16_SINT );
		VD_CASE( SHORT_3, VK_FORMAT_R16G16B16_SINT );
		VD_CASE( SHORT_4, VK_FORMAT_R16G16B16A16_SINT );
		VD_CASE( USHORT_1, VK_FORMAT_R16_UINT );
		VD_CASE( USHORT_2, VK_FORMAT_R16G16_UINT );
		VD_CASE( USHORT_3, VK_FORMAT_R16G16B16_UINT );
		VD_CASE( USHORT_4, VK_FORMAT_R16G16B16A16_UINT );
		VD_CASE( SHORT_1_NORM, VK_FORMAT_R16_SNORM );
		VD_CASE( SHORT_2_NORM, VK_FORMAT_R16G16_SNORM );
		VD_CASE( SHORT_3_NORM, VK_FORMAT_R16G16B16_SNORM );
		VD_CASE( SHORT_4_NORM, VK_FORMAT_R16G16B16A16_SNORM );
		VD_CASE( USHORT_1_NORM, VK_FORMAT_R16_UNORM );
		VD_CASE( USHORT_2_NORM, VK_FORMAT_R16G16_UNORM );
		VD_CASE( USHORT_3_NORM, VK_FORMAT_R16G16B16_UNORM );
		VD_CASE( USHORT_4_NORM, VK_FORMAT_R16G16B16A16_UNORM );
		VD_CASE( INT32_1, VK_FORMAT_R32_SINT );
		VD_CASE( INT32_2, VK_FORMAT_R32G32_SINT );
		VD_CASE( INT32_3, VK_FORMAT_R32G32B32_SINT );
		VD_CASE( INT32_4, VK_FORMAT_R32G32B32A32_SINT );
		VD_CASE( UINT32_1, VK_FORMAT_R32_UINT );
		VD_CASE( UINT32_2, VK_FORMAT_R32G32_UINT );
		VD_CASE( UINT32_3, VK_FORMAT_R32G32B32_UINT );
		VD_CASE( UINT32_4, VK_FORMAT_R32G32B32A32_UINT );
		VD_CASE( FLOAT16_1, VK_FORMAT_R16_SFLOAT );
		VD_CASE( FLOAT16_2, VK_FORMAT_R16G16_SFLOAT );
		VD_CASE( FLOAT16_3, VK_FORMAT_R16G16B16_SFLOAT );
		VD_CASE( FLOAT16_4, VK_FORMAT_R16G16B16A16_SFLOAT );
		VD_CASE( UFLOAT16_1, VK_FORMAT_R16_SFLOAT );
		VD_CASE( UFLOAT16_2, VK_FORMAT_R16G16_SFLOAT );
		VD_CASE( UFLOAT16_3, VK_FORMAT_R16G16B16_SFLOAT );
		VD_CASE( UFLOAT16_4, VK_FORMAT_R16G16B16A16_SFLOAT );
		VD_CASE( FLOAT32_1, VK_FORMAT_R32_SFLOAT );
		VD_CASE( FLOAT32_2, VK_FORMAT_R32G32_SFLOAT );
		VD_CASE( FLOAT32_3, VK_FORMAT_R32G32B32_SFLOAT );
		VD_CASE( FLOAT32_4, VK_FORMAT_R32G32B32A32_SFLOAT );
		VD_CASE( UFLOAT32_1, VK_FORMAT_R32_SFLOAT );
		VD_CASE( UFLOAT32_2, VK_FORMAT_R32G32_SFLOAT );
		VD_CASE( UFLOAT32_3, VK_FORMAT_R32G32B32_SFLOAT );
		VD_CASE( UFLOAT32_4, VK_FORMAT_R32G32B32A32_SFLOAT );
	default:
		break;
	}
#undef VD_CASE
	return VK_FORMAT_UNDEFINED;
}

}

namespace TrinityALImpl
{

ALResult Tr2VertexLayoutAL::Create( const Tr2VertexDefinition& definition, Tr2RenderContextAL& renderContext )
{
	Destroy();
	if( !renderContext.IsValid() )
	{
		return E_FAIL;
	}
	if( definition.m_items.empty() )
	{
		return E_FAIL;
	}
	for( auto& item : definition.m_items )
	{
		if( item.m_usage >= Tr2VertexDefinition::NUM_USAGE_CODE || item.m_stream >= MAX_STREAMS )
		{
			Destroy();
			return E_INVALIDARG;
		}
		VkFormat format = ToVkVertexFormat( item.m_dataType );
		if( format == VK_FORMAT_UNDEFINED )
		{
			CCP_AL_LOGERR( "Vulkan: vertex data type 0x%x has no Vulkan format", unsigned( item.m_dataType ) );
			Destroy();
			return E_INVALIDARG;
		}
		m_elements.push_back( { s_usageNames[item.m_usage], item.m_usageIndex, format, item.m_stream, item.m_offset } );
		m_streamMask |= 1u << item.m_stream;
		if( item.m_instanceStepRate )
		{
			m_instanceStreamMask |= 1u << item.m_stream;
			m_stepRates[item.m_stream] = item.m_instanceStepRate;
		}
	}
	static std::atomic<uint64_t> s_nextId( 1 );
	m_id = s_nextId++;
	m_valid = true;
	return S_OK;
}

void Tr2VertexLayoutAL::Destroy()
{
	m_elements.clear();
	m_streamMask = 0;
	m_instanceStreamMask = 0;
	std::fill( std::begin( m_stepRates ), std::end( m_stepRates ), 0u );
	m_valid = false;
}

const Tr2VertexLayoutAL::Element* Tr2VertexLayoutAL::Find( const std::string& semantic, uint32_t semanticIndex ) const
{
	for( auto& element : m_elements )
	{
		if( element.semanticIndex == semanticIndex && semantic == element.semantic )
		{
			return &element;
		}
	}
	return nullptr;
}

void Tr2VertexLayoutAL::Describe( Tr2DeviceResourceDescriptionAL& ) const
{
}

ALResult Tr2VertexLayoutAL::SetName( const char* name )
{
	m_name = name ? name : "";
	return S_OK;
}
}

#endif

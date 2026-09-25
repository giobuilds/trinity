// Copyright © 2023 CCP ehf.

#include "StdAfx.h"

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "Tr2OcclusionQueryALVulkan.h"
#include "Tr2RenderContextVulkan.h"
#include "VulkanDevice.h"

namespace TrinityALImpl
{

Tr2OcclusionQueryAL::Tr2OcclusionQueryAL()
{
}

Tr2OcclusionQueryAL::~Tr2OcclusionQueryAL()
{
	Destroy();
}

ALResult Tr2OcclusionQueryAL::Create( Tr2RenderContextAL& renderContext )
{
	Destroy();
	if( !renderContext.IsValid() )
	{
		return E_INVALIDARG;
	}
	auto device = renderContext.GetVulkanDeviceShared();
	if( !m_query.Create( device, VK_QUERY_TYPE_OCCLUSION, 0, 1 ) )
	{
		return E_FAIL;
	}
	m_precise = device->GetEnabledFeatures().occlusionQueryPrecise != VK_FALSE;
	return S_OK;
}

bool Tr2OcclusionQueryAL::IsValid() const
{
	return m_query.IsValid();
}

void Tr2OcclusionQueryAL::Destroy()
{
	m_query.Destroy();
}

ALResult Tr2OcclusionQueryAL::Begin( Tr2RenderContextAL& )
{
	if( !m_query.IsValid() )
	{
		return E_INVALIDCALL;
	}
	return m_query.Begin( m_precise ? VK_QUERY_CONTROL_PRECISE_BIT : 0 ) ? S_OK : E_INVALIDCALL;
}

ALResult Tr2OcclusionQueryAL::End( Tr2RenderContextAL& )
{
	if( !m_query.IsValid() )
	{
		return E_INVALIDCALL;
	}
	return m_query.End() ? S_OK : E_INVALIDCALL;
}

ALResult Tr2OcclusionQueryAL::GetPixelCount( Tr2RenderContextAL&, uint32_t& count, ::Tr2OcclusionQueryAL::WaitMode waitMode )
{
	if( !m_query.IsValid() )
	{
		return E_INVALIDCALL;
	}
	uint64_t samples = 0;
	switch( m_query.GetResults( &samples, waitMode == ::Tr2OcclusionQueryAL::WAIT ) )
	{
	case VulkanQuery::READY:
		count = uint32_t( samples );
		return S_OK;
	case VulkanQuery::NOT_READY:
		return S_FALSE;
	default:
		return E_FAIL;
	}
}

void Tr2OcclusionQueryAL::Describe( Tr2DeviceResourceDescriptionAL& ) const
{
}

ALResult Tr2OcclusionQueryAL::SetName( const char* name )
{
	m_name = name ? name : "";
	return S_OK;
}
}

#endif

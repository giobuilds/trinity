// Copyright © 2023 CCP ehf.

#include "StdAfx.h"

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "Tr2GpuTimerALVulkan.h"
#include "Tr2PrimaryRenderContextVulkan.h"
#include "VulkanDevice.h"
#include "ALLog.h"

namespace TrinityALImpl
{

Tr2GpuTimerAL::Tr2GpuTimerAL()
{
}

Tr2GpuTimerAL::~Tr2GpuTimerAL()
{
	Destroy();
}

ALResult Tr2GpuTimerAL::Create( Tr2PrimaryRenderContextAL& renderContext )
{
	Destroy();
	if( !renderContext.IsValid() )
	{
		return E_INVALIDARG;
	}
	auto device = renderContext.GetVulkanDeviceShared();
	if( device->GetTimestampValidBits() == 0 )
	{
		return E_FAIL; // the queue cannot write timestamps
	}
	VkQueryPoolCreateInfo info{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
	info.queryType = VK_QUERY_TYPE_TIMESTAMP;
	info.queryCount = 2;
	VkResult result = vkCreateQueryPool( device->GetHandle(), &info, nullptr, &m_pool );
	if( result != VK_SUCCESS )
	{
		CCP_AL_LOGERR( "Vulkan: timestamp query pool failed: %s", VkResultToString( result ) );
		m_pool = VK_NULL_HANDLE;
		return E_FAIL;
	}
	m_device = device;
	m_state = READY;
	return S_OK;
}

void Tr2GpuTimerAL::Destroy()
{
	if( m_pool )
	{
		VkDevice device = m_device->GetHandle();
		VkQueryPool pool = m_pool;
		m_device->ReleaseLater( [device, pool] { vkDestroyQueryPool( device, pool, nullptr ); } );
	}
	m_pool = VK_NULL_HANDLE;
	m_device.reset();
	m_state = READY;
	m_lastTime = -1.0f;
}

bool Tr2GpuTimerAL::IsValid() const
{
	return m_pool != VK_NULL_HANDLE;
}

bool Tr2GpuTimerAL::Begin( Tr2RenderContextAL& )
{
	if( !m_pool || m_state != READY )
	{
		return false;
	}
	vkResetQueryPool( m_device->GetHandle(), m_pool, 0, 2 );
	vkCmdWriteTimestamp2( m_device->GetCommandBuffer(), VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, m_pool, 0 );
	m_state = BEGIN_ISSUED;
	return true;
}

void Tr2GpuTimerAL::End( Tr2RenderContextAL& )
{
	if( !m_pool || m_state != BEGIN_ISSUED )
	{
		return;
	}
	vkCmdWriteTimestamp2( m_device->GetCommandBuffer(), VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, m_pool, 1 );
	m_serial = m_device->GetRecordingSerial();
	m_state = END_ISSUED;
}

float Tr2GpuTimerAL::GetTime( Tr2RenderContextAL& )
{
	if( !m_pool || m_state != END_ISSUED || !m_device->IsSerialComplete( m_serial ) )
	{
		return m_lastTime;
	}
	uint64_t ticks[2] = {};
	VkResult result = vkGetQueryPoolResults( m_device->GetHandle(), m_pool, 0, 2, sizeof( ticks ), ticks, sizeof( uint64_t ), VK_QUERY_RESULT_64_BIT );
	if( result != VK_SUCCESS )
	{
		return m_lastTime;
	}
	const uint32_t bits = m_device->GetTimestampValidBits();
	const uint64_t mask = bits >= 64 ? ~0ull : ( 1ull << bits ) - 1;
	const uint64_t elapsed = ( ticks[1] - ticks[0] ) & mask;
	m_lastTime = float( double( elapsed ) * double( m_device->GetLimits().timestampPeriod ) * 1e-9 );
	m_state = READY;
	return m_lastTime;
}

void Tr2GpuTimerAL::Describe( Tr2DeviceResourceDescriptionAL& ) const
{
}

ALResult Tr2GpuTimerAL::SetName( const char* name )
{
	m_name = name ? name : "";
	if( m_device && m_pool )
	{
		m_device->SetObjectName( VK_OBJECT_TYPE_QUERY_POOL, uint64_t( m_pool ), name );
	}
	return S_OK;
}
}

#endif

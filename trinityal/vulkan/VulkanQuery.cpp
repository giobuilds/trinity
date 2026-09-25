// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "VulkanQuery.h"
#include "VulkanDevice.h"
#include "ALLog.h"

namespace TrinityALImpl
{

VulkanQuery::~VulkanQuery()
{
	Destroy();
}

bool VulkanQuery::Create( const std::shared_ptr<VulkanDevice>& device, VkQueryType type, VkQueryPipelineStatisticFlags statistics, uint32_t resultCount )
{
	Destroy();
	VkQueryPoolCreateInfo info{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
	info.queryType = type;
	info.queryCount = 1;
	info.pipelineStatistics = statistics;
	VkResult result = vkCreateQueryPool( device->GetHandle(), &info, nullptr, &m_pool );
	if( result != VK_SUCCESS )
	{
		CCP_AL_LOGERR( "Vulkan: query pool failed: %s", VkResultToString( result ) );
		m_pool = VK_NULL_HANDLE;
		return false;
	}
	m_device = device;
	m_resultCount = resultCount;
	return true;
}

void VulkanQuery::Destroy()
{
	if( m_pool )
	{
		if( m_active && m_device->GetSubmittedFrameCount() == m_beginSubmitCount )
		{
			End(); // a pool must not be destroyed with a query still open in the command buffer
		}
		VkDevice device = m_device->GetHandle();
		VkQueryPool pool = m_pool;
		m_device->ReleaseLater( [device, pool] { vkDestroyQueryPool( device, pool, nullptr ); } );
	}
	m_pool = VK_NULL_HANDLE;
	m_device.reset();
	m_active = false;
	m_serial = 0;
}

bool VulkanQuery::Begin( VkQueryControlFlags flags )
{
	if( !m_pool || m_active )
	{
		return false;
	}
	m_device->RecordFullBarrier(); // closes any open render pass
	VkCommandBuffer commandBuffer = m_device->GetCommandBuffer();
	vkCmdResetQueryPool( commandBuffer, m_pool, 0, 1 );
	vkCmdBeginQuery( commandBuffer, m_pool, 0, flags );
	m_active = true;
	m_beginSubmitCount = m_device->GetSubmittedFrameCount();
	return true;
}

bool VulkanQuery::End()
{
	if( !m_pool || !m_active )
	{
		return false;
	}
	m_active = false;
	if( m_device->GetSubmittedFrameCount() != m_beginSubmitCount )
	{
		CCP_AL_LOGERR( "Vulkan: a query was open across a submission and has been dropped" );
		m_serial = 0;
		return false;
	}
	m_device->RecordFullBarrier();
	vkCmdEndQuery( m_device->GetCommandBuffer(), m_pool, 0 );
	m_serial = m_device->GetRecordingSerial();
	return true;
}

VulkanQuery::Status VulkanQuery::GetResults( uint64_t* results, bool wait )
{
	if( !m_pool )
	{
		return FAILED;
	}
	if( m_serial == 0 )
	{
		return NOT_READY;
	}
	if( wait && !m_device->WaitForSerial( m_serial ) )
	{
		return FAILED;
	}
	if( !m_device->IsSerialComplete( m_serial ) )
	{
		return NOT_READY;
	}
	VkResult result = vkGetQueryPoolResults( m_device->GetHandle(), m_pool, 0, 1, sizeof( uint64_t ) * m_resultCount, results, sizeof( uint64_t ) * m_resultCount, VK_QUERY_RESULT_64_BIT );
	if( result == VK_NOT_READY )
	{
		return NOT_READY;
	}
	return result == VK_SUCCESS ? READY : FAILED;
}

}

#endif

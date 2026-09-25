// Copyright © 2026 CCP ehf.

#pragma once

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "VulkanIncludes.h"

#include <memory>

namespace TrinityALImpl
{
class VulkanDevice;

// One begin/end query (occlusion or pipeline statistics) and the serial of the submission that ends it.
//
// Vulkan requires a query begun inside a render pass to end in the same one, while trinity's Begin/End may span
// render-target changes; so both are recorded outside render passes (closing the open one with a barrier, which the next
// draw reopens). A query must also not span a submission: a submit between Begin and End (e.g. a synchronous readback)
// ends the query there, and End then reports failure.
class VulkanQuery
{
public:
	~VulkanQuery();

	bool Create( const std::shared_ptr<VulkanDevice>& device, VkQueryType type, VkQueryPipelineStatisticFlags statistics, uint32_t resultCount );
	void Destroy();
	bool IsValid() const
	{
		return m_pool != VK_NULL_HANDLE;
	}

	bool Begin( VkQueryControlFlags flags );
	bool End();

	// READY with the results filled in, NOT_READY if the GPU has not got there yet (or the query was never ended).
	enum Status
	{
		READY,
		NOT_READY,
		FAILED,
	};
	Status GetResults( uint64_t* results, bool wait );

	VkQueryPool GetPool() const
	{
		return m_pool;
	}

private:
	std::shared_ptr<VulkanDevice> m_device;
	VkQueryPool m_pool = VK_NULL_HANDLE;
	uint32_t m_resultCount = 0;
	bool m_active = false;
	uint64_t m_beginSubmitCount = 0;
	uint64_t m_serial = 0; // 0: never ended
};

}

#endif

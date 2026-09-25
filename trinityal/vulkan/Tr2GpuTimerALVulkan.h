// Copyright © 2023 CCP ehf.

#pragma once

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "../include/Tr2GpuTimerAL.h"
#include "VulkanIncludes.h"

#include <memory>
#include <string>

namespace TrinityALImpl
{
class VulkanDevice;

// Two timestamps around a stretch of commands. As on DX12, a timer measures one interval at a time: Begin only succeeds
// once the previous interval has been read with GetTime, which returns the last completed measurement in seconds (-1
// before the first). The pool is reset from the host, which is safe exactly then.
class Tr2GpuTimerAL : public Tr2DeviceResourceAL<Tr2GpuTimerAL>
{
public:
	Tr2GpuTimerAL();
	~Tr2GpuTimerAL();

	ALResult Create( Tr2PrimaryRenderContextAL& renderContext );
	void Destroy();

	bool Begin( Tr2RenderContextAL& renderContext );
	void End( Tr2RenderContextAL& renderContext );

	float GetTime( Tr2RenderContextAL& renderContext );

	bool IsValid() const;

	bool operator==( const Tr2GpuTimerAL& other ) const
	{
		return this == &other;
	}

	Tr2ALMemoryType GetMemoryClass() const
	{
		return AL_MEMORY_VIDEO;
	}

	void Describe( Tr2DeviceResourceDescriptionAL& description ) const;
	ALResult SetName( const char* name );

private:
	enum State
	{
		READY,
		BEGIN_ISSUED,
		END_ISSUED,
	};
	std::shared_ptr<VulkanDevice> m_device;
	VkQueryPool m_pool = VK_NULL_HANDLE;
	State m_state = READY;
	uint64_t m_serial = 0;
	float m_lastTime = -1.0f;
	std::string m_name;
};
}

#endif

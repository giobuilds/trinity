// Copyright © 2023 CCP ehf.

#pragma once

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "../include/Tr2FenceAL.h"

#include <memory>
#include <string>

namespace TrinityALImpl
{
class VulkanDevice;

// A point in the command stream: PutFence takes the serial of the submission being recorded; it is reached when the
// GPU has completed that submission (see VulkanDevice::GetRecordingSerial).
class Tr2FenceAL : public Tr2DeviceResourceAL<Tr2FenceAL>
{
public:
	Tr2FenceAL();
	~Tr2FenceAL();

	ALResult Create( Tr2PrimaryRenderContextAL& renderContext );
	void Destroy();

	bool IsValid() const;

	ALResult PutFence( Tr2RenderContextAL& renderContext );
	ALResult IsReached( bool& isReached, Tr2RenderContextAL& renderContext );
	ALResult Wait( Tr2RenderContextAL& renderContext );

	Tr2ALMemoryType GetMemoryClass() const
	{
		return AL_MEMORY_VIDEO;
	}

	void Describe( Tr2DeviceResourceDescriptionAL& description ) const;
	ALResult SetName( const char* name );

private:
	std::shared_ptr<VulkanDevice> m_device;
	uint64_t m_serial = 0; // 0: not put
	std::string m_name;
};
}

#endif

// Copyright © 2023 CCP ehf.

#pragma once

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "../include/Tr2ConstantBufferAL.h"

namespace TrinityALImpl
{
class Tr2ConstantBufferAL : public Tr2DeviceResourceAL<Tr2ConstantBufferAL>
{
public:
	Tr2ConstantBufferAL();

	ALResult Create( uint32_t size, Tr2ConstantUsageAL::Type usage, const void* initialData, Tr2RenderContextAL& renderContext );
	void Destroy();

	ALResult Lock( void** data, Tr2RenderContextAL& renderContext );
	ALResult Unlock( Tr2RenderContextAL& renderContext );

	bool IsValid() const;
	uint32_t GetSize() const;
	Tr2ALMemoryType GetMemoryClass() const;
	void Describe( Tr2DeviceResourceDescriptionAL& description ) const;
	ALResult SetName( const char* name );

	// CPU copy of the constants; binding copies it into the frame's upload ring (set in SetConstants).
	const void* GetData() const
	{
		return m_shadowCopy.get();
	}
	// Incremented on every Unlock, so a binding can tell whether its ring copy is still current.
	uint64_t GetVersion() const
	{
		return m_version;
	}

private:
	Tr2ConstantBufferAL( const Tr2ConstantBufferAL& ) /* = delete */;
	Tr2ConstantBufferAL& operator=( const Tr2ConstantBufferAL& ) /* = delete */;

	CcpMallocBuffer m_shadowCopy;
	Tr2ConstantUsageAL::Type m_usage = Tr2ConstantUsageAL::REUSABLE;
	bool m_locked = false;
	uint64_t m_version = 0;

	friend class Tr2RenderContextAL;
};
}

#endif
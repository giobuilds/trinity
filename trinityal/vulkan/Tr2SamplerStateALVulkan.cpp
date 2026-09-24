// Copyright © 2023 CCP ehf.

#include "StdAfx.h"

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "Tr2SamplerStateALVulkan.h"

namespace TrinityALImpl
{
Tr2SamplerStateAL::Tr2SamplerStateAL()
{
	m_isValid = false;
}

ALResult Tr2SamplerStateAL::Create( const Tr2SamplerDescription&, Tr2RenderContextAL& )
{
	m_isValid = true;
	return S_OK;
}

void Tr2SamplerStateAL::Destroy()
{
	m_isValid = false;
}

uint32_t Tr2SamplerStateAL::GetIndexInHeap() const
{
	return 0xffffffff;
}

bool Tr2SamplerStateAL::IsValid() const
{
	return m_isValid;
}

void Tr2SamplerStateAL::Describe( Tr2DeviceResourceDescriptionAL& ) const
{
}

ALResult Tr2SamplerStateAL::SetName( const char* )
{
	return S_OK;
}
}

#endif
// Copyright © 2023 CCP ehf.

#include "StdAfx.h"

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "Tr2SamplerStateALVulkan.h"
#include "Tr2RenderContextVulkan.h"
#include "VulkanDevice.h"
#include "VulkanStates.h"

namespace TrinityALImpl
{

Tr2SamplerStateAL::Tr2SamplerStateAL()
{
}

Tr2SamplerStateAL::~Tr2SamplerStateAL()
{
	Destroy();
}

ALResult Tr2SamplerStateAL::Create( const Tr2SamplerDescription& description, Tr2RenderContextAL& renderContext )
{
	Destroy();
	if( !renderContext.IsValid() )
	{
		return E_INVALIDCALL;
	}
	auto device = renderContext.GetVulkanDeviceShared();
	m_sampler = CreateVkSampler( *device, description );
	if( !m_sampler )
	{
		return E_FAIL;
	}
	m_device = device;
	return S_OK;
}

void Tr2SamplerStateAL::Destroy()
{
	if( m_sampler )
	{
		VkDevice device = m_device->GetHandle();
		VkSampler sampler = m_sampler;
		m_device->ReleaseLater( [device, sampler] { vkDestroySampler( device, sampler, nullptr ); } );
	}
	m_sampler = VK_NULL_HANDLE;
	m_device.reset();
}

uint32_t Tr2SamplerStateAL::GetIndexInHeap() const
{
	return 0xffffffff; // no bindless heap
}

bool Tr2SamplerStateAL::IsValid() const
{
	return m_sampler != VK_NULL_HANDLE;
}

void Tr2SamplerStateAL::Describe( Tr2DeviceResourceDescriptionAL& ) const
{
}

ALResult Tr2SamplerStateAL::SetName( const char* name )
{
	if( m_device && m_sampler )
	{
		m_device->SetObjectName( VK_OBJECT_TYPE_SAMPLER, uint64_t( m_sampler ), name );
	}
	return S_OK;
}
}

#endif

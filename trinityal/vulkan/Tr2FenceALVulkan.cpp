// Copyright © 2023 CCP ehf.

#include "StdAfx.h"

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "Tr2FenceALVulkan.h"
#include "Tr2RenderContextVulkan.h"
#include "VulkanDevice.h"

namespace TrinityALImpl
{

Tr2FenceAL::Tr2FenceAL()
{
}

Tr2FenceAL::~Tr2FenceAL()
{
	Destroy();
}

ALResult Tr2FenceAL::Create( Tr2PrimaryRenderContextAL& renderContext )
{
	Destroy();
	if( !renderContext.IsValid() )
	{
		return E_INVALIDARG;
	}
	m_device = renderContext.GetVulkanDeviceShared();
	return S_OK;
}

void Tr2FenceAL::Destroy()
{
	m_device.reset();
	m_serial = 0;
}

bool Tr2FenceAL::IsValid() const
{
	return m_device != nullptr;
}

ALResult Tr2FenceAL::PutFence( Tr2RenderContextAL& )
{
	if( !m_device )
	{
		return E_INVALIDCALL;
	}
	m_serial = m_device->GetRecordingSerial();
	return S_OK;
}

ALResult Tr2FenceAL::IsReached( bool& isReached, Tr2RenderContextAL& )
{
	if( !m_device || m_serial == 0 )
	{
		return E_INVALIDCALL;
	}
	isReached = m_device->IsSerialComplete( m_serial );
	return S_OK;
}

ALResult Tr2FenceAL::Wait( Tr2RenderContextAL& )
{
	if( !m_device || m_serial == 0 )
	{
		return E_INVALIDCALL;
	}
	return m_device->WaitForSerial( m_serial ) ? S_OK : E_FAIL;
}

void Tr2FenceAL::Describe( Tr2DeviceResourceDescriptionAL& ) const
{
}

ALResult Tr2FenceAL::SetName( const char* name )
{
	m_name = name ? name : "";
	return S_OK;
}
}

#endif

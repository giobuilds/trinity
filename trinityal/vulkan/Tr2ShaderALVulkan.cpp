// Copyright © 2023 CCP ehf.

#include "StdAfx.h"

#if TRINITY_PLATFORM == TRINITY_VULKAN

#include "Tr2ShaderALVulkan.h"
#include "Tr2RenderContextVulkan.h"
#include "VulkanDevice.h"
#include "ALLog.h"
#include "Tr2HalHelperStructures.h"

using namespace Tr2RenderContextEnum;

namespace TrinityALImpl
{

Tr2ShaderAL::Tr2ShaderAL() :
	m_type( INVALID_SHADER )
{
}

ALResult Tr2ShaderAL::Create(
	Tr2RenderContextEnum::ShaderType type,
	const Tr2ShaderBytecodeAL& bytecode,
	const Tr2ShaderSignatureAL& signature,
	const char* shaderPath,
	Tr2PrimaryRenderContextAL& renderContext )
{
	Destroy();
	if( !renderContext.IsValid() )
	{
		return E_INVALIDCALL;
	}
	if( type >= SHADER_TYPE_COUNT || !bytecode.bytecode || bytecode.size == 0 || bytecode.size % 4 != 0 )
	{
		return E_INVALIDARG;
	}

	std::vector<uint32_t> words( bytecode.size / 4 );
	memcpy( words.data(), bytecode.bytecode, bytecode.size );
	std::string error;
	if( !RemapSpirvBindings( words, type, error ) || !ReflectSpirv( words, m_reflection, error ) )
	{
		CCP_AL_LOGERR( "Vulkan: shader %s: %s", shaderPath && *shaderPath ? shaderPath : "(unnamed)", error.c_str() );
		m_reflection = SpirvReflection();
		return E_INVALIDARG;
	}

	auto device = renderContext.GetVulkanDeviceShared();
	VkShaderModuleCreateInfo moduleInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	moduleInfo.codeSize = words.size() * 4;
	moduleInfo.pCode = words.data();
	VkResult result = vkCreateShaderModule( device->GetHandle(), &moduleInfo, nullptr, &m_module );
	if( result != VK_SUCCESS )
	{
		CCP_AL_LOGERR( "Vulkan: vkCreateShaderModule failed for %s: %s", shaderPath && *shaderPath ? shaderPath : "(unnamed)", VkResultToString( result ) );
		m_module = VK_NULL_HANDLE;
		m_reflection = SpirvReflection();
		return E_INVALIDARG;
	}

	m_bytecode.resize( "Tr2ShaderALVulkan::m_bytecode", bytecode.size );
	if( m_bytecode.empty() )
	{
		Destroy();
		return E_OUTOFMEMORY;
	}
	memcpy( m_bytecode.get(), bytecode.bytecode, bytecode.size );
	m_device = device;
	m_signature = signature;
	m_type = type;
	if( shaderPath && *shaderPath )
	{
		SetName( shaderPath );
	}
	return S_OK;
}

Tr2ShaderAL::~Tr2ShaderAL()
{
	Destroy();
}

void Tr2ShaderAL::Destroy()
{
	if( m_module )
	{
		// Pipelines keep their own copy of the code, so the module can go as soon as nothing is being created from it.
		VkDevice device = m_device->GetHandle();
		VkShaderModule module = m_module;
		m_device->ReleaseLater( [device, module] { vkDestroyShaderModule( device, module, nullptr ); } );
	}
	m_module = VK_NULL_HANDLE;
	m_device.reset();
	m_reflection = SpirvReflection();
	m_signature = Tr2ShaderSignatureAL();
	m_type = INVALID_SHADER;
	m_bytecode.clear();
}

bool Tr2ShaderAL::IsValid() const
{
	return m_type != INVALID_SHADER && m_module != VK_NULL_HANDLE;
}

Tr2RenderContextEnum::ShaderType Tr2ShaderAL::GetType() const
{
	return m_type;
}

ALResult Tr2ShaderAL::GetBytecode( Tr2ShaderBytecodeAL& bytecode ) const
{
	if( m_bytecode.empty() )
	{
		bytecode = Tr2ShaderBytecodeAL();
		return E_INVALIDCALL;
	}
	bytecode.bytecode = m_bytecode.get();
	bytecode.size = m_bytecode.size();
	return S_OK;
}

const Tr2ShaderSignatureAL& Tr2ShaderAL::GetSignature() const
{
	return m_signature;
}

void Tr2ShaderAL::SetNullShaderType( Tr2RenderContextEnum::ShaderType type )
{
	m_type = type;
}

void Tr2ShaderAL::Describe( Tr2DeviceResourceDescriptionAL& ) const
{
}

ALResult Tr2ShaderAL::SetName( const char* name )
{
	m_name = name ? name : "";
	if( m_device && m_module )
	{
		m_device->SetObjectName( VK_OBJECT_TYPE_SHADER_MODULE, uint64_t( m_module ), name );
	}
	return S_OK;
}

}

#endif

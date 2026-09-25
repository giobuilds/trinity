// Copyright © 2023 CCP ehf.

#pragma once

#if TRINITY_PLATFORM == TRINITY_VULKAN

#include "../include/Tr2ShaderAL.h"
#include "VulkanSpirv.h"

#include <memory>
#include <string>


namespace TrinityALImpl
{
class VulkanDevice;

// -------------------------------------------------------------
// Description:
//   A low level wrapper around shaders / shader programs.
//   32bit - no support for shader blobs > 4 gig
// -------------------------------------------------------------
class Tr2ShaderAL : public Tr2DeviceResourceAL<Tr2ShaderAL>
{
public:
	Tr2ShaderAL();
	~Tr2ShaderAL();

	ALResult Create(
		Tr2RenderContextEnum::ShaderType type,
		const Tr2ShaderBytecodeAL& bytecode,
		const Tr2ShaderSignatureAL& signature,
		const char* shaderPath,
		Tr2PrimaryRenderContextAL& renderContext );

	void Destroy();

	bool IsValid() const;
	Tr2RenderContextEnum::ShaderType GetType() const;
	ALResult GetBytecode( Tr2ShaderBytecodeAL& bytecode ) const;
	const Tr2ShaderSignatureAL& GetSignature() const;

	Tr2ALMemoryType GetMemoryClass() const
	{
		return AL_MEMORY_MANAGED;
	}

	void SetNullShaderType( Tr2RenderContextEnum::ShaderType type );
	void Describe( Tr2DeviceResourceDescriptionAL& description ) const;
	ALResult SetName( const char* name );

	// SPIR-V with bindings moved to this stage's range (see SpirvBinding), and what it declares.
	VkShaderModule GetModule() const
	{
		return m_module;
	}
	const SpirvReflection& GetReflection() const
	{
		return m_reflection;
	}
	const std::string& GetName() const
	{
		return m_name;
	}

private:
	Tr2RenderContextEnum::ShaderType m_type;
	CcpMallocBuffer m_bytecode; // as given, for GetBytecode
	Tr2ShaderSignatureAL m_signature;
	std::shared_ptr<VulkanDevice> m_device;
	VkShaderModule m_module = VK_NULL_HANDLE;
	SpirvReflection m_reflection;
	std::string m_name;
};
}

#endif

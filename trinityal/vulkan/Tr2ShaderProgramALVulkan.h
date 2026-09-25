// Copyright © 2023 CCP ehf.

#pragma once

#if TRINITY_PLATFORM == TRINITY_VULKAN

#include "../include/Tr2ShaderProgramAL.h"
#include "../include/Tr2ResourceSetAL.h"
#include "../include/Tr2ShaderAL.h"
#include "VulkanIncludes.h"
#include "VulkanPipelineState.h"
#include "VulkanSpirv.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace TrinityALImpl
{
class VulkanDevice;
class Tr2VertexLayoutAL;

// The shaders of one pipeline and their binding interface: one descriptor set (set 0) holding every stage's registers
// at stage-unique bindings (SpirvBinding), static samplers as immutable samplers, and the pipelines built from them.
// Graphics pipelines depend on draw state too; they are created on first use and cached here, so they go away with the
// program.
class Tr2ShaderProgramAL : public Tr2DeviceResourceAL<Tr2ShaderProgramAL>
{
public:
	// One descriptor binding of the set, and which D3D register (stage, class, index) fills it.
	struct Binding
	{
		uint32_t binding;
		VkDescriptorType type;
		uint32_t count;
		Tr2RenderContextEnum::ShaderType stage;
		SpirvBinding::Class registerClass;
		uint32_t registerIndex;
		VkImageViewType viewType; // images
		bool immutableSampler; // static sampler: nothing to write
	};

	Tr2ShaderProgramAL();
	~Tr2ShaderProgramAL();

	ALResult Create( ::Tr2ShaderAL* shaders, size_t count, Tr2PrimaryRenderContextAL& renderContext );
	void Destroy();
	bool IsValid() const;
	const Tr2RegisterMapAL& GetRegisterMap() const;
	Tr2ALMemoryType GetMemoryClass() const;
	void Describe( Tr2DeviceResourceDescriptionAL& description ) const;
	ALResult SetName( const char* name );

	bool IsCompute() const
	{
		return m_isCompute;
	}
	VkPipelineLayout GetPipelineLayout() const
	{
		return m_pipelineLayout;
	}
	VkDescriptorSetLayout GetSetLayout() const
	{
		return m_setLayout;
	}
	const std::vector<Binding>& GetBindings() const
	{
		return m_bindings;
	}
	VkPipeline GetComputePipeline() const
	{
		return m_computePipeline;
	}
	// Creates the pipeline on first use; VK_NULL_HANDLE if it cannot be built.
	VkPipeline GetGraphicsPipeline( const GraphicsPipelineState& state, const Tr2VertexLayoutAL* layout );

private:
	VkPipeline CreateGraphicsPipeline( const GraphicsPipelineState& state, const Tr2VertexLayoutAL* layout );

	std::shared_ptr<VulkanDevice> m_device;
	std::vector<::Tr2ShaderAL> m_shaders;
	Tr2RegisterMapAL m_registerMap;
	std::vector<Binding> m_bindings;
	std::vector<VkSampler> m_staticSamplers;
	VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
	VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
	VkPipeline m_computePipeline = VK_NULL_HANDLE;
	std::unordered_map<GraphicsPipelineState, VkPipeline, GraphicsPipelineStateHash> m_graphicsPipelines;
	bool m_isCompute = false;
	std::string m_name;

	friend class Tr2RenderContextAL;
};
}

#endif

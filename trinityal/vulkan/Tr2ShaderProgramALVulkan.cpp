// Copyright © 2023 CCP ehf.

#include "StdAfx.h"

#if TRINITY_PLATFORM == TRINITY_VULKAN

#include "Tr2ShaderProgramALVulkan.h"
#include "Tr2ShaderALVulkan.h"
#include "Tr2VertexLayoutALVulkan.h"
#include "Tr2PrimaryRenderContextVulkan.h"
#include "VulkanDevice.h"
#include "VulkanStates.h"
#include "ALLog.h"

#include <algorithm>

using namespace Tr2RenderContextEnum;

namespace
{

VkShaderStageFlagBits StageFlag( ShaderType type )
{
	switch( type )
	{
	case VERTEX_SHADER:
		return VK_SHADER_STAGE_VERTEX_BIT;
	case PIXEL_SHADER:
		return VK_SHADER_STAGE_FRAGMENT_BIT;
	case COMPUTE_SHADER:
		return VK_SHADER_STAGE_COMPUTE_BIT;
	case GEOMETRY_SHADER:
		return VK_SHADER_STAGE_GEOMETRY_BIT;
	case HULL_SHADER:
		return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
	case DOMAIN_SHADER:
		return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
	default:
		return VK_SHADER_STAGE_ALL;
	}
}

VkFormat MissingInputFormat( TrinityALImpl::SpirvNumericType type )
{
	switch( type )
	{
	case TrinityALImpl::SpirvNumericType::SINT:
		return VK_FORMAT_R32G32B32A32_SINT;
	case TrinityALImpl::SpirvNumericType::UINT:
		return VK_FORMAT_R32G32B32A32_UINT;
	default:
		return VK_FORMAT_R32G32B32A32_SFLOAT;
	}
}

}

namespace TrinityALImpl
{

Tr2ShaderProgramAL::Tr2ShaderProgramAL()
{
}

Tr2ShaderProgramAL::~Tr2ShaderProgramAL()
{
	Destroy();
}

ALResult Tr2ShaderProgramAL::Create( ::Tr2ShaderAL* shaders, size_t count, Tr2PrimaryRenderContextAL& renderContext )
{
	Destroy();

	if( !renderContext.IsValid() )
	{
		return E_INVALIDCALL;
	}
	if( count == 0 )
	{
		return E_INVALIDARG;
	}
	uint32_t mask = 0;
	for( size_t i = 0; i < count; ++i )
	{
		if( !shaders[i].IsValid() )
		{
			return E_INVALIDARG;
		}
		uint32_t bit = 1 << shaders[i].GetType();
		if( ( mask & bit ) != 0 )
		{
			return E_INVALIDARG;
		}
		mask |= bit;
	}
	const uint32_t csBit = 1 << COMPUTE_SHADER;
	if( ( mask & csBit ) != 0 && ( mask & ~csBit ) != 0 )
	{
		return E_INVALIDARG;
	}

	auto device = renderContext.GetVulkanDeviceShared();
	m_device = device;
	m_isCompute = ( mask & csBit ) != 0;
	m_shaders.assign( shaders, shaders + count );
	m_registerMap = Tr2RegisterMapAL( shaders, count );

	// The descriptor set: every stage's bindings are already stage-unique (the shaders were remapped at load).
	std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
	std::vector<uint32_t> immutableSamplerIndex; // per layout binding, index into m_staticSamplers or ~0u
	for( size_t i = 0; i < count; ++i )
	{
		auto shader = shaders[i].m_shader.get();
		const ShaderType stage = shader->GetType();
		const auto& signature = shader->GetSignature();
		for( const SpirvResource& resource : shader->GetReflection().resources )
		{
			if( resource.count == 0 )
			{
				CCP_AL_LOGERR( "Vulkan: %s uses an unbounded descriptor array (binding %u); bindless resources are not supported", shader->GetName().c_str(), resource.binding );
				Destroy();
				return E_INVALIDARG;
			}
			if( resource.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER )
			{
				CCP_AL_LOGERR( "Vulkan: %s uses a combined image sampler (binding %u), which HLSL does not produce", shader->GetName().c_str(), resource.binding );
				Destroy();
				return E_INVALIDARG;
			}
			Binding binding{};
			binding.binding = resource.binding;
			binding.type = resource.type;
			binding.count = resource.count;
			binding.stage = stage;
			binding.registerClass = SpirvBinding::GetClass( resource.binding );
			binding.registerIndex = SpirvBinding::Register( resource.binding );
			binding.viewType = resource.viewType;

			VkDescriptorSetLayoutBinding layoutBinding{};
			layoutBinding.binding = resource.binding;
			layoutBinding.descriptorType = resource.type;
			layoutBinding.descriptorCount = resource.count;
			layoutBinding.stageFlags = StageFlag( stage );
			uint32_t samplerIndex = ~0u;
			if( resource.type == VK_DESCRIPTOR_TYPE_SAMPLER )
			{
				const uint32_t space = SpirvBinding::Space( resource.binding );
				for( auto& sampler : signature.samplers )
				{
					if( sampler.registerIndex == binding.registerIndex && sampler.registerSpace == space )
					{
						VkSampler vkSampler = CreateVkSampler( *device, sampler.sampler );
						if( !vkSampler )
						{
							Destroy();
							return E_FAIL;
						}
						samplerIndex = uint32_t( m_staticSamplers.size() );
						m_staticSamplers.push_back( vkSampler );
						binding.immutableSampler = true;
						break;
					}
				}
			}
			m_bindings.push_back( binding );
			layoutBindings.push_back( layoutBinding );
			immutableSamplerIndex.push_back( samplerIndex );
		}
	}
	for( size_t i = 0; i < layoutBindings.size(); ++i )
	{
		if( immutableSamplerIndex[i] != ~0u )
		{
			layoutBindings[i].pImmutableSamplers = &m_staticSamplers[immutableSamplerIndex[i]];
		}
	}

	VkDescriptorSetLayoutCreateInfo setInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	setInfo.bindingCount = uint32_t( layoutBindings.size() );
	setInfo.pBindings = layoutBindings.data();
	VkResult result = vkCreateDescriptorSetLayout( device->GetHandle(), &setInfo, nullptr, &m_setLayout );
	if( result != VK_SUCCESS )
	{
		CCP_AL_LOGERR( "Vulkan: vkCreateDescriptorSetLayout failed: %s", VkResultToString( result ) );
		m_setLayout = VK_NULL_HANDLE;
		Destroy();
		return E_FAIL;
	}
	VkPipelineLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &m_setLayout;
	result = vkCreatePipelineLayout( device->GetHandle(), &layoutInfo, nullptr, &m_pipelineLayout );
	if( result != VK_SUCCESS )
	{
		CCP_AL_LOGERR( "Vulkan: vkCreatePipelineLayout failed: %s", VkResultToString( result ) );
		m_pipelineLayout = VK_NULL_HANDLE;
		Destroy();
		return E_FAIL;
	}

	if( m_isCompute )
	{
		VkComputePipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
		pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		pipelineInfo.stage.module = shaders[0].m_shader->GetModule();
		pipelineInfo.stage.pName = "main";
		pipelineInfo.layout = m_pipelineLayout;
		result = vkCreateComputePipelines( device->GetHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_computePipeline );
		if( result != VK_SUCCESS )
		{
			CCP_AL_LOGERR( "Vulkan: vkCreateComputePipelines failed for %s: %s", shaders[0].m_shader->GetName().c_str(), VkResultToString( result ) );
			m_computePipeline = VK_NULL_HANDLE;
			Destroy();
			return E_FAIL;
		}
	}

	for( auto& shader : m_shaders )
	{
		auto& name = shader.m_shader->GetName();
		if( !name.empty() )
		{
			m_name += m_name.empty() ? name : " + " + name;
		}
	}
	return S_OK;
}

VkPipeline Tr2ShaderProgramAL::GetGraphicsPipeline( const GraphicsPipelineState& state, const Tr2VertexLayoutAL* layout )
{
	auto found = m_graphicsPipelines.find( state );
	if( found != m_graphicsPipelines.end() )
	{
		return found->second;
	}
	VkPipeline pipeline = CreateGraphicsPipeline( state, layout );
	if( pipeline )
	{
		m_graphicsPipelines[state] = pipeline;
	}
	return pipeline;
}

VkPipeline Tr2ShaderProgramAL::CreateGraphicsPipeline( const GraphicsPipelineState& state, const Tr2VertexLayoutAL* layout )
{
	if( m_isCompute || !m_pipelineLayout )
	{
		return VK_NULL_HANDLE;
	}

	std::vector<VkPipelineShaderStageCreateInfo> stages;
	const TrinityALImpl::Tr2ShaderAL* vertexShader = nullptr;
	for( auto& shader : m_shaders )
	{
		VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		stage.stage = StageFlag( shader.GetType() );
		stage.module = shader.m_shader->GetModule();
		stage.pName = "main";
		stages.push_back( stage );
		if( shader.GetType() == VERTEX_SHADER )
		{
			vertexShader = shader.m_shader.get();
		}
		if( shader.GetType() == HULL_SHADER || shader.GetType() == DOMAIN_SHADER )
		{
			CCP_AL_LOGERR( "Vulkan: tessellation shaders are not supported yet (%s)", m_name.c_str() );
			return VK_NULL_HANDLE;
		}
	}
	if( !vertexShader )
	{
		CCP_AL_LOGERR( "Vulkan: graphics program without a vertex shader (%s)", m_name.c_str() );
		return VK_NULL_HANDLE;
	}

	// Vertex input: every shader input is matched by semantic to the layout; inputs the layout lacks read zeros from
	// the zero stream (binding MAX_STREAMS), as D3D's missing input-assembler elements do. Strides are dynamic.
	const uint32_t zeroStream = Tr2VertexLayoutAL::MAX_STREAMS;
	std::vector<VkVertexInputAttributeDescription> attributes;
	std::vector<VkVertexInputBindingDescription> bindings;
	uint32_t usedStreams = 0;
	for( const SpirvVertexInput& input : vertexShader->GetReflection().vertexInputs )
	{
		VkVertexInputAttributeDescription attribute{};
		attribute.location = input.location;
		const Tr2VertexLayoutAL::Element* element = layout ? layout->Find( input.semantic, input.semanticIndex ) : nullptr;
		if( element )
		{
			attribute.binding = element->stream;
			attribute.format = element->format;
			attribute.offset = element->offset;
		}
		else
		{
			attribute.binding = zeroStream;
			attribute.format = MissingInputFormat( input.numericType );
			attribute.offset = 0;
		}
		usedStreams |= 1u << attribute.binding;
		attributes.push_back( attribute );
	}
	for( uint32_t stream = 0; stream <= zeroStream; ++stream )
	{
		if( usedStreams & ( 1u << stream ) )
		{
			VkVertexInputBindingDescription binding{};
			binding.binding = stream;
			binding.stride = 0; // dynamic
			binding.inputRate = stream < zeroStream && layout->IsInstanceStream( stream ) ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
			bindings.push_back( binding );
			if( stream < zeroStream && layout->IsInstanceStream( stream ) && layout->GetStepRate( stream ) > 1 )
			{
				CCP_AL_LOGWARN( "Vulkan: instance step rate %u on stream %u is treated as 1", layout->GetStepRate( stream ), stream );
			}
		}
	}
	VkPipelineVertexInputStateCreateInfo vertexInput{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vertexInput.vertexBindingDescriptionCount = uint32_t( bindings.size() );
	vertexInput.pVertexBindingDescriptions = bindings.data();
	vertexInput.vertexAttributeDescriptionCount = uint32_t( attributes.size() );
	vertexInput.pVertexAttributeDescriptions = attributes.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	inputAssembly.topology = VkPrimitiveTopology( state.topology );

	VkPipelineViewportStateCreateInfo viewport{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewport.viewportCount = 1;
	viewport.scissorCount = 1;

	// The render context flips Y with a negative viewport height, so D3D's clockwise front faces stay clockwise.
	VkPipelineRasterizationStateCreateInfo raster{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	raster.depthClampEnable = !state.depthClipEnable && m_device->GetEnabledFeatures().depthClamp;
	raster.polygonMode = VK_POLYGON_MODE_FILL;
	if( m_device->GetEnabledFeatures().fillModeNonSolid )
	{
		if( state.fillMode == FM_WIREFRAME )
		{
			raster.polygonMode = VK_POLYGON_MODE_LINE;
		}
		else if( state.fillMode == FM_POINT )
		{
			raster.polygonMode = VK_POLYGON_MODE_POINT;
		}
	}
	// D3D9 names the winding to cull: clockwise is D3D's front face.
	raster.cullMode = VK_CULL_MODE_NONE;
	if( state.cullMode == CULLMODE_CW )
	{
		raster.cullMode = VK_CULL_MODE_FRONT_BIT;
	}
	else if( state.cullMode == CULLMODE_CCW )
	{
		raster.cullMode = VK_CULL_MODE_BACK_BIT;
	}
	raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
	raster.depthBiasEnable = state.depthBias != 0 || state.slopeScaledDepthBias != 0.0f;
	raster.depthBiasConstantFactor = float( state.depthBias );
	raster.depthBiasSlopeFactor = state.slopeScaledDepthBias;
	raster.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisample{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	multisample.rasterizationSamples = VkSampleCountFlagBits( state.samples ? state.samples : 1 );

	VkStencilOpState stencilOp{};
	stencilOp.failOp = ToVkStencilOp( state.stencilFailOp );
	stencilOp.passOp = ToVkStencilOp( state.stencilPassOp );
	stencilOp.depthFailOp = ToVkStencilOp( state.stencilDepthFailOp );
	stencilOp.compareOp = ToVkCompareOp( state.stencilFunc );
	stencilOp.compareMask = state.stencilReadMask;
	stencilOp.writeMask = state.stencilWriteMask;
	VkPipelineDepthStencilStateCreateInfo depthStencil{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	const bool hasDepth = state.depthFormat != VK_FORMAT_UNDEFINED;
	const bool hasStencil = state.stencilFormat != VK_FORMAT_UNDEFINED;
	depthStencil.depthTestEnable = hasDepth && state.depthEnable;
	depthStencil.depthWriteEnable = hasDepth && state.depthEnable && state.depthWriteEnable;
	depthStencil.depthCompareOp = ToVkCompareOp( state.depthFunc );
	depthStencil.stencilTestEnable = hasStencil && state.stencilEnable;
	depthStencil.front = stencilOp;
	depthStencil.back = stencilOp;
	depthStencil.maxDepthBounds = 1.0f;

	VkPipelineColorBlendAttachmentState blend{};
	blend.blendEnable = state.blendEnable;
	blend.srcColorBlendFactor = ToVkBlendFactor( state.srcBlend );
	blend.dstColorBlendFactor = ToVkBlendFactor( state.destBlend );
	blend.colorBlendOp = ToVkBlendOp( state.blendOp );
	blend.srcAlphaBlendFactor = ToVkBlendFactor( state.srcBlendAlpha );
	blend.dstAlphaBlendFactor = ToVkBlendFactor( state.destBlendAlpha );
	blend.alphaBlendOp = ToVkBlendOp( state.blendOpAlpha );
	blend.colorWriteMask = state.colorWriteMask & 0xf; // D3D's red/green/blue/alpha bits match Vulkan's R/G/B/A
	std::vector<VkPipelineColorBlendAttachmentState> blends( state.colorAttachmentCount, blend );
	VkPipelineColorBlendStateCreateInfo colorBlend{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	colorBlend.attachmentCount = uint32_t( blends.size() );
	colorBlend.pAttachments = blends.data();

	const VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_STENCIL_REFERENCE,
		VK_DYNAMIC_STATE_BLEND_CONSTANTS,
		VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE,
	};
	VkPipelineDynamicStateCreateInfo dynamic{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamic.dynamicStateCount = uint32_t( sizeof( dynamicStates ) / sizeof( dynamicStates[0] ) );
	dynamic.pDynamicStates = dynamicStates;

	VkFormat colorFormats[8];
	for( uint32_t i = 0; i < state.colorAttachmentCount; ++i )
	{
		colorFormats[i] = VkFormat( state.colorFormats[i] );
	}
	VkPipelineRenderingCreateInfo rendering{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	rendering.colorAttachmentCount = state.colorAttachmentCount;
	rendering.pColorAttachmentFormats = colorFormats;
	rendering.depthAttachmentFormat = VkFormat( state.depthFormat );
	rendering.stencilAttachmentFormat = VkFormat( state.stencilFormat );

	VkGraphicsPipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipelineInfo.pNext = &rendering;
	pipelineInfo.stageCount = uint32_t( stages.size() );
	pipelineInfo.pStages = stages.data();
	pipelineInfo.pVertexInputState = &vertexInput;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewport;
	pipelineInfo.pRasterizationState = &raster;
	pipelineInfo.pMultisampleState = &multisample;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlend;
	pipelineInfo.pDynamicState = &dynamic;
	pipelineInfo.layout = m_pipelineLayout;

	VkPipeline pipeline = VK_NULL_HANDLE;
	VkResult result = vkCreateGraphicsPipelines( m_device->GetHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline );
	if( result != VK_SUCCESS )
	{
		CCP_AL_LOGERR( "Vulkan: vkCreateGraphicsPipelines failed for %s: %s", m_name.c_str(), VkResultToString( result ) );
		return VK_NULL_HANDLE;
	}
	return pipeline;
}

void Tr2ShaderProgramAL::Destroy()
{
	if( m_device )
	{
		VkDevice device = m_device->GetHandle();
		std::vector<VkPipeline> pipelines;
		for( auto& entry : m_graphicsPipelines )
		{
			pipelines.push_back( entry.second );
		}
		if( m_computePipeline )
		{
			pipelines.push_back( m_computePipeline );
		}
		VkPipelineLayout pipelineLayout = m_pipelineLayout;
		VkDescriptorSetLayout setLayout = m_setLayout;
		std::vector<VkSampler> samplers = m_staticSamplers;
		m_device->ReleaseLater( [device, pipelines, pipelineLayout, setLayout, samplers] {
			for( auto pipeline : pipelines )
			{
				vkDestroyPipeline( device, pipeline, nullptr );
			}
			if( pipelineLayout )
			{
				vkDestroyPipelineLayout( device, pipelineLayout, nullptr );
			}
			if( setLayout )
			{
				vkDestroyDescriptorSetLayout( device, setLayout, nullptr );
			}
			for( auto sampler : samplers )
			{
				vkDestroySampler( device, sampler, nullptr );
			}
		} );
	}
	m_graphicsPipelines.clear();
	m_computePipeline = VK_NULL_HANDLE;
	m_pipelineLayout = VK_NULL_HANDLE;
	m_setLayout = VK_NULL_HANDLE;
	m_staticSamplers.clear();
	m_bindings.clear();
	m_shaders.clear();
	m_registerMap = Tr2RegisterMapAL();
	m_isCompute = false;
	m_name.clear();
	m_device.reset();
}

bool Tr2ShaderProgramAL::IsValid() const
{
	return m_pipelineLayout != VK_NULL_HANDLE;
}

Tr2ALMemoryType Tr2ShaderProgramAL::GetMemoryClass() const
{
	return AL_MEMORY_MANAGED;
}

void Tr2ShaderProgramAL::Describe( Tr2DeviceResourceDescriptionAL& ) const
{
}

const Tr2RegisterMapAL& Tr2ShaderProgramAL::GetRegisterMap() const
{
	return m_registerMap;
}

ALResult Tr2ShaderProgramAL::SetName( const char* name )
{
	m_name = name ? name : "";
	if( m_device && m_pipelineLayout )
	{
		m_device->SetObjectName( VK_OBJECT_TYPE_PIPELINE_LAYOUT, uint64_t( m_pipelineLayout ), name );
	}
	return S_OK;
}
}

#endif

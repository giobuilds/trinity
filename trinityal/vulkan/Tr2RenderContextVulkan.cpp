// Copyright © 2023 CCP ehf.

#include "StdAfx.h"

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "Tr2RenderContextVulkan.h"
#include "ITr2RenderContextEvents.h"
#include "ALLog.h"
#include "Tr2AdapterStructures.h"
#include "VulkanDevice.h"
#include "VulkanFormats.h"
#include "VulkanStates.h"
#include "Tr2BufferALVulkan.h"
#include "Tr2ConstantBufferALVulkan.h"
#include "Tr2ResourceSetALVulkan.h"
#include "Tr2ShaderProgramALVulkan.h"
#include "Tr2TextureALVulkan.h"
#include "Tr2VertexLayoutALVulkan.h"

#include <algorithm>


CCP_STATS_DECLARE( vertexCount, "Trinity/AL/vertexCount", true, CST_COUNTER_HIGH, "Vertex count in DrawPrimitive calls." );


using namespace Tr2RenderContextEnum;
#pragma warning( disable : 4189 ) // Scopeguard

bool g_gatherPipelineStatistics = false;

namespace
{

Tr2PrimaryRenderContextAL*& GetPrimaryRenderContextPointer()
{
	static Tr2PrimaryRenderContextAL* primaryRenderContext = nullptr;
	return primaryRenderContext;
}

// Per Topology: the Vulkan topology, and vertices = first * primitives + second.
const VkPrimitiveTopology s_topologies[] = {
	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // TOP_INVALID
	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
	VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
	VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
	VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
};
const std::pair<uint32_t, uint32_t> s_primitiveToVertexCount[] = {
	{ 0, 0 },
	{ 3, 0 },
	{ 1, 2 },
	{ 1, 2 },
	{ 2, 0 },
	{ 1, 1 },
	{ 1, 0 },
};

bool IsUavDescriptor( VkDescriptorType type )
{
	return type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER || type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
}

}



Tr2RenderContextAL::Tr2RenderContextAL() :
	m_isValid( false ),
	m_viewport( 0, 0 ),
	m_frameNumber( 0 ),
	m_indexType( VK_INDEX_TYPE_UINT16 ),
	m_topology( TOP_TRIANGLES ),
	m_separateAlphaBlend( false ),
	m_srgbWrite( false ),
	m_readOnlyDepth( false ),
	m_stencilRef( 0 ),
	m_commandBufferSerial( ~0ull ),
	m_rendering( false ),
	m_renderExtent{ 0, 0 },
	m_boundPipeline( VK_NULL_HANDLE ),
	m_descriptorsDirty( true ),
	m_passWritesUavs( false ),
	m_defaultSampler( VK_NULL_HANDLE ),
	m_events( nullptr )
{
	::GetPrimaryRenderContextPointer() = this;
	std::fill( std::begin( m_boundSlice ), std::end( m_boundSlice ), 0u );
	std::fill( std::begin( m_blendFactor ), std::end( m_blendFactor ), 1.0f );

	// D3D12's default pipeline state (see dx12/util/PsoDescription.cpp).
	m_pipelineState.cullMode = CULLMODE_CCW;
	m_pipelineState.fillMode = FM_SOLID;
	m_pipelineState.depthClipEnable = 1;
	m_pipelineState.depthEnable = 1;
	m_pipelineState.depthWriteEnable = 1;
	m_pipelineState.depthFunc = CMP_LESS;
	m_pipelineState.stencilReadMask = 0xff;
	m_pipelineState.stencilWriteMask = 0xff;
	m_pipelineState.stencilFailOp = STENCILOP_KEEP;
	m_pipelineState.stencilDepthFailOp = STENCILOP_KEEP;
	m_pipelineState.stencilPassOp = STENCILOP_KEEP;
	m_pipelineState.stencilFunc = CMP_ALWAYS;
	m_pipelineState.srcBlend = BM_ONE;
	m_pipelineState.destBlend = BM_ZERO;
	m_pipelineState.blendOp = BO_ADD;
	m_pipelineState.srcBlendAlpha = BM_ONE;
	m_pipelineState.destBlendAlpha = BM_ZERO;
	m_pipelineState.blendOpAlpha = BO_ADD;
	m_pipelineState.colorWriteMask = 0xf;
}

Tr2RenderContextAL::~Tr2RenderContextAL()
{
	Destroy();
}

void Tr2RenderContextAL::SetPrimaryRenderContext( Tr2PrimaryRenderContextAL* renderContext )
{
	::GetPrimaryRenderContextPointer() = renderContext;
}

Tr2PrimaryRenderContextAL& Tr2RenderContextAL::GetPrimaryRenderContext()
{
	CCP_ASSERT( GetPrimaryRenderContextPointer() );
	return *GetPrimaryRenderContextPointer();
}

Tr2PrimaryRenderContextAL* Tr2RenderContextAL::GetPrimaryRenderContextPointer()
{
	return ::GetPrimaryRenderContextPointer();
}

void Tr2RenderContextAL::Destroy()
{
	for( unsigned i = 0; i != MAX_RENDER_TARGET; ++i )
	{
		m_boundRenderTarget[i] = Tr2TextureAL();
		while( !m_stackRT[i].empty() )
		{
			m_stackRT[i].pop();
		}
	}
	while( !m_stackDS.empty() )
	{
		m_stackDS.pop();
	}
	m_boundDepthStencil = Tr2TextureAL();
	m_defaultBackBuffer = Tr2TextureAL();
	for( auto& stream : m_streams )
	{
		stream = VertexStream();
	}
	m_indexBuffer = Tr2BufferAL();
	m_vertexLayout = Tr2VertexLayoutAL();
	m_program = Tr2ShaderProgramAL();
	m_resourceSet = Tr2ResourceSetAL();
	for( auto& stage : m_constants )
	{
		for( auto& buffer : stage )
		{
			buffer = Tr2ConstantBufferAL();
		}
	}
	m_constantUploads.clear();
	if( m_device )
	{
		m_device->Submit();
		m_device->SetRenderingEndHook( nullptr );
		m_device->WaitIdle();
		if( m_defaultSampler )
		{
			vkDestroySampler( m_device->GetHandle(), m_defaultSampler, nullptr );
		}
	}
	m_defaultSampler = VK_NULL_HANDLE;
	m_rendering = false;
	m_boundPipeline = VK_NULL_HANDLE;
	m_commandBufferSerial = ~0ull;
	m_device.reset();
	m_isValid = false;
}

// ------------------------------------------------------------------------------------------------------------------
// Recording helpers

void Tr2RenderContextAL::OnNewCommandBufferVulkan()
{
	// Everything bound to the previous command buffer has to be bound again.
	m_commandBufferSerial = m_device->GetSubmittedFrameCount();
	m_boundPipeline = VK_NULL_HANDLE;
	m_descriptorsDirty = true;
	m_constantUploads.clear();
	m_rendering = false;
	m_passWritesUavs = false;
}

void Tr2RenderContextAL::EndRenderingVulkan( bool barrier )
{
	if( !m_rendering )
	{
		return;
	}
	m_rendering = false;
	m_passWritesUavs = false;
	vkCmdEndRendering( m_device->GetCommandBuffer() );
	if( barrier )
	{
		// What the pass wrote is visible to whatever comes next (sampling, copies, the next pass).
		m_device->RecordFullBarrier();
	}
}

ALResult Tr2RenderContextAL::BeginRenderingVulkan()
{
	VkCommandBuffer commandBuffer = m_device->GetCommandBuffer();
	if( m_commandBufferSerial != m_device->GetSubmittedFrameCount() )
	{
		OnNewCommandBufferVulkan();
	}
	if( m_rendering )
	{
		return S_OK;
	}

	VkRenderingAttachmentInfo colors[MAX_RENDER_TARGET]{};
	uint32_t colorCount = 0;
	uint32_t width = ~0u, height = ~0u, samples = 0;
	for( uint32_t i = 0; i < MAX_RENDER_TARGET; ++i )
	{
		colors[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		colors[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		colors[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		colors[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		m_pipelineState.colorFormats[i] = VK_FORMAT_UNDEFINED;
		auto texture = m_boundRenderTarget[i].IsValid() ? m_boundRenderTarget[i].m_texture.get() : nullptr;
		if( !texture )
		{
			continue;
		}
		colors[i].imageView = texture->GetAttachmentView( m_boundSlice[i], m_srgbWrite );
		if( !colors[i].imageView )
		{
			return E_FAIL;
		}
		m_pipelineState.colorFormats[i] = m_srgbWrite && TrinityALImpl::ToVkFormat( MakeSrgb( texture->GetDesc().GetFormat() ) ) != VK_FORMAT_UNDEFINED ? TrinityALImpl::ToVkFormat( MakeSrgb( texture->GetDesc().GetFormat() ) ) : texture->GetVkFormat();
		colorCount = i + 1;
		width = std::min( width, texture->GetDesc().GetWidth() );
		height = std::min( height, texture->GetDesc().GetHeight() );
		samples = std::max( samples, texture->GetMsaaDesc().samples );
	}

	VkRenderingAttachmentInfo depth{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
	VkRenderingAttachmentInfo stencil{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
	m_pipelineState.depthFormat = VK_FORMAT_UNDEFINED;
	m_pipelineState.stencilFormat = VK_FORMAT_UNDEFINED;
	auto depthTexture = m_boundDepthStencil.IsValid() ? m_boundDepthStencil.m_texture.get() : nullptr;
	if( depthTexture )
	{
		VkImageView view = depthTexture->GetAttachmentView( 0, false );
		if( !view )
		{
			return E_FAIL;
		}
		for( auto* attachment : { &depth, &stencil } )
		{
			attachment->imageView = view;
			attachment->imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			attachment->loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachment->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		}
		if( depthTexture->GetAspectMask() & VK_IMAGE_ASPECT_DEPTH_BIT )
		{
			m_pipelineState.depthFormat = depthTexture->GetVkFormat();
		}
		if( depthTexture->GetAspectMask() & VK_IMAGE_ASPECT_STENCIL_BIT )
		{
			m_pipelineState.stencilFormat = depthTexture->GetVkFormat();
		}
		width = std::min( width, depthTexture->GetDesc().GetWidth() );
		height = std::min( height, depthTexture->GetDesc().GetHeight() );
		samples = std::max( samples, depthTexture->GetMsaaDesc().samples );
	}
	if( width == ~0u )
	{
		// No attachments (e.g. a pass that only writes UAVs): the viewport sets the size.
		width = std::max( uint32_t( m_viewport.m_x + m_viewport.m_width ), 1u );
		height = std::max( uint32_t( m_viewport.m_y + m_viewport.m_height ), 1u );
	}
	m_pipelineState.colorAttachmentCount = colorCount;
	m_pipelineState.samples = std::max( samples, 1u );
	m_renderExtent = { width, height };

	VkRenderingInfo info{ VK_STRUCTURE_TYPE_RENDERING_INFO };
	info.renderArea = { { 0, 0 }, m_renderExtent };
	info.layerCount = 1;
	info.colorAttachmentCount = colorCount;
	info.pColorAttachments = colors;
	info.pDepthAttachment = m_pipelineState.depthFormat != VK_FORMAT_UNDEFINED ? &depth : nullptr;
	info.pStencilAttachment = m_pipelineState.stencilFormat != VK_FORMAT_UNDEFINED ? &stencil : nullptr;
	vkCmdBeginRendering( commandBuffer, &info );
	m_rendering = true;
	return S_OK;
}

uint32_t Tr2RenderContextAL::PrimitiveVertexCount( uint32_t primitiveCount ) const
{
	auto& counts = s_primitiveToVertexCount[m_topology < TOP_MAX_TOPOLOGY ? m_topology : 0];
	return counts.first * primitiveCount + counts.second;
}

bool Tr2RenderContextAL::DescriptorsChangedVulkan() const
{
	if( m_descriptorsDirty )
	{
		return true;
	}
	// Constant buffers can change contents without being bound again.
	size_t i = 0;
	for( auto& binding : m_program.m_program->GetBindings() )
	{
		if( binding.registerClass != TrinityALImpl::SpirvBinding::CONSTANT_BUFFER )
		{
			continue;
		}
		auto& buffer = m_constants[binding.stage][binding.registerIndex];
		uint64_t version = buffer.IsValid() ? buffer.m_buffer->GetVersion() : ~0ull;
		if( i >= m_boundConstantVersions.size() || m_boundConstantVersions[i] != version )
		{
			return true;
		}
		++i;
	}
	return false;
}

bool Tr2RenderContextAL::WriteDescriptorsVulkan( VkPipelineBindPoint bindPoint )
{
	auto program = m_program.m_program.get();
	const auto& bindings = program->GetBindings();
	if( bindings.empty() )
	{
		return true;
	}
	VkDescriptorSet set = m_device->AllocateDescriptorSet( program->GetSetLayout() );
	if( !set )
	{
		return false;
	}
	if( !m_defaultSampler )
	{
		// Unassigned sampler slots (samplers have no null descriptor).
		m_defaultSampler = TrinityALImpl::CreateVkSampler( *m_device, Tr2SamplerDescription() );
	}

	auto resourceSet = m_resourceSet.IsValid() ? m_resourceSet.m_resourceSet.get() : nullptr;
	std::vector<VkWriteDescriptorSet> writes;
	std::vector<VkDescriptorImageInfo> images;
	std::vector<VkDescriptorBufferInfo> buffers;
	std::vector<VkBufferView> texelBuffers;
	size_t total = 0;
	for( auto& binding : bindings )
	{
		total += binding.count;
	}
	images.reserve( total );
	buffers.reserve( total );
	texelBuffers.reserve( total );
	m_boundConstantVersions.clear();

	const VkDeviceSize alignment = m_device->GetLimits().minUniformBufferOffsetAlignment;
	for( auto& binding : bindings )
	{
		if( binding.immutableSampler )
		{
			continue;
		}
		VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write.dstSet = set;
		write.dstBinding = binding.binding;
		write.descriptorCount = binding.count;
		write.descriptorType = binding.type;

		// Element 0 comes from the bound state; further array elements are null (see Tr2ResourceSetAL).
		VkDescriptorImageInfo image{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL };
		VkDescriptorBufferInfo buffer{ VK_NULL_HANDLE, 0, VK_WHOLE_SIZE };
		VkBufferView texelBuffer = VK_NULL_HANDLE;
		if( binding.registerClass == TrinityALImpl::SpirvBinding::CONSTANT_BUFFER )
		{
			auto& constants = m_constants[binding.stage][binding.registerIndex];
			m_boundConstantVersions.push_back( constants.IsValid() ? constants.m_buffer->GetVersion() : ~0ull );
			if( constants.IsValid() )
			{
				auto cb = constants.m_buffer.get();
				auto found = m_constantUploads.find( cb );
				if( found == m_constantUploads.end() || found->second.version != cb->GetVersion() )
				{
					TrinityALImpl::VulkanDevice::UploadAllocation upload;
					if( !m_device->AllocateUpload( cb->GetSize(), alignment, upload ) )
					{
						return false;
					}
					memcpy( upload.data, cb->GetData(), cb->GetSize() );
					found = m_constantUploads.insert_or_assign( cb, ConstantUpload{ cb->GetVersion(), upload.buffer, upload.offset } ).first;
				}
				buffer = { found->second.buffer, found->second.offset, cb->GetSize() };
			}
		}
		else if( auto descriptor = resourceSet ? resourceSet->Find( binding.binding ) : nullptr )
		{
			if( descriptor->type == binding.type )
			{
				image = descriptor->image;
				buffer = descriptor->buffer;
				texelBuffer = descriptor->texelBuffer;
				if( binding.type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || binding.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE )
				{
					image.imageView = TrinityALImpl::Tr2ResourceSetAL::GetImageView( *descriptor, binding.type, binding.viewType );
				}
			}
		}
		if( binding.type == VK_DESCRIPTOR_TYPE_SAMPLER && !image.sampler )
		{
			image.sampler = m_defaultSampler;
		}

		switch( binding.type )
		{
		case VK_DESCRIPTOR_TYPE_SAMPLER:
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			write.pImageInfo = images.data() + images.size();
			images.push_back( image );
			for( uint32_t i = 1; i < binding.count; ++i )
			{
				images.push_back( { binding.type == VK_DESCRIPTOR_TYPE_SAMPLER ? m_defaultSampler : VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL } );
			}
			break;
		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
			write.pTexelBufferView = texelBuffers.data() + texelBuffers.size();
			texelBuffers.push_back( texelBuffer );
			for( uint32_t i = 1; i < binding.count; ++i )
			{
				texelBuffers.push_back( VK_NULL_HANDLE );
			}
			break;
		default:
			write.pBufferInfo = buffers.data() + buffers.size();
			buffers.push_back( buffer );
			for( uint32_t i = 1; i < binding.count; ++i )
			{
				buffers.push_back( { VK_NULL_HANDLE, 0, VK_WHOLE_SIZE } );
			}
			break;
		}
		writes.push_back( write );
	}
	vkUpdateDescriptorSets( m_device->GetHandle(), uint32_t( writes.size() ), writes.data(), 0, nullptr );
	vkCmdBindDescriptorSets( m_device->GetCommandBuffer(), bindPoint, program->GetPipelineLayout(), 0, 1, &set, 0, nullptr );
	return true;
}

ALResult Tr2RenderContextAL::PrepareDrawVulkan( bool indexed )
{
	if( !m_device )
	{
		return E_FAIL;
	}
	if( !m_program.IsValid() || m_program.m_program->IsCompute() )
	{
		return E_INVALIDCALL;
	}
	CR_RETURN_HR( BeginRenderingVulkan() );
	VkCommandBuffer commandBuffer = m_device->GetCommandBuffer();
	auto program = m_program.m_program.get();
	auto layout = m_vertexLayout.IsValid() ? m_vertexLayout.m_layout.get() : nullptr;

	TrinityALImpl::GraphicsPipelineState state = m_pipelineState;
	state.vertexLayoutId = layout ? layout->GetId() : 0;
	state.topology = s_topologies[m_topology < TOP_MAX_TOPOLOGY ? m_topology : 0];
	if( m_readOnlyDepth )
	{
		state.depthWriteEnable = 0;
	}
	if( !m_separateAlphaBlend )
	{
		// As D3D9 and the DX12 backend: without separate alpha blending, alpha follows the colour factors.
		state.srcBlendAlpha = TrinityALImpl::AlphaBlendMode( state.srcBlend );
		state.destBlendAlpha = TrinityALImpl::AlphaBlendMode( state.destBlend );
		state.blendOpAlpha = state.blendOp;
	}
	if( !state.blendEnable )
	{
		state.srcBlend = state.destBlend = state.blendOp = state.srcBlendAlpha = state.destBlendAlpha = state.blendOpAlpha = 0;
	}
	VkPipeline pipeline = program->GetGraphicsPipeline( state, layout );
	if( !pipeline )
	{
		return E_FAIL;
	}
	if( pipeline != m_boundPipeline )
	{
		vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
		m_boundPipeline = pipeline;
	}

	// D3D viewports have y down and +y up in clip space; a negative height flips Vulkan's the same way.
	Tr2Viewport vp = m_viewport;
	if( vp.m_width <= 0 || vp.m_height <= 0 )
	{
		vp = Tr2Viewport( m_renderExtent.width, m_renderExtent.height );
	}
	VkViewport viewport{ vp.m_x, vp.m_y + vp.m_height, vp.m_width, -vp.m_height, vp.m_minZ, vp.m_maxZ };
	vkCmdSetViewport( commandBuffer, 0, 1, &viewport );
	VkRect2D scissor{ { 0, 0 }, m_renderExtent };
	vkCmdSetScissor( commandBuffer, 0, 1, &scissor );
	vkCmdSetStencilReference( commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, m_stencilRef );
	vkCmdSetBlendConstants( commandBuffer, m_blendFactor );

	// Streams 0-3 as set (or zeros), plus the zero stream for inputs the layout does not provide.
	VkBuffer vertexBuffers[MAX_VERTEX_STREAMS + 1];
	VkDeviceSize offsets[MAX_VERTEX_STREAMS + 1];
	VkDeviceSize strides[MAX_VERTEX_STREAMS + 1];
	for( uint32_t i = 0; i < MAX_VERTEX_STREAMS; ++i )
	{
		auto buffer = m_streams[i].buffer.IsValid() ? m_streams[i].buffer.m_buffer.get() : nullptr;
		vertexBuffers[i] = buffer ? buffer->GetVkBuffer() : m_device->GetZeroBuffer();
		offsets[i] = buffer ? m_streams[i].offset : 0;
		strides[i] = buffer ? m_streams[i].stride : 0;
	}
	vertexBuffers[MAX_VERTEX_STREAMS] = m_device->GetZeroBuffer();
	offsets[MAX_VERTEX_STREAMS] = 0;
	strides[MAX_VERTEX_STREAMS] = 0;
	vkCmdBindVertexBuffers2( commandBuffer, 0, MAX_VERTEX_STREAMS + 1, vertexBuffers, offsets, nullptr, strides );

	if( indexed )
	{
		if( !m_indexBuffer.IsValid() )
		{
			return E_INVALIDCALL;
		}
		vkCmdBindIndexBuffer( commandBuffer, m_indexBuffer.m_buffer->GetVkBuffer(), 0, m_indexType );
	}

	if( DescriptorsChangedVulkan() )
	{
		if( !WriteDescriptorsVulkan( VK_PIPELINE_BIND_POINT_GRAPHICS ) )
		{
			return E_FAIL;
		}
		m_descriptorsDirty = false;
	}
	for( auto& binding : program->GetBindings() )
	{
		if( IsUavDescriptor( binding.type ) && binding.registerClass == TrinityALImpl::SpirvBinding::UAV )
		{
			m_passWritesUavs = true;
			break;
		}
	}
	return S_OK;
}

// ------------------------------------------------------------------------------------------------------------------

ALResult Tr2RenderContextAL::SetStreamSource( uint32_t stream, const Tr2BufferAL& buffer, uint32_t offset, uint32_t stride ) throw()
{
	if( stream >= MAX_VERTEX_STREAMS )
	{
		return E_INVALIDARG;
	}
	m_streams[stream].buffer = buffer;
	m_streams[stream].offset = offset;
	m_streams[stream].stride = stride;
	return S_OK;
}

ALResult Tr2RenderContextAL::SetIndices( const Tr2BufferAL& buffer ) throw()
{
	return SetIndices( buffer, buffer.IsValid() ? buffer.GetDesc().stride : 2 );
}

ALResult Tr2RenderContextAL::SetIndices( const Tr2BufferAL& buffer, uint32_t stride ) throw()
{
	m_indexBuffer = buffer;
	m_indexType = stride == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
	return S_OK;
}

ALResult Tr2RenderContextAL::ClearUav( const Tr2BufferAL& buffer, const float values[4] ) throw()
{
	uint32_t bits[4];
	memcpy( bits, values, sizeof( bits ) );
	return ClearUav( buffer, bits );
}

// Fills the buffer with one element built from the values (as many components as the format has; raw and structured
// buffers repeat values[0]).
ALResult Tr2RenderContextAL::ClearUav( const Tr2BufferAL& buffer, const uint32_t values[4] ) throw()
{
	if( !m_device || !buffer.IsValid() )
	{
		return E_INVALIDARG;
	}
	auto impl = buffer.m_buffer.get();
	const auto& desc = impl->GetDesc();
	uint32_t components = 1;
	if( desc.format != PIXEL_FORMAT_UNKNOWN )
	{
		const uint32_t bytes = GetBytesPerPixel( desc.format );
		if( bytes % 4 != 0 )
		{
			return E_INVALIDARG; // only 32-bit component formats for now
		}
		components = bytes / 4;
	}
	const VkDeviceSize size = impl->GetByteSize();
	TrinityALImpl::VulkanDevice::UploadAllocation upload;
	if( !m_device->AllocateUpload( size, 16, upload ) )
	{
		return E_OUTOFMEMORY;
	}
	uint32_t* words = static_cast<uint32_t*>( upload.data );
	for( VkDeviceSize i = 0; i < size / 4; ++i )
	{
		words[i] = values[i % components];
	}
	m_device->RecordFullBarrier();
	VkBufferCopy region{ upload.offset, 0, size / 4 * 4 };
	vkCmdCopyBuffer( m_device->GetCommandBuffer(), upload.buffer, impl->GetVkBuffer(), 1, &region );
	m_device->RecordFullBarrier();
	return S_OK;
}

ALResult Tr2RenderContextAL::ClearUavVulkan( const Tr2TextureAL& texture, uint32_t mip, const VkClearColorValue& value )
{
	if( !m_device || !texture.IsValid() || mip >= texture.GetTrueMipCount() )
	{
		return E_INVALIDARG;
	}
	auto impl = texture.m_texture.get();
	if( impl->GetAspectMask() != VK_IMAGE_ASPECT_COLOR_BIT || IsCompressedFormat( texture.GetFormat() ) )
	{
		return E_INVALIDARG;
	}
	m_device->RecordFullBarrier();
	VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, mip, 1, 0, VK_REMAINING_ARRAY_LAYERS };
	vkCmdClearColorImage( m_device->GetCommandBuffer(), impl->GetVkImage(), VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range );
	m_device->RecordFullBarrier();
	return S_OK;
}

ALResult Tr2RenderContextAL::ClearUav( const Tr2TextureAL& texture, uint32_t mip, const float values[4] ) throw()
{
	VkClearColorValue value;
	memcpy( value.float32, values, sizeof( value.float32 ) );
	return ClearUavVulkan( texture, mip, value );
}

ALResult Tr2RenderContextAL::ClearUav( const Tr2TextureAL& texture, uint32_t mip, const uint32_t values[4] ) throw()
{
	VkClearColorValue value;
	memcpy( value.uint32, values, sizeof( value.uint32 ) );
	return ClearUavVulkan( texture, mip, value );
}

ALResult Tr2RenderContextAL::CopySubBuffer( Tr2BufferAL& dest, uint32_t destOffset, Tr2BufferAL& src, uint32_t offset, uint32_t length )
{
	if( !m_device || !dest.IsValid() || !src.IsValid() )
	{
		return E_INVALIDARG;
	}
	auto destBuffer = dest.m_buffer.get();
	auto srcBuffer = src.m_buffer.get();
	if( VkDeviceSize( destOffset ) + length > destBuffer->GetByteSize() || VkDeviceSize( offset ) + length > srcBuffer->GetByteSize() )
	{
		return E_INVALIDARG;
	}
	if( length == 0 )
	{
		return S_OK;
	}
	m_device->RecordFullBarrier();
	VkBufferCopy region{ offset, destOffset, length };
	vkCmdCopyBuffer( m_device->GetCommandBuffer(), srcBuffer->GetVkBuffer(), destBuffer->GetVkBuffer(), 1, &region );
	m_device->RecordFullBarrier();
	return S_OK;
}

// Clears the whole bound target(s), like D3D's ClearRenderTargetView/ClearDepthStencilView (not just the viewport).
ALResult Tr2RenderContextAL::Clear(
	uint32_t clearFlags,
	uint32_t color,
	float depth,
	uint32_t stencil,
	uint32_t slot )
{
	if( !m_device || slot >= MAX_RENDER_TARGET )
	{
		return E_INVALIDARG;
	}
	VkClearAttachment attachments[2];
	uint32_t count = 0;
	if( ( clearFlags & CLEARFLAGS_TARGET ) && m_boundRenderTarget[slot].IsValid() )
	{
		const float f = 1.0f / 255.0f;
		VkClearAttachment& attachment = attachments[count++];
		attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		attachment.colorAttachment = slot;
		attachment.clearValue.color.float32[0] = f * float( uint8_t( color >> 16 ) );
		attachment.clearValue.color.float32[1] = f * float( uint8_t( color >> 8 ) );
		attachment.clearValue.color.float32[2] = f * float( uint8_t( color ) );
		attachment.clearValue.color.float32[3] = f * float( uint8_t( color >> 24 ) );
	}
	if( ( clearFlags & ( CLEARFLAGS_ZBUFFER | CLEARFLAGS_STENCIL ) ) && m_boundDepthStencil.IsValid() )
	{
		VkImageAspectFlags available = m_boundDepthStencil.m_texture->GetAspectMask();
		VkImageAspectFlags aspect = 0;
		if( clearFlags & CLEARFLAGS_ZBUFFER )
		{
			aspect |= available & VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		if( clearFlags & CLEARFLAGS_STENCIL )
		{
			aspect |= available & VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		if( aspect )
		{
			VkClearAttachment& attachment = attachments[count++];
			attachment.aspectMask = aspect;
			attachment.colorAttachment = 0;
			attachment.clearValue.depthStencil = { depth, stencil };
		}
	}
	if( count == 0 )
	{
		return S_OK;
	}
	CR_RETURN_HR( BeginRenderingVulkan() );
	VkClearRect rect{ { { 0, 0 }, m_renderExtent }, 0, 1 };
	vkCmdClearAttachments( m_device->GetCommandBuffer(), count, attachments, 1, &rect );
	return S_OK;
}

ALResult Tr2RenderContextAL::SetTopology( long topology )
{
	if( topology <= TOP_INVALID || topology >= TOP_MAX_TOPOLOGY )
	{
		return E_FAIL;
	}
	m_topology = uint32_t( topology );
	return S_OK;
}

ALResult Tr2RenderContextAL::DrawIndexedPrimitive(
	uint32_t,
	uint32_t startIndex,
	uint32_t primitiveCount,
	uint32_t minimumIndex )
{
	CR_RETURN_HR( PrepareDrawVulkan( true ) );
	const uint32_t count = PrimitiveVertexCount( primitiveCount );
	CCP_STATS_ADD( vertexCount, count );
	// As the DX12 backend: the minimum index is the base vertex.
	vkCmdDrawIndexed( m_device->GetCommandBuffer(), count, 1, startIndex, int32_t( minimumIndex ), 0 );
	return S_OK;
}

ALResult Tr2RenderContextAL::DrawIndexedInstanced(
	uint32_t,
	uint32_t startIndex,
	uint32_t primitiveCount,
	uint32_t numInstances )
{
	CR_RETURN_HR( PrepareDrawVulkan( true ) );
	const uint32_t count = PrimitiveVertexCount( primitiveCount );
	CCP_STATS_ADD( vertexCount, count * numInstances );
	vkCmdDrawIndexed( m_device->GetCommandBuffer(), count, numInstances, startIndex, 0, 0 );
	return S_OK;
}

ALResult Tr2RenderContextAL::DrawIndexedInstanced(
	uint32_t indexCountPerInstance,
	uint32_t instanceCount,
	uint32_t startIndexLocation,
	int32_t baseVertexLocation,
	uint32_t startInstanceLocation )
{
	CR_RETURN_HR( PrepareDrawVulkan( true ) );
	CCP_STATS_ADD( vertexCount, indexCountPerInstance * instanceCount );
	vkCmdDrawIndexed( m_device->GetCommandBuffer(), indexCountPerInstance, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation );
	return S_OK;
}

ALResult Tr2RenderContextAL::DrawInstanced(
	uint32_t vertexCountPerInstance,
	uint32_t instanceCount,
	uint32_t startVertexLocation,
	uint32_t startInstanceLocation )
{
	CR_RETURN_HR( PrepareDrawVulkan( false ) );
	CCP_STATS_ADD( vertexCount, vertexCountPerInstance * instanceCount );
	vkCmdDraw( m_device->GetCommandBuffer(), vertexCountPerInstance, instanceCount, startVertexLocation, startInstanceLocation );
	return S_OK;
}

ALResult Tr2RenderContextAL::DrawPrimitive( uint32_t startVertex, uint32_t primitiveCount )
{
	CR_RETURN_HR( PrepareDrawVulkan( false ) );
	const uint32_t count = PrimitiveVertexCount( primitiveCount );
	CCP_STATS_ADD( vertexCount, count );
	vkCmdDraw( m_device->GetCommandBuffer(), count, 1, startVertex, 0 );
	return S_OK;
}

ALResult Tr2RenderContextAL::DrawIndexedInstancedIndirect( Tr2BufferAL& params, uint32_t offset )
{
	if( !params.IsValid() )
	{
		return E_INVALIDARG;
	}
	CR_RETURN_HR( PrepareDrawVulkan( true ) );
	vkCmdDrawIndexedIndirect( m_device->GetCommandBuffer(), params.m_buffer->GetVkBuffer(), offset, 1, sizeof( VkDrawIndexedIndirectCommand ) );
	return S_OK;
}

ALResult Tr2RenderContextAL::DrawInstancedIndirect( Tr2BufferAL& params, uint32_t offset )
{
	if( !params.IsValid() )
	{
		return E_INVALIDARG;
	}
	CR_RETURN_HR( PrepareDrawVulkan( false ) );
	vkCmdDrawIndirect( m_device->GetCommandBuffer(), params.m_buffer->GetVkBuffer(), offset, 1, sizeof( VkDrawIndirectCommand ) );
	return S_OK;
}

// User-pointer draws: vertices (and indices) are copied to the frame's upload memory and drawn from there.
ALResult Tr2RenderContextAL::DrawUPVulkan( uint32_t vertexCount, uint32_t vertexDataSize, const void* vertexData, uint32_t stride, const void* indexData, uint32_t indexCount, VkIndexType indexType )
{
	if( !m_device || !vertexData || stride == 0 )
	{
		return E_INVALIDARG;
	}
	TrinityALImpl::VulkanDevice::UploadAllocation vertices, indices;
	if( !m_device->AllocateUpload( vertexDataSize, 16, vertices ) )
	{
		return E_OUTOFMEMORY;
	}
	memcpy( vertices.data, vertexData, vertexDataSize );
	const uint32_t indexSize = indexType == VK_INDEX_TYPE_UINT16 ? 2 : 4;
	if( indexData && !m_device->AllocateUpload( VkDeviceSize( indexCount ) * indexSize, 16, indices ) )
	{
		return E_OUTOFMEMORY;
	}
	if( indexData )
	{
		memcpy( indices.data, indexData, size_t( indexCount ) * indexSize );
	}

	// Stream 0 is replaced for this draw only, as D3D9's DrawPrimitiveUP resets it afterwards.
	VertexStream saved = m_streams[0];
	m_streams[0] = VertexStream();
	ALResult hr = PrepareDrawVulkan( false );
	m_streams[0] = saved;
	if( FAILED( hr ) )
	{
		return hr;
	}
	VkCommandBuffer commandBuffer = m_device->GetCommandBuffer();
	VkDeviceSize vertexStride = stride;
	vkCmdBindVertexBuffers2( commandBuffer, 0, 1, &vertices.buffer, &vertices.offset, nullptr, &vertexStride );
	if( indexData )
	{
		vkCmdBindIndexBuffer( commandBuffer, indices.buffer, indices.offset, indexType );
		vkCmdDrawIndexed( commandBuffer, indexCount, 1, 0, 0, 0 );
	}
	else
	{
		vkCmdDraw( commandBuffer, vertexCount, 1, 0, 0 );
	}
	CCP_STATS_ADD( vertexCount, indexData ? indexCount : vertexCount );
	return S_OK;
}

ALResult Tr2RenderContextAL::DrawPrimitiveUP(
	uint32_t primitiveCount,
	const void* vertexStreamZeroData,
	uint32_t vertexStreamZeroStride )
{
	const uint32_t count = PrimitiveVertexCount( primitiveCount );
	return DrawUPVulkan( count, count * vertexStreamZeroStride, vertexStreamZeroData, vertexStreamZeroStride, nullptr, 0, VK_INDEX_TYPE_UINT16 );
}

ALResult Tr2RenderContextAL::DrawIndexedPrimitiveUP(
	uint32_t numVertices,
	uint32_t primitiveCount,
	const uint32_t* indexData,
	const void* vertexStreamZeroData,
	uint32_t vertexStreamZeroStride )
{
	if( !indexData || !vertexStreamZeroData )
	{
		return E_FAIL;
	}
	return DrawUPVulkan( numVertices, numVertices * vertexStreamZeroStride, vertexStreamZeroData, vertexStreamZeroStride, indexData, PrimitiveVertexCount( primitiveCount ), VK_INDEX_TYPE_UINT32 );
}

ALResult Tr2RenderContextAL::DrawIndexedPrimitiveUP(
	uint32_t numVertices,
	uint32_t primitiveCount,
	const uint16_t* indexData,
	const void* vertexStreamZeroData,
	uint32_t vertexStreamZeroStride )
{
	if( !indexData || !vertexStreamZeroData )
	{
		return E_FAIL;
	}
	return DrawUPVulkan( numVertices, numVertices * vertexStreamZeroStride, vertexStreamZeroData, vertexStreamZeroStride, indexData, PrimitiveVertexCount( primitiveCount ), VK_INDEX_TYPE_UINT16 );
}

ALResult Tr2RenderContextAL::RunComputeShader( unsigned groupDimX, unsigned groupDimY, unsigned groupDimZ )
{
	if( !m_device )
	{
		return E_FAIL;
	}
	if( !m_program.IsValid() || !m_program.m_program->IsCompute() )
	{
		return E_INVALIDCALL;
	}
	VkCommandBuffer commandBuffer = m_device->GetCommandBuffer();
	if( m_commandBufferSerial != m_device->GetSubmittedFrameCount() )
	{
		OnNewCommandBufferVulkan();
	}
	// Dispatches run outside render passes, ordered with everything around them.
	m_device->RecordFullBarrier();
	vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_program.m_program->GetComputePipeline() );
	if( !WriteDescriptorsVulkan( VK_PIPELINE_BIND_POINT_COMPUTE ) )
	{
		return E_FAIL;
	}
	m_descriptorsDirty = true; // the graphics bind point's set was not touched, but versions were recorded for compute
	vkCmdDispatch( commandBuffer, groupDimX, groupDimY, groupDimZ );
	m_device->RecordFullBarrier();
	return S_OK;
}

ALResult Tr2RenderContextAL::RunComputeShaderIndirect( Tr2BufferAL& params, unsigned offset )
{
	if( !m_device || !params.IsValid() )
	{
		return E_INVALIDARG;
	}
	if( !m_program.IsValid() || !m_program.m_program->IsCompute() )
	{
		return E_INVALIDCALL;
	}
	VkCommandBuffer commandBuffer = m_device->GetCommandBuffer();
	if( m_commandBufferSerial != m_device->GetSubmittedFrameCount() )
	{
		OnNewCommandBufferVulkan();
	}
	m_device->RecordFullBarrier();
	vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_program.m_program->GetComputePipeline() );
	if( !WriteDescriptorsVulkan( VK_PIPELINE_BIND_POINT_COMPUTE ) )
	{
		return E_FAIL;
	}
	m_descriptorsDirty = true;
	vkCmdDispatchIndirect( commandBuffer, params.m_buffer->GetVkBuffer(), offset );
	m_device->RecordFullBarrier();
	return S_OK;
}

ALResult Tr2RenderContextAL::SetConstants(
	const Tr2ConstantBufferAL& buffer,
	ShaderType stage,
	uint32_t registerIndex,
	uint32_t )
{
	if( stage >= SHADER_TYPE_COUNT || registerIndex >= MAX_CONSTANT_REGISTERS )
	{
		return E_INVALIDARG;
	}
	m_constants[stage][registerIndex] = buffer;
	m_descriptorsDirty = true;
	return S_OK;
}

void Tr2RenderContextAL::SetReadOnlyDepth( bool enable )
{
	m_readOnlyDepth = enable;
}

bool Tr2RenderContextAL::GetReadOnlyDepth() const
{
	return m_readOnlyDepth;
}

ALResult Tr2RenderContextAL::SetDepthStencil( const Tr2TextureAL& depthStencil )
{
	if( depthStencil == m_boundDepthStencil )
	{
		return S_OK;
	}
	if( depthStencil.IsValid() && !Tr2GpuUsage::HasFlag( depthStencil.GetGpuUsage(), Tr2GpuUsage::DEPTH_STENCIL ) )
	{
		return E_INVALIDARG;
	}
	EndRenderingVulkan( true );
	m_boundDepthStencil = depthStencil;
	return S_OK;
}

ALResult Tr2RenderContextAL::SetRenderTarget( const Tr2TextureAL& renderTarget, uint32_t slot, uint32_t slice )
{
	if( slot >= MAX_RENDER_TARGET )
	{
		return E_INVALIDARG;
	}
	if( renderTarget == m_boundRenderTarget[slot] && slice == m_boundSlice[slot] )
	{
		return S_OK;
	}
	if( renderTarget.IsValid() && !Tr2GpuUsage::HasFlag( renderTarget.GetGpuUsage(), Tr2GpuUsage::RENDER_TARGET ) )
	{
		return E_INVALIDARG;
	}
	EndRenderingVulkan( true );
	m_boundRenderTarget[slot] = renderTarget;
	m_boundSlice[slot] = slice;
	return S_OK;
}

ALResult Tr2RenderContextAL::CreateDevice(
	uint32_t Adapter,
	Tr2WindowHandle,
	const Tr2PresentParametersAL& presentationParameters )
{
	Destroy();
	m_device = TrinityALImpl::VulkanDevice::Create( Adapter );
	if( !m_device )
	{
		return E_FAIL;
	}
	m_device->SetRenderingEndHook( [this] { EndRenderingVulkan( false ); } );
	m_isValid = true;
	CR_RETURN_HR( SetPresentParameters( Adapter, presentationParameters ) );
	if( m_events )
	{
		m_events->OnContextCreated( *this );
	}

	return S_OK;
}

PixelFormat Tr2RenderContextAL::GetBackBufferFormat() const
{
	return m_defaultBackBuffer.GetFormat();
}

ALResult Tr2RenderContextAL::SetPresentParameters( unsigned, const Tr2PresentParametersAL& presentationParameters )
{
	// Offscreen until the swapchain exists (phase 3). CPU-readable so screenshots and tests can read it back.
	EndRenderingVulkan( true );
	CR_RETURN_HR( m_defaultBackBuffer.Create(
		Tr2BitmapDimensions( presentationParameters.mode.width, presentationParameters.mode.height, 1, PIXEL_FORMAT_B8G8R8A8_UNORM ),
		Tr2MsaaDesc(),
		Tr2GpuUsage::RENDER_TARGET,
		Tr2CpuUsage::READ,
		nullptr,
		*this ) );

	SetRenderTarget( m_defaultBackBuffer );
	m_viewport = Tr2Viewport( presentationParameters.mode.width, presentationParameters.mode.height );

	return S_OK;
}

const Tr2CapsAL& Tr2RenderContextAL::GetCaps() const
{
	return m_caps;
}

ALResult Tr2RenderContextAL::BeginScene()
{
	if( !m_device )
	{
		return E_FAIL;
	}
	m_device->GetCommandBuffer();
	return S_OK;
}

ALResult Tr2RenderContextAL::EndScene()
{
	return S_OK;
}

ALResult Tr2RenderContextAL::Present()
{
	if( !m_device )
	{
		return E_FAIL;
	}
	// Offscreen until the swapchain exists (phase 3): presenting submits the frame's commands.
	EndRenderingVulkan( false );
	if( m_device->Submit() != VK_SUCCESS )
	{
		return E_FAIL;
	}
	++m_frameNumber;
	return S_OK;
}

bool Tr2RenderContextAL::IsValid()
{
	return m_isValid && m_device;
}

ALResult Tr2RenderContextAL::SetVertexLayout( const Tr2VertexLayoutAL& layout )
{
	m_vertexLayout = layout;
	return S_OK;
}

ALResult Tr2RenderContextAL::SetShaderProgram( const Tr2ShaderProgramAL& program )
{
	if( program == m_program )
	{
		return S_OK;
	}
	if( m_passWritesUavs )
	{
		EndRenderingVulkan( true ); // UAV writes of earlier draws visible to later ones
	}
	m_program = program;
	m_descriptorsDirty = true;
	return S_OK;
}

ALResult Tr2RenderContextAL::SetRenderState( RenderState state, uint32_t value )
{
	auto& s = m_pipelineState;
	float floatValue;
	memcpy( &floatValue, &value, sizeof( floatValue ) );
	switch( state )
	{
	case RS_ALPHABLENDENABLE:
		s.blendEnable = value != 0;
		break;
	case RS_SRCBLEND:
		s.srcBlend = value;
		break;
	case RS_DESTBLEND:
		s.destBlend = value;
		break;
	case RS_BLENDOP:
		s.blendOp = value;
		break;
	case RS_SRCBLENDALPHA:
		s.srcBlendAlpha = value;
		break;
	case RS_DESTBLENDALPHA:
		s.destBlendAlpha = value;
		break;
	case RS_BLENDOPALPHA:
		s.blendOpAlpha = value;
		break;
	case RS_SEPARATEALPHABLENDENABLE:
		m_separateAlphaBlend = value != 0;
		break;
	case RS_COLORWRITEENABLE:
		s.colorWriteMask = value & 0xf;
		break;
	case RS_BLENDFACTOR: // D3DCOLOR
		m_blendFactor[0] = float( uint8_t( value >> 16 ) ) / 255.0f;
		m_blendFactor[1] = float( uint8_t( value >> 8 ) ) / 255.0f;
		m_blendFactor[2] = float( uint8_t( value ) ) / 255.0f;
		m_blendFactor[3] = float( uint8_t( value >> 24 ) ) / 255.0f;
		break;
	case RS_CULLMODE:
		s.cullMode = value;
		break;
	case RS_FILLMODE:
		s.fillMode = value;
		break;
	case RS_DEPTHBIAS:
	case RS_ZBIAS:
		s.depthBias = int32_t( floatValue ); // as DX12: the float state becomes D3D12's integer DepthBias
		break;
	case RS_SLOPESCALEDEPTHBIAS:
		s.slopeScaledDepthBias = floatValue;
		break;
	case RS_DEPTH_CLIP_ENABLE:
		s.depthClipEnable = value != 0;
		break;
	case RS_ZENABLE:
		s.depthEnable = value != 0;
		break;
	case RS_ZWRITEENABLE:
		s.depthWriteEnable = value != 0;
		break;
	case RS_ZFUNC:
		s.depthFunc = value;
		break;
	case RS_STENCILENABLE:
		s.stencilEnable = value != 0;
		break;
	case RS_STENCILMASK:
		s.stencilReadMask = s.stencilWriteMask = value & 0xff;
		break;
	case RS_STENCILWRITEMASK:
		s.stencilWriteMask = value & 0xff;
		break;
	case RS_STENCILFAIL:
		s.stencilFailOp = value;
		break;
	case RS_STENCILZFAIL:
		s.stencilDepthFailOp = value;
		break;
	case RS_STENCILPASS:
		s.stencilPassOp = value;
		break;
	case RS_STENCILFUNC:
		s.stencilFunc = value;
		break;
	case RS_STENCILREF:
		m_stencilRef = value;
		break;
	case RS_SRGBWRITEENABLE:
		if( m_srgbWrite != ( value != 0 ) )
		{
			EndRenderingVulkan( true ); // the attachments' views change
			m_srgbWrite = value != 0;
		}
		break;
	case RS_ALPHATESTENABLE:
	case RS_ALPHAFUNC:
	case RS_ALPHAREF:
		return S_OK; // no fixed-function alpha test, as DX12
	default:
		return E_FAIL; // as DX12 (E_NOTIMPL there): state has no equivalent
	}
	return S_OK;
}

ALResult Tr2RenderContextAL::SetRenderStates( const uint32_t* stateValuePairs, uint32_t count )
{
	while( count-- )
	{
		auto state = *stateValuePairs++;
		auto value = *stateValuePairs++;
		SetRenderState( RenderState( state ), value );
	}
	return S_OK;
}

ALResult Tr2RenderContextAL::SetResourceSet( const Tr2ResourceSetAL& resourceSet )
{
	if( m_passWritesUavs )
	{
		EndRenderingVulkan( true ); // UAV writes of earlier draws visible to later ones
	}
	m_resourceSet = resourceSet;
	m_descriptorsDirty = true;
	return S_OK;
}

ALResult Tr2RenderContextAL::SetViewport( const Tr2Viewport& viewport )
{
	m_viewport = viewport;
	return S_OK;
}

ALResult Tr2RenderContextAL::GetViewport( Tr2Viewport& viewport )
{
	viewport = m_viewport;
	return S_OK;
}

long Tr2RenderContextAL::GetTotalVideoMemory()
{
	if( !m_device )
	{
		return 0;
	}
	VkPhysicalDeviceMemoryProperties memory;
	vkGetPhysicalDeviceMemoryProperties( m_device->GetPhysicalDevice(), &memory );
	VkDeviceSize total = 0;
	for( uint32_t i = 0; i < memory.memoryHeapCount; ++i )
	{
		if( memory.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT )
		{
			total += memory.memoryHeaps[i].size;
		}
	}
	return long( total / ( 1024 * 1024 ) );
}

ALResult Tr2RenderContextAL::PushRenderTarget( uint32_t slot )
{
	CCP_ASSERT( slot < MAX_RENDER_TARGET );
	if( slot >= MAX_RENDER_TARGET )
	{
		return E_INVALIDARG;
	}
	RenderTargetBinding binding;
	binding.texture = m_boundRenderTarget[slot];
	binding.slice = m_boundSlice[slot];
	m_stackRT[slot].push( binding );
	return S_OK;
}

ALResult Tr2RenderContextAL::PopRenderTarget( uint32_t slot )
{
	CCP_ASSERT( slot < MAX_RENDER_TARGET );
	if( slot >= MAX_RENDER_TARGET )
	{
		return E_INVALIDARG;
	}
	CCP_ASSERT( !m_stackRT[slot].empty() );
	if( m_stackRT[slot].empty() )
	{
		return E_FAIL;
	}
	RenderTargetBinding binding = m_stackRT[slot].top();
	m_stackRT[slot].pop();
	return SetRenderTarget( binding.texture, slot, binding.slice );
}

ALResult Tr2RenderContextAL::PushDepthStencil()
{
	m_stackDS.push( m_boundDepthStencil );
	return S_OK;
}

ALResult Tr2RenderContextAL::PopDepthStencil()
{
	if( m_stackDS.empty() )
	{
		return E_FAIL;
	}
	Tr2TextureAL depthStencil = m_stackDS.top();
	m_stackDS.pop();
	return SetDepthStencil( depthStencil );
}

ALResult Tr2RenderContextAL::GetRenderTargetSize( uint32_t& width, uint32_t& height, uint32_t slot )
{
	if( slot >= MAX_RENDER_TARGET )
	{
		return E_FAIL;
	}
	if( !m_boundRenderTarget[slot].IsValid() )
	{
		return E_INVALIDCALL;
	}
	width = m_boundRenderTarget[slot].GetWidth();
	height = m_boundRenderTarget[slot].GetHeight();
	return S_OK;
}

void Tr2RenderContextAL::ReleaseDeviceResources()
{
	EndRenderingVulkan( false );
	for( unsigned i = 0; i != MAX_RENDER_TARGET; ++i )
	{
		m_boundRenderTarget[i] = Tr2TextureAL();
	}
	m_boundDepthStencil = Tr2TextureAL();
	m_defaultBackBuffer = Tr2TextureAL();
}

// --------------------------------------------------------------------------------------
void Tr2RenderContextAL::AddGpuMarker( const char* )
{
}

void Tr2RenderContextAL::PushGpuMarker( const char* )
{
}

void Tr2RenderContextAL::PopGpuMarker()
{
}

// --------------------------------------------------------------------------------------
ALResult Tr2RenderContextAL::GetGpuStateMarker( Tr2RenderContextEnum::RenderContextStatus&, std::string& ) const
{
	return E_FAIL;
}

// --------------------------------------------------------------------------------------
ALResult Tr2RenderContextAL::GetGpuPageFaultResource(
	Tr2RenderContextEnum::PixelFormat&,
	uint64_t&,
	uint32_t&,
	uint32_t&,
	uint32_t&,
	uint32_t& ) const
{
	return E_FAIL;
}

void Tr2RenderContextAL::RenderPassHint( const Tr2ColorAttachment&, const Tr2DepthAttachment& )
{
}

void Tr2RenderContextAL::RenderPassHint( const Tr2ColorAttachment&, const Tr2ColorAttachment&, const Tr2DepthAttachment& )
{
}

ALResult Tr2RenderContextAL::UseResources( Tr2UseResourceDestination, Tr2GpuUsage::Type, const Tr2BindlessResourcesAL& )
{
	return S_OK;
}

ALResult Tr2RenderContextAL::UseAccelerationStructure( Tr2RtTopLevelAccelerationStructureAL tlas )
{
	return S_OK;
}

bool Tr2RenderContextAL::SupportsBindlessTextures() const
{
	return false;
}

uint64_t Tr2RenderContextAL::GetRecordingFrameNumber() const
{
	return m_frameNumber + 1;
}


Tr2UpscalingAL::Result Tr2RenderContextAL::EnableUpscaling( Tr2UpscalingAL::Technique tech, Tr2UpscalingAL::Setting setting, bool frameGeneration, uint32_t adapter )
{
	return Tr2UpscalingAL::Result::OK;
}

Tr2UpscalingContextAL* Tr2RenderContextAL::GetUpscalingContext( uint32_t upscalingContextID )
{
	return nullptr;
}

Tr2UpscalingContextAL* Tr2RenderContextAL::CreateUpscalingContext( Tr2UpscalingAL::UpscalingContextParams params, uint32_t existingContext )
{
	return nullptr;
}

void Tr2RenderContextAL::DeleteUpscalingContext( uint32_t contextID )
{
}

Tr2UpscalingAL::UpscalingInfo Tr2RenderContextAL::GetUpscalingInfo( uint32_t upscalingContextID )
{
	return Tr2UpscalingAL::UpscalingInfo();
}

void Tr2PrimaryRenderContextAL::GetUpscalingSetup( Tr2UpscalingAL::Technique& technique, Tr2UpscalingAL::Setting& setting, bool& framegeneration, bool& temporal )
{
	technique = Tr2UpscalingAL::Technique::NONE;
	setting = Tr2UpscalingAL::Setting::NATIVE;
	framegeneration = false;
	temporal = false;
}

std::vector<std::tuple<Tr2UpscalingAL::Technique, uint32_t, bool>> Tr2RenderContextAL::GetSupportedUpscalingTechniques( uint32_t adapter )
{
	return std::vector<std::tuple<Tr2UpscalingAL::Technique, uint32_t, bool>>();
}


void Tr2RenderContextAL::MarkFrameEvent( Tr2RenderContextEnum::FrameEvent frameEvent )
{
}
uint64_t Tr2RenderContextAL::GetRenderedFrameNumber() const
{
	return m_frameNumber;
}


#endif

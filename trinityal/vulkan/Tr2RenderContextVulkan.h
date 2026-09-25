// Copyright © 2023 CCP ehf.

#pragma once
#ifndef Tr2RenderContextVulkan_h_
#define Tr2RenderContextVulkan_h_

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "../Tr2RenderContextEnum.h"
#include "../include/Tr2TextureAL.h"
#include "../include/Tr2CapsAL.h"
#include "../include/Tr2SamplerStateAL.h"
#include "../include/Tr2RenderPassAL.h"
#include "../include/Tr2RtTopLevelAccelerationStructureAL.h"
#include "../Tr2HalHelperStructures.h"
#include "../include/upscaling/Tr2UpscalingAL.h"
#include "../include/Tr2BufferAL.h"
#include "../include/Tr2ConstantBufferAL.h"
#include "../include/Tr2ResourceSetAL.h"
#include "../include/Tr2ShaderProgramAL.h"
#include "../include/Tr2VertexLayoutAL.h"
#include "VulkanIncludes.h"
#include "VulkanPipelineState.h"

#include <deque>
#include <unordered_map>

namespace TrinityALImpl
{
class VulkanDevice;
}

class Tr2ConstantBufferAL;
class Tr2VertexLayoutAL;
class Tr2ShaderAL;
class Tr2SamplerStateAL;
class Tr2TextureAL;
class Tr2ResourceSetAL;
class Tr2BufferAL;
class Tr2RtShaderTableAL;
class Tr2RtPipelineStateAL;
struct ITr2RenderContextEvents;
struct Tr2PresentParametersAL;



class Tr2BindlessResourcesAL
{
public:
	void Add( const Tr2TextureAL& )
	{
	}
	void Add( const Tr2BufferAL& )
	{
	}
	void Add( const Tr2BindlessResourcesAL& )
	{
	}
	void Clear()
	{
	}
};

// -------------------------------------------------------------
// Description:
//   See http://carbon/wiki/Tr2RenderContext
// -------------------------------------------------------------
class Tr2RenderContextAL
{
public:
	Tr2RenderContextAL();
	~Tr2RenderContextAL();
	void Destroy();

	static void SetPrimaryRenderContext( Tr2PrimaryRenderContextAL* );
	static Tr2PrimaryRenderContextAL& GetPrimaryRenderContext();
	static Tr2PrimaryRenderContextAL* GetPrimaryRenderContextPointer();

	ALResult CreateDevice(
		uint32_t Adapter,
		Tr2WindowHandle hFocusWindow,
		const Tr2PresentParametersAL& presentationParameters );
	ALResult SetPresentParameters( unsigned adapter, const Tr2PresentParametersAL& presentationParameters );

	const Tr2CapsAL& GetCaps() const;

	ALResult BeginScene();
	ALResult EndScene();
	ALResult Present();

	bool IsValid();

	void ReleaseDeviceResources();




	ALResult SetStreamSource(
		uint32_t stream,
		const Tr2BufferAL& buffer,
		uint32_t offset,
		uint32_t stride ) throw();
	ALResult SetIndices( const Tr2BufferAL& buffer ) throw();
	ALResult SetIndices( const Tr2BufferAL& buffer, uint32_t stride ) throw();
	ALResult ClearUav( const Tr2BufferAL& buffer, const float values[4] ) throw();
	ALResult ClearUav( const Tr2BufferAL& buffer, const uint32_t values[4] ) throw();

	ALResult CopySubBuffer(
		Tr2BufferAL& dest,
		uint32_t destOffset,
		Tr2BufferAL& src,
		uint32_t offset,
		uint32_t length );

	ALResult SetTopology( long topology );
	ALResult SetShaderProgram( const Tr2ShaderProgramAL& shaderProgram );


	ALResult ClearUav( const Tr2TextureAL& texture, uint32_t mip, const float values[4] ) throw();
	ALResult ClearUav( const Tr2TextureAL& texture, uint32_t mip, const uint32_t values[4] ) throw();

	ALResult SetResourceSet( const Tr2ResourceSetAL& resourceSet );

	ALResult DrawIndexedPrimitive(
		uint32_t numVertices,
		uint32_t startIndex,
		uint32_t primitiveCount,
		uint32_t minimumIndex = 0 );

	ALResult DrawPrimitive( uint32_t startVertex, uint32_t primitiveCount );

	ALResult DrawIndexedInstanced(
		uint32_t numVertices,
		uint32_t startIndex,
		uint32_t primitiveCount,
		uint32_t numInstances );

	ALResult DrawIndexedInstanced(
		uint32_t indexCountPerInstance,
		uint32_t instanceCount,
		uint32_t startIndexLocation,
		int32_t baseVertexLocation,
		uint32_t startInstanceLocation );
	ALResult DrawInstanced(
		uint32_t vertexCountPerInstance,
		uint32_t instanceCount,
		uint32_t startVertexLocation,
		uint32_t startInstanceLocation );

	ALResult DrawIndexedPrimitiveUP(
		uint32_t numVertices,
		uint32_t primitiveCount,
		const uint32_t* indexData,
		const void* vertexStreamZeroData,
		uint32_t vertexStreamZeroStride );

	ALResult DrawIndexedPrimitiveUP(
		uint32_t numVertices,
		uint32_t primitiveCount,
		const uint16_t* indexData,
		const void* vertexStreamZeroData,
		uint32_t vertexStreamZeroStride );

	ALResult DrawPrimitiveUP(
		uint32_t primitiveCount,
		const void* vertexStreamZeroData,
		uint32_t vertexStreamZeroStride );

	ALResult DrawIndexedInstancedIndirect( Tr2BufferAL& params, uint32_t offset );
	ALResult DrawInstancedIndirect( Tr2BufferAL& params, uint32_t offset );

	ALResult RunComputeShader( unsigned groupDimX, unsigned groupDimY, unsigned groupDimZ );
	ALResult RunComputeShaderIndirect( Tr2BufferAL& params, unsigned offset );

	ALResult DispatchRays( Tr2RtPipelineStateAL& pipeline, Tr2RtShaderTableAL& shaderTable, const wchar_t* rayGenShader, uint32_t width, uint32_t height, uint32_t depth )
	{
		return E_FAIL;
	}

	ALResult SetVertexLayout( const Tr2VertexLayoutAL& layout );

	ALResult SetRenderState( Tr2RenderContextEnum::RenderState state, uint32_t value );
	ALResult SetRenderStates( const uint32_t* stateValuePairs, uint32_t count );

	ALResult SetConstants(
		const Tr2ConstantBufferAL& buffer,
		Tr2RenderContextEnum::ShaderType constantType,
		uint32_t registerIndex,
		uint32_t maxRegisterCount = 0 );

	// Helper function to clear the current primary backbuffer, depth and/or stencil.
	ALResult Clear(
		uint32_t clearFlags,
		uint32_t color,
		float depth,
		uint32_t stencil = 0,
		uint32_t slot = 0 );

	ALResult SetDepthStencil( const Tr2TextureAL& depthStencil );
	void SetReadOnlyDepth( bool enable );
	bool GetReadOnlyDepth() const;
	ALResult SetRenderTarget( const Tr2TextureAL& renderTarget, uint32_t slot = 0, uint32_t slice = 0 );

	void RenderPassHint( const Tr2ColorAttachment& rt0, const Tr2DepthAttachment& depth );
	void RenderPassHint( const Tr2ColorAttachment& rt0, const Tr2ColorAttachment& rt1, const Tr2DepthAttachment& depth );

	ALResult SetViewport( const Tr2Viewport& viewport );
	ALResult GetViewport( Tr2Viewport& viewport );

	ALResult PushRenderTarget( uint32_t slot = 0 );
	ALResult PopRenderTarget( uint32_t slot = 0 );
	ALResult PushDepthStencil();
	ALResult PopDepthStencil();
	ALResult GetRenderTargetSize(
		uint32_t& width,
		uint32_t& height,
		uint32_t slot = 0 );

	long GetTotalVideoMemory();

	Tr2RenderContextEnum::PixelFormat GetBackBufferFormat() const;

	static const uint32_t SHADER_TYPE_MASK =
		( 1 << Tr2RenderContextEnum::VERTEX_SHADER ) |
		( 1 << Tr2RenderContextEnum::PIXEL_SHADER );

	// Debug helpers
	size_t GetStackSizeRT( uint32_t = 0 ) const
	{
		return 0;
	}
	size_t GetStackSizeDS() const
	{
		return 0;
	}

	Tr2CapsAL m_caps;

	ITr2RenderContextEvents* m_events;

	// The Vulkan device this context records into; null until CreateDevice succeeds.
	TrinityALImpl::VulkanDevice* GetVulkanDevice() const
	{
		return m_device.get();
	}
	// Resources keep the device alive until they are destroyed.
	const std::shared_ptr<TrinityALImpl::VulkanDevice>& GetVulkanDeviceShared() const
	{
		return m_device;
	}
	Tr2TextureAL& GetDefaultBackBuffer()
	{
		return m_defaultBackBuffer;
	}

	void AddGpuMarker( const char* marker );
	void PushGpuMarker( const char* marker );
	void PopGpuMarker();
	ALResult GetGpuStateMarker( Tr2RenderContextEnum::RenderContextStatus& status, std::string& marker ) const;
	ALResult GetGpuPageFaultResource(
		Tr2RenderContextEnum::PixelFormat& format,
		uint64_t& size,
		uint32_t& width,
		uint32_t& height,
		uint32_t& depth,
		uint32_t& mips ) const;

	ALResult UseResources( Tr2UseResourceDestination dest, Tr2GpuUsage::Type usage, const Tr2BindlessResourcesAL& resources );
	ALResult UseAccelerationStructure( Tr2RtTopLevelAccelerationStructureAL tlas );

	bool SupportsBindlessTextures() const;

	uint64_t GetRecordingFrameNumber() const;
	uint64_t GetRenderedFrameNumber() const;


	Tr2UpscalingAL::Result EnableUpscaling( Tr2UpscalingAL::Technique tech, Tr2UpscalingAL::Setting setting, bool framegeneration, uint32_t adapter );
	Tr2UpscalingContextAL* GetUpscalingContext( uint32_t upscalingContextID );
	Tr2UpscalingContextAL* CreateUpscalingContext( Tr2UpscalingAL::UpscalingContextParams params, uint32_t existingContext = Tr2UpscalingAL::INVALID_CONTEXT_ID );
	void DeleteUpscalingContext( uint32_t contextID );
	Tr2UpscalingAL::UpscalingInfo GetUpscalingInfo( uint32_t upscalingContextID );
	std::vector<std::tuple<Tr2UpscalingAL::Technique, uint32_t, bool>> GetSupportedUpscalingTechniques( uint32_t adapter );
	void GetUpscalingSetup( Tr2UpscalingAL::Technique& technique, Tr2UpscalingAL::Setting& setting, bool& framegeneration, bool& temporal );
	void MarkFrameEvent( Tr2RenderContextEnum::FrameEvent frameEvent );

private:
	enum
	{
		MAX_RENDER_TARGET = 8,
		MAX_VERTEX_STREAMS = 4,
		MAX_CONSTANT_REGISTERS = 32,
	};

	struct RenderTargetBinding
	{
		Tr2TextureAL texture;
		uint32_t slice = 0;
	};
	struct VertexStream
	{
		Tr2BufferAL buffer;
		uint32_t offset = 0;
		uint32_t stride = 0;
	};

	// Recording helpers. Draws and clears run inside a dynamic-rendering scope that stays open until the attachments
	// change or something outside a render pass has to be recorded (the device's rendering-end hook).
	void OnNewCommandBufferVulkan();
	ALResult BeginRenderingVulkan();
	void EndRenderingVulkan( bool barrier );
	ALResult PrepareDrawVulkan( bool indexed );
	bool WriteDescriptorsVulkan( VkPipelineBindPoint bindPoint );
	bool DescriptorsChangedVulkan() const;
	uint32_t PrimitiveVertexCount( uint32_t primitiveCount ) const;
	ALResult DrawUPVulkan( uint32_t vertexCount, uint32_t vertexDataSize, const void* vertexData, uint32_t stride, const void* indexData, uint32_t indexCount, VkIndexType indexType );
	ALResult ClearUavVulkan( const Tr2TextureAL& texture, uint32_t mip, const VkClearColorValue& value );

	Tr2TextureAL m_boundRenderTarget[MAX_RENDER_TARGET];
	uint32_t m_boundSlice[MAX_RENDER_TARGET];
	Tr2TextureAL m_boundDepthStencil;
	bool m_isValid;
	Tr2TextureAL m_defaultBackBuffer;
	Tr2Viewport m_viewport;
	TrackableStdStack<RenderTargetBinding> m_stackRT[MAX_RENDER_TARGET];
	TrackableStdStack<Tr2TextureAL> m_stackDS;
	uint64_t m_frameNumber; // frames presented
	// Presented frames still on the GPU, with the serial of their last submission; the rendered frame number advances as
	// they complete.
	mutable std::deque<std::pair<uint64_t, uint64_t>> m_framesInFlight;
	mutable uint64_t m_renderedFrameNumber;
	std::shared_ptr<TrinityALImpl::VulkanDevice> m_device;

	// Draw state
	VertexStream m_streams[MAX_VERTEX_STREAMS];
	Tr2BufferAL m_indexBuffer;
	VkIndexType m_indexType;
	Tr2VertexLayoutAL m_vertexLayout;
	Tr2ShaderProgramAL m_program;
	Tr2ResourceSetAL m_resourceSet;
	Tr2ConstantBufferAL m_constants[Tr2RenderContextEnum::SHADER_TYPE_COUNT][MAX_CONSTANT_REGISTERS];
	uint32_t m_topology;
	TrinityALImpl::GraphicsPipelineState m_pipelineState; // render states; attachments are filled in per draw
	bool m_separateAlphaBlend;
	bool m_srgbWrite;
	bool m_readOnlyDepth;
	uint32_t m_stencilRef;
	float m_blendFactor[4];

	// Recording state
	uint64_t m_commandBufferSerial; // device submit count the bound state below belongs to
	bool m_rendering;
	VkExtent2D m_renderExtent;
	VkPipeline m_boundPipeline;
	bool m_descriptorsDirty;
	bool m_passWritesUavs; // a draw in the open pass used UAVs; the next resource change needs a barrier
	std::vector<uint64_t> m_boundConstantVersions; // per constant-buffer binding of the program, at the last write
	struct ConstantUpload
	{
		uint64_t version;
		VkBuffer buffer;
		VkDeviceSize offset;
	};
	std::unordered_map<const void*, ConstantUpload> m_constantUploads; // this command buffer's copies
	VkSampler m_defaultSampler;

public:
	TrinityALImpl::Tr2SamplerStateALFactory m_samplerStateFactory;
};

#endif // #if( TRINITY_PLATFORM==TRINITY_VULKAN )

#endif //Tr2RenderContextVulkan_h_

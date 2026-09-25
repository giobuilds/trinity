// Copyright © 2026 CCP ehf.

#pragma once

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "VulkanIncludes.h"

#include <cstring>
#include <functional>

namespace TrinityALImpl
{

// Everything a graphics pipeline depends on besides the program: fixed-function state in trinity's (D3D) terms, the
// vertex layout and the attachment formats. Viewport, scissor, stencil reference, blend constants and vertex strides are
// dynamic. Plain data, zero-filled, so it hashes and compares as bytes.
struct GraphicsPipelineState
{
	GraphicsPipelineState()
	{
		memset( this, 0, sizeof( *this ) );
	}

	uint64_t vertexLayoutId;
	uint32_t topology; // VkPrimitiveTopology

	// Rasterizer (D3D12 defaults are set by the render context)
	uint32_t cullMode; // Tr2RenderContextEnum::CullMode
	uint32_t fillMode; // Tr2RenderContextEnum::FillMode
	int32_t depthBias;
	float slopeScaledDepthBias;
	uint32_t depthClipEnable;

	// Depth-stencil
	uint32_t depthEnable;
	uint32_t depthWriteEnable;
	uint32_t depthFunc;
	uint32_t stencilEnable;
	uint32_t stencilReadMask;
	uint32_t stencilWriteMask;
	uint32_t stencilFailOp;
	uint32_t stencilDepthFailOp;
	uint32_t stencilPassOp;
	uint32_t stencilFunc;

	// Blend (render target 0's state, applied to every attachment as D3D12 does without independent blend)
	uint32_t blendEnable;
	uint32_t srcBlend;
	uint32_t destBlend;
	uint32_t blendOp;
	uint32_t srcBlendAlpha;
	uint32_t destBlendAlpha;
	uint32_t blendOpAlpha;
	uint32_t colorWriteMask;

	// Attachments
	uint32_t colorAttachmentCount;
	uint32_t colorFormats[8]; // VkFormat
	uint32_t depthFormat; // VkFormat
	uint32_t stencilFormat; // VkFormat
	uint32_t samples;

	bool operator==( const GraphicsPipelineState& other ) const
	{
		return memcmp( this, &other, sizeof( *this ) ) == 0;
	}
};

struct GraphicsPipelineStateHash
{
	size_t operator()( const GraphicsPipelineState& state ) const
	{
		// FNV-1a over the bytes
		const uint8_t* bytes = reinterpret_cast<const uint8_t*>( &state );
		uint64_t hash = 1469598103934665603ull;
		for( size_t i = 0; i < sizeof( state ); ++i )
		{
			hash = ( hash ^ bytes[i] ) * 1099511628211ull;
		}
		return size_t( hash );
	}
};

}

#endif

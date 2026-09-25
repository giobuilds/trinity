// Copyright © 2026 CCP ehf.

#pragma once

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "VulkanIncludes.h"
#include "../Tr2RenderContextEnum.h"

struct Tr2SamplerDescription;

namespace TrinityALImpl
{

class VulkanDevice;

// Trinity's state enums carry D3D values (D3D9 names, D3D11/12 numbering); these translate them for Vulkan.
VkCompareOp ToVkCompareOp( uint32_t compareFunc );
VkBlendFactor ToVkBlendFactor( uint32_t blendMode );
// The alpha-channel equivalent of a colour blend factor (D3D derives alpha blending this way when it is not separate).
uint32_t AlphaBlendMode( uint32_t blendMode );
VkBlendOp ToVkBlendOp( uint32_t blendOperation );
VkStencilOp ToVkStencilOp( uint32_t stencilOperation );
VkSamplerAddressMode ToVkAddressMode( uint32_t addressMode );

// A sampler for a trinity description, as DX12 builds its samplers and static samplers. VK_NULL_HANDLE on failure.
VkSampler CreateVkSampler( VulkanDevice& device, const Tr2SamplerDescription& description );

}

#endif

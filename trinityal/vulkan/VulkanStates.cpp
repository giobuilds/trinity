// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "VulkanStates.h"
#include "VulkanDevice.h"
#include "ALLog.h"
#include "../Tr2HalHelperStructures.h"

#include <algorithm>
#include <limits>

using namespace Tr2RenderContextEnum;

namespace TrinityALImpl
{

VkCompareOp ToVkCompareOp( uint32_t compareFunc )
{
	switch( compareFunc )
	{
	case CMP_NEVER:
		return VK_COMPARE_OP_NEVER;
	case CMP_LESS:
		return VK_COMPARE_OP_LESS;
	case CMP_EQUAL:
		return VK_COMPARE_OP_EQUAL;
	case CMP_LESSEQUAL:
		return VK_COMPARE_OP_LESS_OR_EQUAL;
	case CMP_GREATER:
		return VK_COMPARE_OP_GREATER;
	case CMP_NOTEQUAL:
		return VK_COMPARE_OP_NOT_EQUAL;
	case CMP_GREATEREQUAL:
		return VK_COMPARE_OP_GREATER_OR_EQUAL;
	default:
		return VK_COMPARE_OP_ALWAYS;
	}
}

VkBlendFactor ToVkBlendFactor( uint32_t blendMode )
{
	switch( blendMode )
	{
	case BM_ZERO:
		return VK_BLEND_FACTOR_ZERO;
	case BM_ONE:
		return VK_BLEND_FACTOR_ONE;
	case BM_SRCCOLOR:
		return VK_BLEND_FACTOR_SRC_COLOR;
	case BM_INVSRCCOLOR:
		return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
	case BM_SRCALPHA:
	case BM_BOTHSRCALPHA:
		return VK_BLEND_FACTOR_SRC_ALPHA;
	case BM_INVSRCALPHA:
	case BM_BOTHINVSRCALPHA:
		return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	case BM_DESTALPHA:
		return VK_BLEND_FACTOR_DST_ALPHA;
	case BM_INVDESTALPHA:
		return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
	case BM_DESTCOLOR:
		return VK_BLEND_FACTOR_DST_COLOR;
	case BM_INVDESTCOLOR:
		return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
	case BM_SRCALPHASAT:
		return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
	case BM_BLENDFACTOR:
		return VK_BLEND_FACTOR_CONSTANT_COLOR;
	case BM_INVBLENDFACTOR:
		return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
	case 16: // D3D12_BLEND_SRC1_COLOR
		return VK_BLEND_FACTOR_SRC1_COLOR;
	case 17: // D3D12_BLEND_INV_SRC1_COLOR
		return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
	case 18: // D3D12_BLEND_SRC1_ALPHA
		return VK_BLEND_FACTOR_SRC1_ALPHA;
	case 19: // D3D12_BLEND_INV_SRC1_ALPHA
		return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
	default:
		return VK_BLEND_FACTOR_ONE;
	}
}

uint32_t AlphaBlendMode( uint32_t blendMode )
{
	switch( blendMode )
	{
	case BM_SRCCOLOR:
		return BM_SRCALPHA;
	case BM_INVSRCCOLOR:
		return BM_INVSRCALPHA;
	case BM_DESTCOLOR:
		return BM_DESTALPHA;
	case BM_INVDESTCOLOR:
		return BM_INVDESTALPHA;
	case 16:
		return 18;
	case 17:
		return 19;
	default:
		return blendMode;
	}
}

VkBlendOp ToVkBlendOp( uint32_t blendOperation )
{
	switch( blendOperation )
	{
	case BO_SUBTRACT:
		return VK_BLEND_OP_SUBTRACT;
	case BO_REVSUBTRACT:
		return VK_BLEND_OP_REVERSE_SUBTRACT;
	case BO_MIN:
		return VK_BLEND_OP_MIN;
	case BO_MAX:
		return VK_BLEND_OP_MAX;
	default:
		return VK_BLEND_OP_ADD;
	}
}

VkStencilOp ToVkStencilOp( uint32_t stencilOperation )
{
	switch( stencilOperation )
	{
	case STENCILOP_ZERO:
		return VK_STENCIL_OP_ZERO;
	case STENCILOP_REPLACE:
		return VK_STENCIL_OP_REPLACE;
	case STENCILOP_INCRSAT:
		return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
	case STENCILOP_DECRSAT:
		return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
	case STENCILOP_INVERT:
		return VK_STENCIL_OP_INVERT;
	case STENCILOP_INCR:
		return VK_STENCIL_OP_INCREMENT_AND_WRAP;
	case STENCILOP_DECR:
		return VK_STENCIL_OP_DECREMENT_AND_WRAP;
	default:
		return VK_STENCIL_OP_KEEP;
	}
}

VkSamplerAddressMode ToVkAddressMode( uint32_t addressMode )
{
	switch( addressMode )
	{
	case TA_MIRROR:
		return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	case TA_CLAMP:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	case TA_BORDER:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	case TA_MIRROR_ONCE:
		return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE; // core since Vulkan 1.2 (samplerMirrorClampToEdge)
	default:
		return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	}
}

VkSampler CreateVkSampler( VulkanDevice& device, const Tr2SamplerDescription& description )
{
	const bool anisotropic = description.m_minFilter == TF_ANISOTROPIC || description.m_magFilter == TF_ANISOTROPIC || description.m_mipFilter == TF_ANISOTROPIC;
	VkSamplerCreateInfo info{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	info.minFilter = description.m_minFilter == TF_POINT ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
	info.magFilter = description.m_magFilter == TF_POINT ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
	info.mipmapMode = description.m_mipFilter == TF_POINT || description.m_mipFilter == TF_NONE ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
	if( anisotropic )
	{
		info.minFilter = info.magFilter = VK_FILTER_LINEAR;
		info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	}
	info.addressModeU = ToVkAddressMode( description.m_addressU );
	info.addressModeV = ToVkAddressMode( description.m_addressV );
	info.addressModeW = ToVkAddressMode( description.m_addressW );
	info.mipLodBias = description.m_mipLODBias;
	info.anisotropyEnable = anisotropic && device.GetEnabledFeatures().samplerAnisotropy && description.m_maxAnisotropy > 1;
	info.maxAnisotropy = std::min( float( std::max( description.m_maxAnisotropy, 1u ) ), device.GetLimits().maxSamplerAnisotropy );
	info.compareEnable = description.m_isComparisonFilter;
	info.compareOp = ToVkCompareOp( description.m_comparisonFunc );
	info.minLod = description.m_minLOD;
	info.maxLod = description.m_mipFilter == TF_NONE ? description.m_minLOD : std::max( description.m_minLOD, description.m_maxLOD );
	if( info.maxLod >= std::numeric_limits<float>::max() )
	{
		info.maxLod = VK_LOD_CLAMP_NONE;
	}
	// As DX12's static samplers: Vulkan only has the three fixed border colours.
	if( description.m_borderColor[0] > 0 )
	{
		info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	}
	else if( description.m_borderColor[3] > 0 )
	{
		info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	}
	else
	{
		info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	}
	VkSampler sampler = VK_NULL_HANDLE;
	VkResult result = vkCreateSampler( device.GetHandle(), &info, nullptr, &sampler );
	if( result != VK_SUCCESS )
	{
		CCP_AL_LOGERR( "Vulkan: vkCreateSampler failed: %s", VkResultToString( result ) );
		return VK_NULL_HANDLE;
	}
	return sampler;
}

}

#endif

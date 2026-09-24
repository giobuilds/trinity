// Copyright © 2026 CCP ehf.

#pragma once

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "VulkanIncludes.h"
#include "../Tr2RenderContextEnum.h"

namespace TrinityALImpl
{

// The Vulkan format backing a trinity pixel format (trinity's formats are DXGI's). Typeless formats map to a
// representative typed format; images that need other views are created mutable. VK_FORMAT_UNDEFINED when Vulkan has no
// equivalent (e.g. R1_UNORM, the packed 4:2:2 formats).
VkFormat ToVkFormat( Tr2RenderContextEnum::PixelFormat format );

bool IsDepthFormat( Tr2RenderContextEnum::PixelFormat format );
bool IsStencilFormat( Tr2RenderContextEnum::PixelFormat format );
VkImageAspectFlags AspectMask( Tr2RenderContextEnum::PixelFormat format );

}

#endif

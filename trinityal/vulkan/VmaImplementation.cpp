// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

// The one VMA implementation on the Vulkan platform (trinity/Tr2VirtualAllocator.cpp skips its own here), so the
// TrinityAL tests, which do not link trinity, have it too.
#define VMA_IMPLEMENTATION
#include "VulkanIncludes.h"

#endif

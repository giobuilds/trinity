// Copyright © 2026 CCP ehf.

#pragma once

// Every Vulkan backend file includes Vulkan through this header.
// - volk loads libvulkan at runtime, so nothing links against the loader (VK_NO_PROTOTYPES comes from volk.h).
// - VMA's configuration must be identical in every translation unit that sees it, because it changes the layout of
//   VmaVulkanFunctions and friends. trinity/Tr2VirtualAllocator.cpp uses the same settings for VMA's virtual
//   allocator; on this platform the implementation itself is compiled once, in VmaImplementation.cpp.

#include <volk.h>

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#define VMA_VULKAN_VERSION 1000000
#define VMA_STATS_STRING_ENABLED 0
#include <vk_mem_alloc.h>

// Copyright © 2026 CCP ehf.

#pragma once

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "VulkanIncludes.h"
#include "../Tr2RenderContextEnum.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

struct Tr2AdapterInfo;

namespace TrinityALImpl
{

// A physical device usable by the backend: Vulkan 1.3 and a queue family with graphics and compute.
struct VulkanAdapter
{
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkPhysicalDeviceProperties properties{};
	VkPhysicalDeviceIDProperties idProperties{};
	VkPhysicalDeviceDriverProperties driverProperties{};
	uint32_t queueFamily = 0;
};

// Process-wide instance and adapter list. Created on first use; adapters can be listed without creating a device.
class VulkanInstance
{
public:
	static VulkanInstance* Get(); // nullptr when no Vulkan loader or no usable adapter is present

	VkInstance GetHandle() const
	{
		return m_instance;
	}
	const std::vector<VulkanAdapter>& GetAdapters() const
	{
		return m_adapters;
	}
	bool ValidationEnabled() const
	{
		return m_validation;
	}
	bool SurfacesEnabled() const
	{
		return m_surfaces;
	}
	static uint32_t ValidationErrorCount();

	void FillAdapterInfo( uint32_t adapter, Tr2AdapterInfo& info ) const;

	~VulkanInstance();

private:
	VulkanInstance() = default;
	bool Initialize();

	VkInstance m_instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT m_messenger = VK_NULL_HANDLE;
	bool m_validation = false;
	bool m_surfaces = false;
	std::vector<VulkanAdapter> m_adapters;
};

// One logical device, its graphics+compute queue, the VMA allocator and the frame ring.
//
// Recording model: commands go into the current frame's command buffer, which is begun lazily by
// GetCommandBuffer(). Submit() ends and submits it and moves to the next frame; before a frame slot is reused its fence
// is waited on and the resources released into it are destroyed. Flush() submits and waits for the GPU (readbacks).
class VulkanDevice
{
public:
	static constexpr uint32_t FRAMES_IN_FLIGHT = 2;

	static std::shared_ptr<VulkanDevice> Create( uint32_t adapter );
	~VulkanDevice();

	VkDevice GetHandle() const
	{
		return m_device;
	}
	VkPhysicalDevice GetPhysicalDevice() const
	{
		return m_adapter.physicalDevice;
	}
	const VulkanAdapter& GetAdapter() const
	{
		return m_adapter;
	}
	VmaAllocator GetAllocator() const
	{
		return m_allocator;
	}
	VkQueue GetQueue() const
	{
		return m_queue;
	}
	const VkPhysicalDeviceFeatures& GetEnabledFeatures() const
	{
		return m_features;
	}

	VkCommandBuffer GetCommandBuffer();
	bool IsRecording() const
	{
		return m_recording;
	}
	VkResult Submit();
	VkResult Flush();
	void WaitIdle();

	// Destroy something once the GPU can no longer be using it: after the next submitted frame has completed, which
	// implies every earlier frame has too.
	void ReleaseLater( std::function<void()> release );

	// Records a staging copy of data into dst at offset, ordered after everything recorded so far and before
	// everything recorded later (a full memory barrier on each side). The staging buffer is released later.
	bool UploadToBuffer( VkBuffer dst, VkDeviceSize offset, const void* data, VkDeviceSize size );

	// A persistently mapped host buffer for transfers: write-combined for uploads, cached for readbacks.
	struct StagingBuffer
	{
		VkBuffer buffer = VK_NULL_HANDLE;
		VmaAllocation allocation = VK_NULL_HANDLE;
		void* mapped = nullptr;
	};
	bool CreateStagingBuffer( VkDeviceSize size, bool readback, StagingBuffer& staging );
	// Destroys the buffer once the GPU is done with it (ReleaseLater) and clears the handle.
	void ReleaseStagingBuffer( StagingBuffer& staging );

	// The Vulkan format images of a trinity format are created with on this device. Usually ToVkFormat(); depth formats
	// the device cannot render to fall back to one it can (D24S8 -> D32S8, as AMD hardware has no D24).
	VkFormat GetImageFormat( Tr2RenderContextEnum::PixelFormat format ) const;

	// Frame-lifetime host memory for constants and user-pointer draws: valid until the frame it was allocated in has
	// completed on the GPU. Usable as uniform, storage, vertex, index and transfer-source data.
	struct UploadAllocation
	{
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceSize offset = 0;
		void* data = nullptr;
	};
	bool AllocateUpload( VkDeviceSize size, VkDeviceSize alignment, UploadAllocation& allocation );

	// A descriptor set from the recording frame's pools; freed (by pool reset) when the frame slot is reused.
	VkDescriptorSet AllocateDescriptorSet( VkDescriptorSetLayout layout );

	// The render context keeps a dynamic-rendering scope open across draws. Anything recorded outside a render pass
	// (copies, barriers, dispatches, submits) must close it first; the device calls this hook before doing so.
	void SetRenderingEndHook( std::function<void()> hook )
	{
		m_renderingEndHook = std::move( hook );
	}
	void EndRendering()
	{
		if( m_renderingEndHook )
		{
			m_renderingEndHook();
		}
	}

	// 64 zeroed bytes, bound as a stride-0 vertex stream for shader inputs the vertex layout does not provide.
	VkBuffer GetZeroBuffer() const
	{
		return m_zeroBuffer;
	}
	bool HasNullDescriptors() const
	{
		return m_nullDescriptors;
	}
	const VkPhysicalDeviceLimits& GetLimits() const
	{
		return m_adapter.properties.limits;
	}

	// A full pipeline/memory barrier in the current command buffer: every earlier write is visible to every later
	// access. Coarse; used until per-resource state tracking lands.
	void RecordFullBarrier();

	// Make the GPU's writes visible to the CPU and ensure the GPU no longer reads memory the CPU is about to write:
	// submit what is recorded and wait for all submitted work.
	void SynchronizeForCpuAccess();

	// Debug name for validation messages and tools (only with the debug-utils extension, i.e. validation enabled).
	void SetObjectName( VkObjectType type, uint64_t handle, const char* name );

	uint64_t GetSubmittedFrameCount() const
	{
		return m_submittedFrames;
	}

	// Submissions are numbered 1, 2, ... Work recorded now goes out with GetRecordingSerial(); a serial is complete once
	// the GPU has finished it (and, the queue being in order, everything before it).
	uint64_t GetRecordingSerial() const
	{
		return m_submittedFrames + 1;
	}
	uint64_t GetCompletedSerial(); // polls the frame fences
	// True when everything recorded before the serial was taken has completed: the serial itself, or, if nothing has
	// been recorded since it was taken, all submitted work.
	bool IsSerialComplete( uint64_t serial );
	// Submits if the serial is still being recorded, then waits for it. False if the device failed.
	bool WaitForSerial( uint64_t serial );

	// Semaphores the next Submit waits on / signals (swapchain acquire and present).
	void AddSubmitWait( VkSemaphore semaphore, VkPipelineStageFlags2 stages );
	void AddSubmitSignal( VkSemaphore semaphore );
	bool SupportsSwapchains() const
	{
		return m_swapchains;
	}
	uint32_t GetQueueFamily() const
	{
		return m_adapter.queueFamily;
	}

	uint32_t GetTimestampValidBits() const
	{
		return m_timestampValidBits;
	}

private:
	VulkanDevice() = default;
	bool Initialize( uint32_t adapter );
	void WaitForFrame( uint32_t frameIndex );

	struct Frame
	{
		VkCommandPool pool = VK_NULL_HANDLE;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		VkFence fence = VK_NULL_HANDLE;
		bool submitted = false;
		uint64_t serial = 0;
		std::vector<std::function<void()>> releases;
		// Upload chunks and descriptor pools, reset when this slot starts recording again.
		std::vector<StagingBuffer> uploadChunks;
		std::vector<VkDeviceSize> uploadChunkSizes;
		size_t uploadChunk = 0;
		VkDeviceSize uploadOffset = 0;
		std::vector<VkDescriptorPool> descriptorPools;
		size_t descriptorPool = 0;
	};
	void ResetFrameAllocators( Frame& frame );
	void FlushUploads( Frame& frame );

	VulkanAdapter m_adapter;
	VkPhysicalDeviceFeatures m_features{};
	bool m_hasD24S8 = true;
	bool m_nullDescriptors = false;
	bool m_swapchains = false;
	std::vector<VkSemaphoreSubmitInfo> m_submitWaits;
	std::vector<VkSemaphoreSubmitInfo> m_submitSignals;
	std::function<void()> m_renderingEndHook;
	VkBuffer m_zeroBuffer = VK_NULL_HANDLE;
	VmaAllocation m_zeroBufferAllocation = VK_NULL_HANDLE;
	VkDevice m_device = VK_NULL_HANDLE;
	VkQueue m_queue = VK_NULL_HANDLE;
	VmaAllocator m_allocator = VK_NULL_HANDLE;
	Frame m_frames[FRAMES_IN_FLIGHT];
	// Released since the last submit: they may be referenced by work recorded so far or by frames still in flight, so
	// they are attached to the next submitted frame and run once its fence has signalled.
	std::vector<std::function<void()>> m_pendingReleases;
	uint32_t m_frameIndex = 0;
	bool m_recording = false;
	uint64_t m_submittedFrames = 0;
	uint64_t m_completedSerial = 0;
	uint32_t m_timestampValidBits = 0;
};

const char* VkResultToString( VkResult result );

}

#endif

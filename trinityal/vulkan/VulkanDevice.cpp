// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#if ( TRINITY_PLATFORM == TRINITY_VULKAN )

#include "VulkanDevice.h"
#include "ALLog.h"
#include "Tr2AdapterStructures.h"
#include "VulkanFormats.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>

namespace TrinityALImpl
{

namespace
{

std::atomic<uint32_t> s_validationErrors{ 0 };

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT,
	const VkDebugUtilsMessengerCallbackDataEXT* data,
	void* )
{
	if( severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT )
	{
		++s_validationErrors;
		CCP_AL_LOGERR( "Vulkan: %s", data->pMessage );
		fprintf( stderr, "Vulkan validation error: %s\n", data->pMessage ); // visible in test output as well as the log
	}
	else if( severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT )
	{
		CCP_AL_LOGWARN( "Vulkan: %s", data->pMessage );
	}
	return VK_FALSE;
}

bool HasLayer( const char* name )
{
	uint32_t count = 0;
	vkEnumerateInstanceLayerProperties( &count, nullptr );
	std::vector<VkLayerProperties> layers( count );
	vkEnumerateInstanceLayerProperties( &count, layers.data() );
	return std::any_of( layers.begin(), layers.end(), [name]( const VkLayerProperties& l ) { return strcmp( l.layerName, name ) == 0; } );
}

bool HasInstanceExtension( const char* name )
{
	uint32_t count = 0;
	vkEnumerateInstanceExtensionProperties( nullptr, &count, nullptr );
	std::vector<VkExtensionProperties> extensions( count );
	vkEnumerateInstanceExtensionProperties( nullptr, &count, extensions.data() );
	return std::any_of( extensions.begin(), extensions.end(), [name]( const VkExtensionProperties& e ) { return strcmp( e.extensionName, name ) == 0; } );
}

// Validation is on in non-Release builds when the layer is installed; CARBON_VULKAN_VALIDATION=0/1 overrides.
bool WantValidation()
{
	if( const char* env = getenv( "CARBON_VULKAN_VALIDATION" ) )
	{
		return atoi( env ) != 0;
	}
#if CCP_ASSERT_ENABLED
	return true;
#else
	return false;
#endif
}

int DeviceTypeRank( VkPhysicalDeviceType type )
{
	switch( type )
	{
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
		return 0;
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
		return 1;
	case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
		return 2;
	case VK_PHYSICAL_DEVICE_TYPE_CPU:
		return 3;
	default:
		return 4;
	}
}

// CARBON_VULKAN_DEVICE selects the adapter list's first entry: an index into the physical devices, or a
// case-insensitive substring of the device name (e.g. "llvmpipe" for lavapipe, "RADV").
void ApplyDeviceOverride( std::vector<VulkanAdapter>& adapters )
{
	const char* env = getenv( "CARBON_VULKAN_DEVICE" );
	if( !env || !*env || adapters.empty() )
	{
		return;
	}
	char* end = nullptr;
	long index = strtol( env, &end, 10 );
	size_t chosen = adapters.size();
	if( end && *end == 0 && index >= 0 && size_t( index ) < adapters.size() )
	{
		chosen = size_t( index );
	}
	else
	{
		std::string wanted = env;
		std::transform( wanted.begin(), wanted.end(), wanted.begin(), ::tolower );
		for( size_t i = 0; i < adapters.size(); ++i )
		{
			std::string name = adapters[i].properties.deviceName;
			std::transform( name.begin(), name.end(), name.begin(), ::tolower );
			if( name.find( wanted ) != std::string::npos )
			{
				chosen = i;
				break;
			}
		}
	}
	if( chosen < adapters.size() )
	{
		std::rotate( adapters.begin(), adapters.begin() + chosen, adapters.begin() + chosen + 1 );
	}
	else
	{
		CCP_AL_LOGWARN( "Vulkan: CARBON_VULKAN_DEVICE=%s matches no adapter", env );
	}
}

} // namespace

const char* VkResultToString( VkResult result )
{
	switch( result )
	{
	case VK_SUCCESS:
		return "VK_SUCCESS";
	case VK_NOT_READY:
		return "VK_NOT_READY";
	case VK_TIMEOUT:
		return "VK_TIMEOUT";
	case VK_ERROR_OUT_OF_HOST_MEMORY:
		return "VK_ERROR_OUT_OF_HOST_MEMORY";
	case VK_ERROR_OUT_OF_DEVICE_MEMORY:
		return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
	case VK_ERROR_INITIALIZATION_FAILED:
		return "VK_ERROR_INITIALIZATION_FAILED";
	case VK_ERROR_DEVICE_LOST:
		return "VK_ERROR_DEVICE_LOST";
	case VK_ERROR_LAYER_NOT_PRESENT:
		return "VK_ERROR_LAYER_NOT_PRESENT";
	case VK_ERROR_EXTENSION_NOT_PRESENT:
		return "VK_ERROR_EXTENSION_NOT_PRESENT";
	case VK_ERROR_FEATURE_NOT_PRESENT:
		return "VK_ERROR_FEATURE_NOT_PRESENT";
	case VK_ERROR_INCOMPATIBLE_DRIVER:
		return "VK_ERROR_INCOMPATIBLE_DRIVER";
	case VK_ERROR_FORMAT_NOT_SUPPORTED:
		return "VK_ERROR_FORMAT_NOT_SUPPORTED";
	default:
		return "VkResult(other)";
	}
}

// ------------------------------------------------------------------------------------------------------------------
// VulkanInstance

VulkanInstance* VulkanInstance::Get()
{
	// Deliberately never destroyed: at process exit, static destructors run after (or interleaved with) the
	// validation layer's and loader's own teardown, and vkDestroyInstance then crashes inside the layer. Devices are
	// destroyed explicitly; the instance is reclaimed with the process.
	static VulkanInstance* s_instance = nullptr;
	static bool s_tried = false;
	if( !s_tried )
	{
		s_tried = true;
		std::unique_ptr<VulkanInstance> instance( new VulkanInstance() );
		if( instance->Initialize() )
		{
			s_instance = instance.release();
		}
	}
	return s_instance;
}

uint32_t VulkanInstance::ValidationErrorCount()
{
	return s_validationErrors.load();
}

bool VulkanInstance::Initialize()
{
	if( volkInitialize() != VK_SUCCESS )
	{
		CCP_AL_LOGWARN( "Vulkan: no Vulkan loader (libvulkan.so.1) found" );
		return false;
	}
	if( volkGetInstanceVersion() < VK_API_VERSION_1_3 )
	{
		CCP_AL_LOGWARN( "Vulkan: the loader supports only Vulkan %u.%u; 1.3 is required", VK_API_VERSION_MAJOR( volkGetInstanceVersion() ), VK_API_VERSION_MINOR( volkGetInstanceVersion() ) );
		return false;
	}

	const char* validationLayer = "VK_LAYER_KHRONOS_validation";
	m_validation = WantValidation() && HasLayer( validationLayer ) && HasInstanceExtension( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );

	std::vector<const char*> layers;
	std::vector<const char*> extensions;
	if( m_validation )
	{
		layers.push_back( validationLayer );
		extensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
	}

	VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
	appInfo.pApplicationName = "Carbon";
	appInfo.pEngineName = "Trinity";
	appInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo createInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledLayerCount = uint32_t( layers.size() );
	createInfo.ppEnabledLayerNames = layers.data();
	createInfo.enabledExtensionCount = uint32_t( extensions.size() );
	createInfo.ppEnabledExtensionNames = extensions.data();

	VkResult result = vkCreateInstance( &createInfo, nullptr, &m_instance );
	if( result != VK_SUCCESS )
	{
		CCP_AL_LOGERR( "Vulkan: vkCreateInstance failed: %s", VkResultToString( result ) );
		return false;
	}
	volkLoadInstanceOnly( m_instance );

	if( m_validation )
	{
		VkDebugUtilsMessengerCreateInfoEXT messengerInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		messengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		messengerInfo.pfnUserCallback = DebugCallback;
		vkCreateDebugUtilsMessengerEXT( m_instance, &messengerInfo, nullptr, &m_messenger );
		CCP_AL_LOG( "Vulkan: validation layer enabled" );
	}

	uint32_t count = 0;
	vkEnumeratePhysicalDevices( m_instance, &count, nullptr );
	std::vector<VkPhysicalDevice> devices( count );
	vkEnumeratePhysicalDevices( m_instance, &count, devices.data() );
	for( auto device : devices )
	{
		VulkanAdapter adapter;
		adapter.physicalDevice = device;
		adapter.driverProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
		adapter.idProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
		adapter.idProperties.pNext = &adapter.driverProperties;
		VkPhysicalDeviceProperties2 properties2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		properties2.pNext = &adapter.idProperties;
		vkGetPhysicalDeviceProperties2( device, &properties2 );
		adapter.properties = properties2.properties;
		adapter.idProperties.pNext = nullptr;
		adapter.driverProperties.pNext = nullptr;

		if( adapter.properties.apiVersion < VK_API_VERSION_1_3 )
		{
			CCP_AL_LOG( "Vulkan: skipping %s (Vulkan %u.%u)", adapter.properties.deviceName, VK_API_VERSION_MAJOR( adapter.properties.apiVersion ), VK_API_VERSION_MINOR( adapter.properties.apiVersion ) );
			continue;
		}

		uint32_t familyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties( device, &familyCount, nullptr );
		std::vector<VkQueueFamilyProperties> families( familyCount );
		vkGetPhysicalDeviceQueueFamilyProperties( device, &familyCount, families.data() );
		bool found = false;
		for( uint32_t i = 0; i < familyCount; ++i )
		{
			const VkQueueFlags needed = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
			if( ( families[i].queueFlags & needed ) == needed )
			{
				adapter.queueFamily = i;
				found = true;
				break;
			}
		}
		if( !found )
		{
			continue;
		}
		m_adapters.push_back( adapter );
	}

	std::stable_sort( m_adapters.begin(), m_adapters.end(), []( const VulkanAdapter& a, const VulkanAdapter& b ) {
		return DeviceTypeRank( a.properties.deviceType ) < DeviceTypeRank( b.properties.deviceType );
	} );
	ApplyDeviceOverride( m_adapters );

	for( auto& adapter : m_adapters )
	{
		CCP_AL_LOG( "Vulkan: adapter %s (%s)", adapter.properties.deviceName, adapter.driverProperties.driverInfo );
	}
	return true;
}

VulkanInstance::~VulkanInstance()
{
	if( m_messenger )
	{
		vkDestroyDebugUtilsMessengerEXT( m_instance, m_messenger, nullptr );
	}
	if( m_instance )
	{
		vkDestroyInstance( m_instance, nullptr );
	}
}

void VulkanInstance::FillAdapterInfo( uint32_t index, Tr2AdapterInfo& info ) const
{
	const VulkanAdapter& adapter = m_adapters[index];
	info.driver = adapter.driverProperties.driverName;
	info.deviceName = adapter.properties.deviceName;
	std::string description = std::string( adapter.properties.deviceName ) + " (" + adapter.driverProperties.driverInfo + ")";
	info.description = std::wstring( description.begin(), description.end() );
	info.driverVersion = int64_t( adapter.properties.driverVersion );
	info.vendorID = adapter.properties.vendorID;
	info.deviceID = adapter.properties.deviceID;
	info.subSystemID = 0;
	info.revision = 0;
	const uint8_t* uuid = adapter.idProperties.deviceUUID;
	memcpy( &info.deviceIdentifier.data1, uuid, 4 );
	memcpy( &info.deviceIdentifier.data2, uuid + 4, 2 );
	memcpy( &info.deviceIdentifier.data3, uuid + 6, 2 );
	memcpy( info.deviceIdentifier.data4, uuid + 8, 8 );
	if( adapter.idProperties.deviceLUIDValid )
	{
		memcpy( info.luid, adapter.idProperties.deviceLUID, 8 );
	}
	else
	{
		memset( info.luid, 0, sizeof( info.luid ) );
	}
}

// ------------------------------------------------------------------------------------------------------------------
// VulkanDevice

std::shared_ptr<VulkanDevice> VulkanDevice::Create( uint32_t adapter )
{
	std::shared_ptr<VulkanDevice> device( new VulkanDevice() );
	if( !device->Initialize( adapter ) )
	{
		return nullptr;
	}
	return device;
}

bool VulkanDevice::Initialize( uint32_t adapterIndex )
{
	VulkanInstance* instance = VulkanInstance::Get();
	if( !instance || adapterIndex >= instance->GetAdapters().size() )
	{
		return false;
	}
	m_adapter = instance->GetAdapters()[adapterIndex];

	// Core 1.0 features the engine can use, when present.
	VkPhysicalDeviceFeatures available{};
	vkGetPhysicalDeviceFeatures( m_adapter.physicalDevice, &available );
	{
		VkFormatProperties d24{};
		vkGetPhysicalDeviceFormatProperties( m_adapter.physicalDevice, VK_FORMAT_D24_UNORM_S8_UINT, &d24 );
		m_hasD24S8 = ( d24.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT ) != 0;
	}
	m_features.samplerAnisotropy = available.samplerAnisotropy;
	m_features.textureCompressionBC = available.textureCompressionBC;
	m_features.geometryShader = available.geometryShader;
	m_features.tessellationShader = available.tessellationShader;
	m_features.independentBlend = available.independentBlend;
	m_features.fillModeNonSolid = available.fillModeNonSolid;
	m_features.depthClamp = available.depthClamp;
	m_features.imageCubeArray = available.imageCubeArray;
	m_features.fragmentStoresAndAtomics = available.fragmentStoresAndAtomics;
	m_features.vertexPipelineStoresAndAtomics = available.vertexPipelineStoresAndAtomics;
	m_features.shaderStorageImageWriteWithoutFormat = available.shaderStorageImageWriteWithoutFormat;
	m_features.shaderStorageImageReadWithoutFormat = available.shaderStorageImageReadWithoutFormat;
	m_features.shaderImageGatherExtended = available.shaderImageGatherExtended;
	m_features.sampleRateShading = available.sampleRateShading;
	m_features.dualSrcBlend = available.dualSrcBlend;
	m_features.occlusionQueryPrecise = available.occlusionQueryPrecise;
	m_features.pipelineStatisticsQuery = available.pipelineStatisticsQuery;
	m_features.multiDrawIndirect = available.multiDrawIndirect;
	m_features.drawIndirectFirstInstance = available.drawIndirectFirstInstance;

	// Vulkan 1.2/1.3 features the backend relies on.
	VkPhysicalDeviceVulkan13Features features13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	features13.dynamicRendering = VK_TRUE;
	features13.synchronization2 = VK_TRUE;
	VkPhysicalDeviceVulkan12Features features12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	features12.pNext = &features13;
	features12.timelineSemaphore = VK_TRUE;
	features12.hostQueryReset = VK_TRUE;
	{
		VkPhysicalDeviceVulkan12Features available12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
		VkPhysicalDeviceFeatures2 query{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		query.pNext = &available12;
		vkGetPhysicalDeviceFeatures2( m_adapter.physicalDevice, &query );
		features12.samplerMirrorClampToEdge = available12.samplerMirrorClampToEdge; // TA_MIRROR_ONCE
	}
	VkPhysicalDeviceFeatures2 features2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
	features2.pNext = &features12;
	features2.features = m_features;

	// Null descriptors (VK_EXT_robustness2) let unbound resource slots read zero, as unbound D3D slots do.
	std::vector<const char*> extensions;
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties( m_adapter.physicalDevice, nullptr, &extensionCount, nullptr );
	std::vector<VkExtensionProperties> availableExtensions( extensionCount );
	vkEnumerateDeviceExtensionProperties( m_adapter.physicalDevice, nullptr, &extensionCount, availableExtensions.data() );
	auto hasExtension = [&]( const char* name ) {
		return std::any_of( availableExtensions.begin(), availableExtensions.end(), [name]( const VkExtensionProperties& p ) { return strcmp( p.extensionName, name ) == 0; } );
	};
	VkPhysicalDeviceRobustness2FeaturesEXT robustness2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT };
	if( hasExtension( VK_EXT_ROBUSTNESS_2_EXTENSION_NAME ) )
	{
		VkPhysicalDeviceRobustness2FeaturesEXT query{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT };
		VkPhysicalDeviceFeatures2 queryFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		queryFeatures.pNext = &query;
		vkGetPhysicalDeviceFeatures2( m_adapter.physicalDevice, &queryFeatures );
		if( query.nullDescriptor )
		{
			robustness2.nullDescriptor = VK_TRUE;
			robustness2.pNext = features2.pNext;
			features2.pNext = &robustness2;
			extensions.push_back( VK_EXT_ROBUSTNESS_2_EXTENSION_NAME );
			m_nullDescriptors = true;
		}
	}
	if( !m_nullDescriptors )
	{
		CCP_AL_LOGWARN( "Vulkan: %s has no null descriptors; unbound resource slots are undefined", m_adapter.properties.deviceName );
	}

	float priority = 1.0f;
	VkDeviceQueueCreateInfo queueInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	queueInfo.queueFamilyIndex = m_adapter.queueFamily;
	queueInfo.queueCount = 1;
	queueInfo.pQueuePriorities = &priority;

	VkDeviceCreateInfo createInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	createInfo.pNext = &features2;
	createInfo.queueCreateInfoCount = 1;
	createInfo.pQueueCreateInfos = &queueInfo;
	createInfo.enabledExtensionCount = uint32_t( extensions.size() );
	createInfo.ppEnabledExtensionNames = extensions.data();

	VkResult result = vkCreateDevice( m_adapter.physicalDevice, &createInfo, nullptr, &m_device );
	if( result != VK_SUCCESS )
	{
		CCP_AL_LOGERR( "Vulkan: vkCreateDevice failed on %s: %s", m_adapter.properties.deviceName, VkResultToString( result ) );
		return false;
	}
	volkLoadDevice( m_device );
	vkGetDeviceQueue( m_device, m_adapter.queueFamily, 0, &m_queue );
	{
		uint32_t familyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties( m_adapter.physicalDevice, &familyCount, nullptr );
		std::vector<VkQueueFamilyProperties> families( familyCount );
		vkGetPhysicalDeviceQueueFamilyProperties( m_adapter.physicalDevice, &familyCount, families.data() );
		m_timestampValidBits = families[m_adapter.queueFamily].timestampValidBits;
	}

	// VMA is built without its own function loading (see VulkanIncludes.h), so it gets volk's pointers.
	VmaVulkanFunctions functions{};
	functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
	functions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
	functions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
	functions.vkAllocateMemory = vkAllocateMemory;
	functions.vkFreeMemory = vkFreeMemory;
	functions.vkMapMemory = vkMapMemory;
	functions.vkUnmapMemory = vkUnmapMemory;
	functions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
	functions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
	functions.vkBindBufferMemory = vkBindBufferMemory;
	functions.vkBindImageMemory = vkBindImageMemory;
	functions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
	functions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
	functions.vkCreateBuffer = vkCreateBuffer;
	functions.vkDestroyBuffer = vkDestroyBuffer;
	functions.vkCreateImage = vkCreateImage;
	functions.vkDestroyImage = vkDestroyImage;
	functions.vkCmdCopyBuffer = vkCmdCopyBuffer;

	VmaAllocatorCreateInfo allocatorInfo{};
	allocatorInfo.physicalDevice = m_adapter.physicalDevice;
	allocatorInfo.device = m_device;
	allocatorInfo.instance = VulkanInstance::Get()->GetHandle();
	allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_0; // matches VMA_VULKAN_VERSION
	allocatorInfo.pVulkanFunctions = &functions;
	result = vmaCreateAllocator( &allocatorInfo, &m_allocator );
	if( result != VK_SUCCESS )
	{
		CCP_AL_LOGERR( "Vulkan: vmaCreateAllocator failed: %s", VkResultToString( result ) );
		return false;
	}

	for( auto& frame : m_frames )
	{
		VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		poolInfo.queueFamilyIndex = m_adapter.queueFamily;
		if( vkCreateCommandPool( m_device, &poolInfo, nullptr, &frame.pool ) != VK_SUCCESS )
		{
			return false;
		}
		VkCommandBufferAllocateInfo allocateInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		allocateInfo.commandPool = frame.pool;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandBufferCount = 1;
		if( vkAllocateCommandBuffers( m_device, &allocateInfo, &frame.commandBuffer ) != VK_SUCCESS )
		{
			return false;
		}
		VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		if( vkCreateFence( m_device, &fenceInfo, nullptr, &frame.fence ) != VK_SUCCESS )
		{
			return false;
		}
	}

	{
		VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferInfo.size = 64;
		bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		VmaAllocationCreateInfo zeroInfo{};
		zeroInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		if( vmaCreateBuffer( m_allocator, &bufferInfo, &zeroInfo, &m_zeroBuffer, &m_zeroBufferAllocation, nullptr ) != VK_SUCCESS )
		{
			return false;
		}
		vkCmdFillBuffer( GetCommandBuffer(), m_zeroBuffer, 0, VK_WHOLE_SIZE, 0 );
		RecordFullBarrier();
	}

	CCP_AL_LOG( "Vulkan: device created on %s", m_adapter.properties.deviceName );
	if( const char* verbose = getenv( "CARBON_VULKAN_VERBOSE" ); verbose && atoi( verbose ) )
	{
		fprintf( stderr, "Vulkan: device created on %s (%s), validation %s\n", m_adapter.properties.deviceName, m_adapter.driverProperties.driverInfo, VulkanInstance::Get()->ValidationEnabled() ? "on" : "off" );
	}
	return true;
}

VulkanDevice::~VulkanDevice()
{
	if( !m_device )
	{
		return;
	}
	if( m_recording )
	{
		Submit();
	}
	WaitIdle();
	// Released after the last submit (typically by the last resource holding the device): the GPU is idle, so now.
	for( auto& release : m_pendingReleases )
	{
		release();
	}
	m_pendingReleases.clear();
	for( auto& frame : m_frames )
	{
		for( auto& chunk : frame.uploadChunks )
		{
			vmaDestroyBuffer( m_allocator, chunk.buffer, chunk.allocation );
		}
		for( auto pool : frame.descriptorPools )
		{
			vkDestroyDescriptorPool( m_device, pool, nullptr );
		}
		if( frame.fence )
		{
			vkDestroyFence( m_device, frame.fence, nullptr );
		}
		if( frame.pool )
		{
			vkDestroyCommandPool( m_device, frame.pool, nullptr );
		}
	}
	if( m_zeroBuffer )
	{
		vmaDestroyBuffer( m_allocator, m_zeroBuffer, m_zeroBufferAllocation );
	}
	if( m_allocator )
	{
		vmaDestroyAllocator( m_allocator );
	}
	vkDestroyDevice( m_device, nullptr );
}

void VulkanDevice::WaitForFrame( uint32_t frameIndex )
{
	Frame& frame = m_frames[frameIndex];
	if( frame.submitted )
	{
		vkWaitForFences( m_device, 1, &frame.fence, VK_TRUE, UINT64_MAX );
		frame.submitted = false;
		m_completedSerial = std::max( m_completedSerial, frame.serial );
	}
	for( auto& release : frame.releases )
	{
		release();
	}
	frame.releases.clear();
}

VkCommandBuffer VulkanDevice::GetCommandBuffer()
{
	Frame& frame = m_frames[m_frameIndex];
	if( !m_recording )
	{
		WaitForFrame( m_frameIndex );
		vkResetCommandPool( m_device, frame.pool, 0 );
		ResetFrameAllocators( frame );
		VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer( frame.commandBuffer, &beginInfo );
		m_recording = true;
	}
	return frame.commandBuffer;
}

VkResult VulkanDevice::Submit()
{
	Frame& frame = m_frames[m_frameIndex];
	if( !m_recording )
	{
		return VK_SUCCESS;
	}
	EndRendering();
	FlushUploads( frame );
	m_recording = false;
	VkResult result = vkEndCommandBuffer( frame.commandBuffer );
	if( result != VK_SUCCESS )
	{
		CCP_AL_LOGERR( "Vulkan: vkEndCommandBuffer failed: %s", VkResultToString( result ) );
		return result;
	}
	vkResetFences( m_device, 1, &frame.fence );
	VkCommandBufferSubmitInfo commandInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
	commandInfo.commandBuffer = frame.commandBuffer;
	VkSubmitInfo2 submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
	submitInfo.commandBufferInfoCount = 1;
	submitInfo.pCommandBufferInfos = &commandInfo;
	result = vkQueueSubmit2( m_queue, 1, &submitInfo, frame.fence );
	if( result != VK_SUCCESS )
	{
		CCP_AL_LOGERR( "Vulkan: vkQueueSubmit2 failed: %s", VkResultToString( result ) );
		return result;
	}
	frame.submitted = true;
	frame.serial = m_submittedFrames + 1;
	frame.releases.insert( frame.releases.end(), std::make_move_iterator( m_pendingReleases.begin() ), std::make_move_iterator( m_pendingReleases.end() ) );
	m_pendingReleases.clear();
	++m_submittedFrames;
	m_frameIndex = ( m_frameIndex + 1 ) % FRAMES_IN_FLIGHT;
	return VK_SUCCESS;
}

VkResult VulkanDevice::Flush()
{
	uint32_t submittedIndex = m_frameIndex;
	VkResult result = Submit();
	if( result != VK_SUCCESS )
	{
		return result;
	}
	WaitForFrame( submittedIndex );
	return VK_SUCCESS;
}

void VulkanDevice::WaitIdle()
{
	vkDeviceWaitIdle( m_device );
	m_completedSerial = m_submittedFrames;
	for( uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i )
	{
		m_frames[i].submitted = false;
		WaitForFrame( i );
	}
	if( !m_recording ) // nothing recorded can still reference them
	{
		for( auto& release : m_pendingReleases )
		{
			release();
		}
		m_pendingReleases.clear();
	}
}

uint64_t VulkanDevice::GetCompletedSerial()
{
	for( uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i )
	{
		if( m_frames[i].submitted && vkGetFenceStatus( m_device, m_frames[i].fence ) == VK_SUCCESS )
		{
			WaitForFrame( i ); // signalled: returns at once and runs the frame's releases
		}
	}
	return m_completedSerial;
}

bool VulkanDevice::IsSerialComplete( uint64_t serial )
{
	uint64_t completed = GetCompletedSerial();
	if( completed >= serial )
	{
		return true;
	}
	return serial > m_submittedFrames && !m_recording && completed == m_submittedFrames;
}

bool VulkanDevice::WaitForSerial( uint64_t serial )
{
	if( serial > m_submittedFrames )
	{
		if( !m_recording )
		{
			return true; // nothing was recorded after the serial was taken, so there is nothing to wait for
		}
		if( Submit() != VK_SUCCESS )
		{
			return false;
		}
	}
	for( uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i )
	{
		if( m_frames[i].submitted && m_frames[i].serial <= serial )
		{
			WaitForFrame( i );
		}
	}
	return true;
}

void VulkanDevice::ReleaseLater( std::function<void()> release )
{
	m_pendingReleases.push_back( std::move( release ) );
}

bool VulkanDevice::CreateStagingBuffer( VkDeviceSize size, bool readback, StagingBuffer& staging )
{
	staging = StagingBuffer();
	VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.size = std::max<VkDeviceSize>( size, 4 );
	bufferInfo.usage = readback ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	allocationInfo.usage = readback ? VMA_MEMORY_USAGE_GPU_TO_CPU : VMA_MEMORY_USAGE_CPU_ONLY;
	VmaAllocationInfo info{};
	VkResult result = vmaCreateBuffer( m_allocator, &bufferInfo, &allocationInfo, &staging.buffer, &staging.allocation, &info );
	if( result != VK_SUCCESS )
	{
		CCP_AL_LOGERR( "Vulkan: staging buffer of %llu bytes failed: %s", (unsigned long long)size, VkResultToString( result ) );
		staging = StagingBuffer();
		return false;
	}
	staging.mapped = info.pMappedData;
	return true;
}

void VulkanDevice::ReleaseStagingBuffer( StagingBuffer& staging )
{
	if( staging.buffer )
	{
		VmaAllocator allocator = m_allocator;
		VkBuffer buffer = staging.buffer;
		VmaAllocation allocation = staging.allocation;
		ReleaseLater( [allocator, buffer, allocation] { vmaDestroyBuffer( allocator, buffer, allocation ); } );
	}
	staging = StagingBuffer();
}

VkFormat VulkanDevice::GetImageFormat( Tr2RenderContextEnum::PixelFormat format ) const
{
	VkFormat vkFormat = ToVkFormat( format );
	if( vkFormat == VK_FORMAT_D24_UNORM_S8_UINT && !m_hasD24S8 )
	{
		return VK_FORMAT_D32_SFLOAT_S8_UINT;
	}
	return vkFormat;
}

bool VulkanDevice::UploadToBuffer( VkBuffer dst, VkDeviceSize offset, const void* data, VkDeviceSize size )
{
	if( size == 0 )
	{
		return true;
	}
	StagingBuffer staging;
	if( !CreateStagingBuffer( size, false, staging ) )
	{
		return false;
	}
	memcpy( staging.mapped, data, size_t( size ) );
	vmaFlushAllocation( m_allocator, staging.allocation, 0, VK_WHOLE_SIZE );

	VkCommandBuffer commandBuffer = GetCommandBuffer();
	RecordFullBarrier();
	VkBufferCopy region{ 0, offset, size };
	vkCmdCopyBuffer( commandBuffer, staging.buffer, dst, 1, &region );
	RecordFullBarrier();

	ReleaseStagingBuffer( staging );
	return true;
}

void VulkanDevice::ResetFrameAllocators( Frame& frame )
{
	frame.uploadChunk = 0;
	frame.uploadOffset = 0;
	for( auto pool : frame.descriptorPools )
	{
		vkResetDescriptorPool( m_device, pool, 0 );
	}
	frame.descriptorPool = 0;
}

void VulkanDevice::FlushUploads( Frame& frame )
{
	for( size_t i = 0; i <= frame.uploadChunk && i < frame.uploadChunks.size(); ++i )
	{
		vmaFlushAllocation( m_allocator, frame.uploadChunks[i].allocation, 0, VK_WHOLE_SIZE );
	}
}

bool VulkanDevice::AllocateUpload( VkDeviceSize size, VkDeviceSize alignment, UploadAllocation& allocation )
{
	static const VkDeviceSize CHUNK_SIZE = 4 * 1024 * 1024;
	GetCommandBuffer(); // the allocation belongs to the recording frame
	Frame& frame = m_frames[m_frameIndex];
	alignment = std::max<VkDeviceSize>( alignment, 16 );
	for( ;; )
	{
		if( frame.uploadChunk < frame.uploadChunks.size() )
		{
			VkDeviceSize offset = ( frame.uploadOffset + alignment - 1 ) / alignment * alignment;
			if( offset + size <= frame.uploadChunkSizes[frame.uploadChunk] )
			{
				auto& chunk = frame.uploadChunks[frame.uploadChunk];
				allocation.buffer = chunk.buffer;
				allocation.offset = offset;
				allocation.data = static_cast<uint8_t*>( chunk.mapped ) + offset;
				frame.uploadOffset = offset + size;
				return true;
			}
			if( frame.uploadChunk + 1 < frame.uploadChunks.size() && size <= frame.uploadChunkSizes[frame.uploadChunk + 1] )
			{
				++frame.uploadChunk;
				frame.uploadOffset = 0;
				continue;
			}
		}
		// A new chunk after the current one (chunks past it are empty and kept for the next frames).
		const VkDeviceSize chunkSize = std::max( CHUNK_SIZE, size );
		VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferInfo.size = chunkSize;
		bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
		VmaAllocationCreateInfo allocationInfo{};
		allocationInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		allocationInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
		StagingBuffer chunk;
		VmaAllocationInfo info{};
		VkResult result = vmaCreateBuffer( m_allocator, &bufferInfo, &allocationInfo, &chunk.buffer, &chunk.allocation, &info );
		if( result != VK_SUCCESS )
		{
			CCP_AL_LOGERR( "Vulkan: upload chunk of %llu bytes failed: %s", (unsigned long long)chunkSize, VkResultToString( result ) );
			return false;
		}
		chunk.mapped = info.pMappedData;
		const size_t insertAt = frame.uploadChunks.empty() ? 0 : frame.uploadChunk + 1;
		frame.uploadChunks.insert( frame.uploadChunks.begin() + insertAt, chunk );
		frame.uploadChunkSizes.insert( frame.uploadChunkSizes.begin() + insertAt, chunkSize );
		frame.uploadChunk = insertAt;
		frame.uploadOffset = 0;
	}
}

VkDescriptorSet VulkanDevice::AllocateDescriptorSet( VkDescriptorSetLayout layout )
{
	GetCommandBuffer();
	Frame& frame = m_frames[m_frameIndex];
	for( ;; )
	{
		if( frame.descriptorPool < frame.descriptorPools.size() )
		{
			VkDescriptorSetAllocateInfo allocateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
			allocateInfo.descriptorPool = frame.descriptorPools[frame.descriptorPool];
			allocateInfo.descriptorSetCount = 1;
			allocateInfo.pSetLayouts = &layout;
			VkDescriptorSet set = VK_NULL_HANDLE;
			VkResult result = vkAllocateDescriptorSets( m_device, &allocateInfo, &set );
			if( result == VK_SUCCESS )
			{
				return set;
			}
			if( result != VK_ERROR_OUT_OF_POOL_MEMORY && result != VK_ERROR_FRAGMENTED_POOL )
			{
				CCP_AL_LOGERR( "Vulkan: vkAllocateDescriptorSets failed: %s", VkResultToString( result ) );
				return VK_NULL_HANDLE;
			}
			++frame.descriptorPool;
			continue;
		}
		const uint32_t perType = 4096;
		const VkDescriptorPoolSize sizes[] = {
			{ VK_DESCRIPTOR_TYPE_SAMPLER, perType },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, perType },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, perType },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, perType },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, perType },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, perType },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, perType },
		};
		VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		poolInfo.maxSets = 1024;
		poolInfo.poolSizeCount = uint32_t( sizeof( sizes ) / sizeof( sizes[0] ) );
		poolInfo.pPoolSizes = sizes;
		VkDescriptorPool pool = VK_NULL_HANDLE;
		VkResult result = vkCreateDescriptorPool( m_device, &poolInfo, nullptr, &pool );
		if( result != VK_SUCCESS )
		{
			CCP_AL_LOGERR( "Vulkan: vkCreateDescriptorPool failed: %s", VkResultToString( result ) );
			return VK_NULL_HANDLE;
		}
		frame.descriptorPools.push_back( pool );
		frame.descriptorPool = frame.descriptorPools.size() - 1;
	}
}

void VulkanDevice::RecordFullBarrier()
{
	EndRendering();
	VkMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
	barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
	VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	dependency.memoryBarrierCount = 1;
	dependency.pMemoryBarriers = &barrier;
	vkCmdPipelineBarrier2( GetCommandBuffer(), &dependency );
}

void VulkanDevice::SynchronizeForCpuAccess()
{
	if( m_recording )
	{
		EndRendering();
		// Make device writes available to the host before the fence signals.
		VkMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
		barrier.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT | VK_ACCESS_2_HOST_WRITE_BIT;
		VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
		dependency.memoryBarrierCount = 1;
		dependency.pMemoryBarriers = &barrier;
		vkCmdPipelineBarrier2( m_frames[m_frameIndex].commandBuffer, &dependency );
		Submit();
	}
	for( uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i )
	{
		WaitForFrame( i );
	}
}

void VulkanDevice::SetObjectName( VkObjectType type, uint64_t handle, const char* name )
{
	if( !name || !VulkanInstance::Get()->ValidationEnabled() )
	{
		return;
	}
	VkDebugUtilsObjectNameInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
	info.objectType = type;
	info.objectHandle = handle;
	info.pObjectName = name;
	vkSetDebugUtilsObjectNameEXT( m_device, &info );
}

} // namespace TrinityALImpl

#endif

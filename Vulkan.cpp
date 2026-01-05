#include "Vulkan.h"
#include <stdio.h>
#include <string.h>
#pragma comment(lib,"vulkan-1.lib")

static VkInstance sVulkanInstance;
static const char* sEnabledExtensions[] = {
	VK_KHR_SURFACE_EXTENSION_NAME,
	VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
	VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME
};
static char** sPreferedEnabledLayers = nullptr;
static int sPreferedEnabledLayersCount = 0;

PFN_vkCreateDebugReportCallbackEXT __vkCreateDebugReportCallback = nullptr;
PFN_vkDestroyDebugReportCallbackEXT __vkDestroyDebugReportCallback = nullptr;
PFN_vkCreateWin32SurfaceKHR __vkCreateWin32Surface = nullptr;
VkDebugReportCallbackEXT sVulkanDebugReportCallback = nullptr;
PFN_vkCmdBeginDebugUtilsLabelEXT __vkCmdBeginDebugUtilsLabelEXT = nullptr;
PFN_vkCmdEndDebugUtilsLabelEXT __vkCmdEndDebugUtilsLabelEXT = nullptr;
PFN_vkSetDebugUtilsObjectNameEXT __vkSetDebugUtilsObjectNameEXT = nullptr;
static VkSurfaceKHR sVulkanSurface = nullptr;
static VkPhysicalDevice sGPU = nullptr;
static uint32_t sGraphicQueueFamilyIndex = 0, sPresentQueueFamilyIndex = 0;
static VkDevice sDevice = nullptr;
static VkQueue sGraphicQueue = nullptr, sPresentQueue = nullptr;
static VkSurfaceCapabilitiesKHR sSurfaceCapabilitiesKHR = {};
static VkSurfaceFormatKHR* sVulkanSurfaceFormats = nullptr;
static uint32_t sVulkanSurfaceFormatsCount = 0;
static uint32_t sPresentModeCount = 0;
static VkPresentModeKHR* sVulkanSurfacePresentMode = nullptr;
static VkSwapchainKHR sSwapChain = nullptr;
static VkImage* sSwapChainColorBuffer = nullptr;
static uint32_t sSwapChainColorBufferCount = 0;
static VkImageView* sSwapChainColorBufferView = nullptr;
static Texture* sSwapChainDSBuffer = nullptr;
static VkRenderPass sSwapChainRenderPass = nullptr;
static VkFramebuffer sSwapChainFBOs[2];
static VkCommandPool sCommandPool = nullptr;//command pool -> command buffer
static VkSemaphore sReadyToBeRendered, sReadyToShow;
static uint32_t sCurrentFBOToRenderIndex = 0;

static ShaderParameterDescription sUberShaderParameterDescription;
Buffer::Buffer() {
	mBuffer = nullptr;
	mMemory = nullptr;
}
Buffer::~Buffer() {
	//
}
static void InitUberPassPipelineLayout() {
	VkDescriptorSetLayoutBinding descriptorSetLayoutBinding[4];
	descriptorSetLayoutBinding[0].binding = 0;//b0
	descriptorSetLayoutBinding[0].descriptorCount = 1;//ubo -> descriptor
	descriptorSetLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorSetLayoutBinding[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT;

	descriptorSetLayoutBinding[1].binding = 1;//b1
	descriptorSetLayoutBinding[1].descriptorCount = 1;//ubo -> descriptor
	descriptorSetLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorSetLayoutBinding[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	descriptorSetLayoutBinding[2].binding = 2;//t0
	descriptorSetLayoutBinding[2].descriptorCount = 1;//ubo -> descriptor
	descriptorSetLayoutBinding[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorSetLayoutBinding[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	descriptorSetLayoutBinding[2].pImmutableSamplers = nullptr;

	descriptorSetLayoutBinding[3].binding = 3;//t0
	descriptorSetLayoutBinding[3].descriptorCount = 1;//texturecube -> descriptor
	descriptorSetLayoutBinding[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorSetLayoutBinding[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	descriptorSetLayoutBinding[3].pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.bindingCount = _countof(descriptorSetLayoutBinding);
	descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBinding;
	vkCreateDescriptorSetLayout(sDevice, &descriptorSetLayoutCreateInfo, nullptr, &sUberShaderParameterDescription.mDescriptorSetLayout);

	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(float) * 16 * 2;
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &sUberShaderParameterDescription.mDescriptorSetLayout;
	vkCreatePipelineLayout(sDevice, &pipelineLayoutCreateInfo, nullptr, &sUberShaderParameterDescription.mPipelineLayout);
}
Buffer* GenBufferObject(VkBufferUsageFlags inBufferUsageFlag,
	VkMemoryPropertyFlagBits inMemoryPropertyFlagBits, const void* inData, int inLen) {
	Buffer* buffer = new Buffer;
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = inLen;
	bufferCreateInfo.usage = inBufferUsageFlag;
	if (VK_SUCCESS != vkCreateBuffer(sDevice, &bufferCreateInfo, nullptr, &buffer->mBuffer)) {
		printf("create vbo failed\n");
	}
	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(sDevice, buffer->mBuffer, &memoryRequirements);
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memoryRequirements.size;
	VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
	vkGetPhysicalDeviceMemoryProperties(GetPhysicalDevice(), &physicalDeviceMemoryProperties);
	for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++) {
		if ((memoryRequirements.memoryTypeBits & (1 << i)) &&
			(physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags
				& inMemoryPropertyFlagBits)) {
			allocInfo.memoryTypeIndex = i;
			break;
		}
	}

	vkAllocateMemory(sDevice, &allocInfo, nullptr, &buffer->mMemory);
	vkBindBufferMemory(sDevice, buffer->mBuffer, buffer->mMemory, 0);
	if (inMemoryPropertyFlagBits == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
		if (inData != nullptr) {
			void* memPtr = nullptr;
			vkMapMemory(sDevice, buffer->mMemory, 0, inLen, 0, &memPtr);
			memcpy(memPtr, inData, inLen);
			vkUnmapMemory(sDevice, buffer->mMemory);
		}
	}
	else {
		//...
	}
	buffer->mSize = inLen;
	return buffer;
}
void BufferSubData(Buffer* buffer, const  void* data, VkDeviceSize size) {
	void* dst;
	vkMapMemory(sDevice, buffer->mMemory, 0, size, 0, &dst);
	memcpy(dst, data, size_t(size));
	vkUnmapMemory(sDevice, buffer->mMemory);
}
static void InitVulkanLayers() {
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	VkLayerProperties* layerProperties = new VkLayerProperties[layerCount];
	sPreferedEnabledLayers = new char* [layerCount];
	uint32_t preferedLayerIndex = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, layerProperties);
	for (uint32_t i = 0; i < layerCount; i++) {
		//printf("detect layer : [%s]\n", layerProperties[i].layerName);
		if (nullptr != strstr(layerProperties[i].layerName, "validation")) {
			sPreferedEnabledLayers[preferedLayerIndex] = new char[64];
			memset(sPreferedEnabledLayers[preferedLayerIndex], 0, 64);
			strcpy_s(sPreferedEnabledLayers[preferedLayerIndex++], 64,layerProperties[i].layerName);
		}
	}
	sPreferedEnabledLayersCount = preferedLayerIndex;
}
static bool InitVulkanInstance() {
	VkApplicationInfo vkApplicationInfo = {};
	vkApplicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	vkApplicationInfo.pApplicationName = "Unity3D";
	vkApplicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	vkApplicationInfo.pEngineName = "UnrealEngine";
	vkApplicationInfo.engineVersion = VK_MAKE_VERSION(5, 5, 4);
	vkApplicationInfo.apiVersion = VK_API_VERSION_1_2;
	VkInstanceCreateInfo vkInstanceCreateInfo = {
	};
	vkInstanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	vkInstanceCreateInfo.pApplicationInfo = &vkApplicationInfo;
	vkInstanceCreateInfo.enabledExtensionCount = _countof(sEnabledExtensions);
	vkInstanceCreateInfo.ppEnabledExtensionNames = sEnabledExtensions;
	InitVulkanLayers();
#ifdef _DEBUG
	vkInstanceCreateInfo.enabledLayerCount = sPreferedEnabledLayersCount;
	vkInstanceCreateInfo.ppEnabledLayerNames = sPreferedEnabledLayers;
#endif
	if (VK_SUCCESS != vkCreateInstance(&vkInstanceCreateInfo, nullptr, &sVulkanInstance)) {
		printf("InitVulkanInstance failed");
		return false;
	}
	return true;
}
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugReportFlagsEXT                       flags,
	VkDebugReportObjectTypeEXT                  objectType,
	uint64_t                                    object,
	size_t                                      location,
	int32_t                                     messageCode,
	const char* pLayerPrefix,
	const char* pMessage,
	void* pUserData) {
	printf("debug report : %s\n", pMessage);
	return VK_FALSE;
}
static bool InitDebugger() {
	VkDebugReportCallbackCreateInfoEXT debugReportCallbackCreateInfo = {};
	debugReportCallbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	debugReportCallbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
	debugReportCallbackCreateInfo.pfnCallback = debugCallback;
	if (VK_SUCCESS != __vkCreateDebugReportCallback(sVulkanInstance,
		&debugReportCallbackCreateInfo, nullptr,
		&sVulkanDebugReportCallback)) {
		return false;
	}
	return true;
}
static bool InitSurface(InitVulkanUserData* inUserData) {
	VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo = {};
	win32SurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	win32SurfaceCreateInfo.hinstance = inUserData->mApplicationInstance;
	win32SurfaceCreateInfo.hwnd = inUserData->mHWND;
	if (VK_SUCCESS != __vkCreateWin32Surface(sVulkanInstance, &win32SurfaceCreateInfo,
		nullptr, &sVulkanSurface)) {
		return false;
	}
	return true;
}
static bool InitVulkanPhysicalDevice() {
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(sVulkanInstance, &deviceCount, nullptr);
	VkPhysicalDevice* allVulkanGPU = new VkPhysicalDevice[deviceCount];
	vkEnumeratePhysicalDevices(sVulkanInstance, &deviceCount, allVulkanGPU);
	int graphicQueueFamilyIndex = -1;
	int presentQueueFamilyIndex = -1;
	for (uint32_t i = 0; i < deviceCount; i++) {
		VkPhysicalDevice device = allVulkanGPU[i];//
		VkPhysicalDeviceProperties deviceProps{};
		vkGetPhysicalDeviceProperties(device, &deviceProps);
		bool isDiscrete = (deviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
		uint32_t queueFamilyPropertyCount = 0;//
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyPropertyCount, nullptr);
		VkQueueFamilyProperties* queueFamilyProperties = new VkQueueFamilyProperties[queueFamilyPropertyCount];
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyPropertyCount, queueFamilyProperties);
		for (uint32_t j = 0; j < queueFamilyPropertyCount; j++) {
			if (queueFamilyProperties[j].queueCount > 0 &&
				queueFamilyProperties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				graphicQueueFamilyIndex = j;
			}
			VkBool32 supportPresent = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, j, sVulkanSurface, &supportPresent);
			if (queueFamilyProperties[j].queueCount > 0 && supportPresent) {
				presentQueueFamilyIndex = j;
			}
			if (graphicQueueFamilyIndex != -1 &&
				presentQueueFamilyIndex != -1) {
				sGPU = device;
				sGraphicQueueFamilyIndex = uint32_t(graphicQueueFamilyIndex);
				sPresentQueueFamilyIndex = uint32_t(presentQueueFamilyIndex);
				return true;
			}
		}
	}
	return false;
}
static bool InitVulkanLogicDevice() {
	VkDeviceQueueCreateInfo queueCreateInfos[2];
	float p = 1.0f;
	int queueCreateInfoCount = 2;
	if (sGraphicQueueFamilyIndex == sPresentQueueFamilyIndex) {
		queueCreateInfos[0] = {};
		queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfos[0].queueFamilyIndex = sGraphicQueueFamilyIndex;
		queueCreateInfos[0].queueCount = 1;//
		queueCreateInfos[0].pQueuePriorities = &p;//20 -> 0.0~1.0 foreground background
		queueCreateInfoCount = 1;
	}
	else {
		queueCreateInfos[0] = {};
		queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfos[0].queueFamilyIndex = sGraphicQueueFamilyIndex;
		queueCreateInfos[0].queueCount = 1;//
		queueCreateInfos[0].pQueuePriorities = &p;//20 -> 0.0~1.0 foreground background
		queueCreateInfos[1] = {};
		queueCreateInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfos[1].queueFamilyIndex = sPresentQueueFamilyIndex;
		queueCreateInfos[1].queueCount = 1;//
		queueCreateInfos[1].pQueuePriorities = &p;//20 -> 0.0~1.0 foreground background
		queueCreateInfoCount = 2;
	}
	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = queueCreateInfoCount;
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos;

#ifdef _DEBUG
	deviceCreateInfo.enabledLayerCount = sPreferedEnabledLayersCount;
	deviceCreateInfo.ppEnabledLayerNames = sPreferedEnabledLayers;
#endif

	deviceCreateInfo.enabledExtensionCount = 1;//core 
	const char* extensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};
	deviceCreateInfo.ppEnabledExtensionNames = extensions;
	VkPhysicalDeviceFeatures2 features2 = {};
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	VkPhysicalDeviceShaderAtomicInt64Features shaderAtomic64Features = {};
	shaderAtomic64Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES;
	features2.pNext = &shaderAtomic64Features;
	vkGetPhysicalDeviceFeatures2(sGPU, &features2);
	bool supportShaderInt64 = features2.features.shaderInt64;
	bool supportBufferInt64Atomics = shaderAtomic64Features.shaderBufferInt64Atomics;

	//VkPhysicalDeviceShaderAtomicInt64Features shaderAtomic64Features = {};
	shaderAtomic64Features = {};
	shaderAtomic64Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES;
	shaderAtomic64Features.shaderBufferInt64Atomics = VK_TRUE;
	features2 = {};
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.features.shaderInt64 = VK_TRUE;
	features2.features.geometryShader = VK_TRUE;
	features2.features.tessellationShader = VK_TRUE;
	features2.features.fillModeNonSolid = VK_TRUE;
	features2.features.wideLines = VK_TRUE;
	features2.features.shaderInt64 = VK_TRUE;
	features2.features.fragmentStoresAndAtomics = VK_TRUE;
	features2.pNext = &shaderAtomic64Features;

	VkPhysicalDeviceFeatures physicalDeviceFeatures = {};

	deviceCreateInfo.pEnabledFeatures = nullptr;
	deviceCreateInfo.pNext = &features2;
	if (VK_SUCCESS != vkCreateDevice(sGPU, &deviceCreateInfo, nullptr, &sDevice)) {
		return false;
	}
	vkGetDeviceQueue(sDevice, sGraphicQueueFamilyIndex, 0, &sGraphicQueue);
	vkGetDeviceQueue(sDevice, sPresentQueueFamilyIndex, 0, &sPresentQueue);
	return true;
}
void InitSurfaceProperties() {
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(sGPU, sVulkanSurface, &sSurfaceCapabilitiesKHR);
	vkGetPhysicalDeviceSurfaceFormatsKHR(sGPU, sVulkanSurface, &sVulkanSurfaceFormatsCount, nullptr);
	sVulkanSurfaceFormats = new VkSurfaceFormatKHR[sVulkanSurfaceFormatsCount];
	vkGetPhysicalDeviceSurfaceFormatsKHR(sGPU, sVulkanSurface, &sVulkanSurfaceFormatsCount, sVulkanSurfaceFormats);
	vkGetPhysicalDeviceSurfacePresentModesKHR(sGPU, sVulkanSurface, &sPresentModeCount, nullptr);
	sVulkanSurfacePresentMode = new VkPresentModeKHR[sPresentModeCount];
	vkGetPhysicalDeviceSurfacePresentModesKHR(sGPU, sVulkanSurface, &sPresentModeCount, sVulkanSurfacePresentMode);
}
void InitSwapchain() {
	VkSurfaceFormatKHR selectedSurfaceFormat;
	for (uint32_t i = 0; i < sVulkanSurfaceFormatsCount; i++) {
		if (sVulkanSurfaceFormats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
			sVulkanSurfaceFormats[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
			selectedSurfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
			selectedSurfaceFormat.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
			break;
		}
	}
	VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChainCreateInfo.imageArrayLayers = 1;//texture cube,Array,สýื้
	swapChainCreateInfo.imageColorSpace = selectedSurfaceFormat.colorSpace;
	swapChainCreateInfo.imageFormat = selectedSurfaceFormat.format;
	swapChainCreateInfo.imageExtent.width = 1280u;
	swapChainCreateInfo.imageExtent.height = 720u;
	swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;//queue
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapChainCreateInfo.minImageCount = 2;// sSurfaceCapabilitiesKHR.minImageCount;//double
	uint32_t queueFamilyIndices[2] = { 0 };
	uint32_t queueFamilyIndexCount = 2;
	if (sGraphicQueueFamilyIndex == sPresentQueueFamilyIndex) {
		queueFamilyIndices[0] = sGraphicQueueFamilyIndex;
		queueFamilyIndexCount = 1;
	}
	else {
		queueFamilyIndices[0] = sGraphicQueueFamilyIndex;
		queueFamilyIndices[1] = sPresentQueueFamilyIndex;
		queueFamilyIndexCount = 2;
	}
	swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
	swapChainCreateInfo.queueFamilyIndexCount = queueFamilyIndexCount;
	swapChainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;//opengl
	swapChainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapChainCreateInfo.surface = sVulkanSurface;
	vkCreateSwapchainKHR(sDevice, &swapChainCreateInfo, nullptr, &sSwapChain);//color buffer
}
void GenImage(Texture* inOutTexture, int inWidth, int inHeight,
	VkImageUsageFlags inUsage, VkMemoryPropertyFlagBits inPreferedMemoryPropertyFlagBits) {
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.extent = { uint32_t(inWidth),uint32_t(inHeight),1 };
	imageCreateInfo.format = inOutTexture->mFormat;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = inUsage;
	vkCreateImage(sDevice, &imageCreateInfo, nullptr, &inOutTexture->mImage);//logic object

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(sDevice, inOutTexture->mImage, &memoryRequirements);
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memoryRequirements.size;
	VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
	vkGetPhysicalDeviceMemoryProperties(sGPU, &physicalDeviceMemoryProperties);
	for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++) {
		if ((memoryRequirements.memoryTypeBits & (1 << i)) &&
			(physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags
				& inPreferedMemoryPropertyFlagBits)) {
			allocInfo.memoryTypeIndex = i;
			break;
		}
	}
	vkAllocateMemory(sDevice, &allocInfo, nullptr, &inOutTexture->mMemory);
	vkBindImageMemory(sDevice, inOutTexture->mImage, inOutTexture->mMemory, 0);
}
VkImageView GenImageView2D(VkImage inImage, VkFormat inFormat, VkImageAspectFlags inImageAspectFlag) {
	VkImageView imageView;
	VkImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.format = inFormat;
	imageViewCreateInfo.image = inImage;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.subresourceRange.aspectMask = inImageAspectFlag;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	if (VK_SUCCESS != vkCreateImageView(sDevice, &imageViewCreateInfo, nullptr, &imageView)) {
		return nullptr;
	}
	return imageView;
}
void GenImageCubeMap(Texture* inOutTexture, int inWidth, int inHeight,
	VkImageUsageFlags inUsage, VkMemoryPropertyFlagBits inPreferedMemoryPropertyFlagBits) {
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.arrayLayers = 6;//texture2DArray
	imageCreateInfo.extent = { uint32_t(inWidth),uint32_t(inHeight),1 };
	imageCreateInfo.format = inOutTexture->mFormat;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.mipLevels = 1;//mipmap chain
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = inUsage;
	imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	vkCreateImage(sDevice, &imageCreateInfo, nullptr, &inOutTexture->mImage);//logic object

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(sDevice, inOutTexture->mImage, &memoryRequirements);
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memoryRequirements.size;
	VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
	vkGetPhysicalDeviceMemoryProperties(sGPU, &physicalDeviceMemoryProperties);
	for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++) {
		if ((memoryRequirements.memoryTypeBits & (1 << i)) &&
			(physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags
				& inPreferedMemoryPropertyFlagBits)) {
			allocInfo.memoryTypeIndex = i;
			break;
		}
	}
	vkAllocateMemory(sDevice, &allocInfo, nullptr, &inOutTexture->mMemory);
	vkBindImageMemory(sDevice, inOutTexture->mImage, inOutTexture->mMemory, 0);
}
VkImageView GenImageViewCubeMap(VkImage inImage, VkFormat inFormat, VkImageAspectFlags inImageAspectFlag) {
	VkImageView imageView;
	VkImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	imageViewCreateInfo.image = inImage;
	imageViewCreateInfo.format = inFormat;
	imageViewCreateInfo.subresourceRange.aspectMask = inImageAspectFlag;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 6;
	if (VK_SUCCESS != vkCreateImageView(sDevice, &imageViewCreateInfo, nullptr, &imageView)) {
		return nullptr;
	}
	return imageView;
}
void InitSwapChainRenderTarget() {
	vkGetSwapchainImagesKHR(sDevice, sSwapChain, &sSwapChainColorBufferCount, nullptr);
	sSwapChainColorBuffer = new VkImage[sSwapChainColorBufferCount];
	vkGetSwapchainImagesKHR(sDevice, sSwapChain, &sSwapChainColorBufferCount, sSwapChainColorBuffer);
	sSwapChainColorBufferView = new VkImageView[sSwapChainColorBufferCount];
	for (uint32_t i = 0; i < sSwapChainColorBufferCount; i++) {
		sSwapChainColorBufferView[i] = GenImageView2D(sSwapChainColorBuffer[i],
			VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
	}
}
void InitSwapChainDSRT() {
	sSwapChainDSBuffer = new Texture;
	sSwapChainDSBuffer->mFormat = VK_FORMAT_D24_UNORM_S8_UINT;//D16 D32
	sSwapChainDSBuffer->mImageAspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	GenImage(sSwapChainDSBuffer, 1280, 720, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	sSwapChainDSBuffer->mImageView = GenImageView2D(sSwapChainDSBuffer->mImage,
		VK_FORMAT_D24_UNORM_S8_UINT, sSwapChainDSBuffer->mImageAspectFlag);
}
void InitSwapChainRenderPass() {//rt
	VkAttachmentDescription attachments[2];
	attachments[0] = {};
	attachments[0].format = VK_FORMAT_B8G8R8A8_UNORM;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;//   backgroundbuffer  [ ]
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;//L2 -> GMEM -> RAM MEMORY
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	attachments[1] = {};
	attachments[1].format = VK_FORMAT_D24_UNORM_S8_UINT;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;//   backgroundbuffer  [ ]
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;//L2 -> GMEM -> RAM MEMORY
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentReference = {};
	colorAttachmentReference.attachment = 0;//
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference dsAttachmentReference = {};
	dsAttachmentReference.attachment = 1;//
	dsAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentReference;
	subpass.pDepthStencilAttachment = &dsAttachmentReference;

	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 2;
	renderPassCreateInfo.pAttachments = attachments;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	vkCreateRenderPass(sDevice, &renderPassCreateInfo, nullptr, &sSwapChainRenderPass);
}
static void InitSwapChainFBO() {
	//color attachment
	for (int i = 0; i < 2; i++) {
		VkImageView attachments[2];
		attachments[0] = sSwapChainColorBufferView[i];
		attachments[1] = sSwapChainDSBuffer->mImageView;
		VkFramebufferCreateInfo frameBufferCreateInfo = {};
		frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferCreateInfo.renderPass = sSwapChainRenderPass;
		frameBufferCreateInfo.attachmentCount = 2;
		frameBufferCreateInfo.pAttachments = attachments;
		frameBufferCreateInfo.width = 1280u;
		frameBufferCreateInfo.height = 720u;
		frameBufferCreateInfo.layers = 1;

		vkCreateFramebuffer(sDevice, &frameBufferCreateInfo, nullptr, &sSwapChainFBOs[i]);
	}
}
void InitCommandPool() {
	VkCommandPoolCreateInfo commandPoolCreateInfo = {};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.queueFamilyIndex = sGraphicQueueFamilyIndex;
	vkCreateCommandPool(sDevice, &commandPoolCreateInfo, nullptr, &sCommandPool);
}
bool InitVulkan(void* inUserData, int inCanvasWidth, int inCanvasHeight) {
	//init vulkan instance
	if (false == InitVulkanInstance()) {
		return false;
	}
	__vkCreateDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(sVulkanInstance,
		"vkCreateDebugReportCallbackEXT");
	__vkDestroyDebugReportCallback = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(sVulkanInstance,
		"vkDestroyDebugReportCallbackEXT");
	__vkCreateWin32Surface = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(sVulkanInstance,
		"vkCreateWin32SurfaceKHR");
	__vkCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)
		vkGetInstanceProcAddr(sVulkanInstance, "vkCmdBeginDebugUtilsLabelEXT");
	__vkCmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)
		vkGetInstanceProcAddr(sVulkanInstance, "vkCmdEndDebugUtilsLabelEXT");
	__vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)
		vkGetInstanceProcAddr(sVulkanInstance, "vkSetDebugUtilsObjectNameEXT");

	if (false == InitDebugger()) {
		return false;
	}
	if (false == InitSurface((InitVulkanUserData*)inUserData)) {
		return false;
	}
	if (false == InitVulkanPhysicalDevice()) {
		return false;
	}
	if (false == InitVulkanLogicDevice()) {
		return false;
	}
	InitSurfaceProperties();
	InitSwapchain();
	InitSwapChainRenderTarget();
	InitSwapChainDSRT();
	InitSwapChainRenderPass();
	InitSwapChainFBO();
	InitCommandPool();
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	vkCreateSemaphore(sDevice, &semaphoreCreateInfo, nullptr, &sReadyToBeRendered);
	vkCreateSemaphore(sDevice, &semaphoreCreateInfo, nullptr, &sReadyToShow);
	InitUberPassPipelineLayout();
	return true;
}
VkCommandBuffer CreateCommandBuffer(VkCommandBufferLevel inCommandBufferLevel) {
	VkCommandBuffer commandBuffer = nullptr;
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.commandPool = sCommandPool;
	commandBufferAllocateInfo.commandBufferCount = 1;
	commandBufferAllocateInfo.level = inCommandBufferLevel;
	vkAllocateCommandBuffers(sDevice, &commandBufferAllocateInfo, &commandBuffer);
	return commandBuffer;
}
void BeginCommandBuffer(VkCommandBuffer inCommandBuffer, VkCommandBufferUsageFlagBits inUsage) {
	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.flags = inUsage;//
	vkBeginCommandBuffer(inCommandBuffer, &commandBufferBeginInfo);
}
uint32_t BeginSwapChainRenderPass(VkCommandBuffer inCommandBuffer) {
	vkAcquireNextImageKHR(sDevice, sSwapChain, 1000000, sReadyToBeRendered, nullptr, &sCurrentFBOToRenderIndex);
	//begin render pass -> rt
	VkClearValue clearValues[2];
	clearValues[0].color = { 0.1f,0.4f,0.6f,1.0f };
	clearValues[1].depthStencil = { 1.0f,0u };
	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.framebuffer = sSwapChainFBOs[sCurrentFBOToRenderIndex];
	renderPassBeginInfo.pClearValues = clearValues;
	renderPassBeginInfo.renderArea.offset = { 0,0 };
	renderPassBeginInfo.renderArea.extent = { 1280u,720u };
	renderPassBeginInfo.renderPass = sSwapChainRenderPass;

	vkCmdBeginRenderPass(inCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	return sCurrentFBOToRenderIndex;
}
void EndSwapChainRenderPass(VkCommandBuffer inCommandBuffer) {
	vkCmdEndRenderPass(inCommandBuffer);
	vkEndCommandBuffer(inCommandBuffer);
	{
		//submit
		VkPipelineStageFlags waitStage[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &inCommandBuffer;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &sReadyToBeRendered;
		submitInfo.pWaitDstStageMask = waitStage;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &sReadyToShow;
		vkQueueSubmit(sGraphicQueue, 1, &submitInfo, nullptr);//cpu -> gpu
	}
	{
		//present,swapbuffers
		VkPresentInfoKHR presentInfoKHR = {};
		presentInfoKHR.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfoKHR.waitSemaphoreCount = 1;
		presentInfoKHR.pWaitSemaphores = &sReadyToShow;
		presentInfoKHR.pSwapchains = &sSwapChain;
		presentInfoKHR.swapchainCount = 1;
		presentInfoKHR.pImageIndices = &sCurrentFBOToRenderIndex;
		vkQueuePresentKHR(sPresentQueue, &presentInfoKHR);
		vkQueueWaitIdle(sPresentQueue);
	}
	vkFreeCommandBuffers(sDevice, sCommandPool, 1, &inCommandBuffer);
}
VkQueue GetGraphicQueue() {
	return sGraphicQueue;
}
VkDevice GetVulkanDevice() {
	return sDevice;
}
VkPhysicalDevice GetPhysicalDevice() {
	return sGPU;
}
VkRenderPass GetSwapChainRenderPass() {
	return sSwapChainRenderPass;
}
ShaderParameterDescription* GetUberPassShaderParameterDescription() {
	return &sUberShaderParameterDescription;
}
VkPipeline CreatePSO(VkRenderPass inRenderPass, VkPrimitiveTopology inPrimitiveType,
	const std::vector<VkVertexInputBindingDescription>& inVertexInputBindingDescriptions,
	const std::vector<VkVertexInputAttributeDescription>& inVertexInputAttributeDescriptions,
	const VkShaderModule inVS, const VkShaderModule inFS) {
	VkPipelineVertexInputStateCreateInfo pipelinVertexInputStateCreateInfo = {};
	//pos,texcoord,normal,tangent|pos,texcoord,normal,tangent|pos,texcoord,normal,tangent|..
	pipelinVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	pipelinVertexInputStateCreateInfo.vertexBindingDescriptionCount = uint32_t(inVertexInputBindingDescriptions.size());
	pipelinVertexInputStateCreateInfo.vertexAttributeDescriptionCount = uint32_t(inVertexInputAttributeDescriptions.size());
	pipelinVertexInputStateCreateInfo.pVertexBindingDescriptions = inVertexInputBindingDescriptions.data();
	pipelinVertexInputStateCreateInfo.pVertexAttributeDescriptions = inVertexInputAttributeDescriptions.data();
	//glViewport,glScissor
	VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo = {};
	pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	pipelineDynamicStateCreateInfo.dynamicStateCount = 0;
	pipelineDynamicStateCreateInfo.pDynamicStates = nullptr;//viewport,scissor
	VkViewport viewport = {};//ndc -> viewport : -1~1 -> 0.0 ~ canvas size
	viewport.x = 0.0f;
	viewport.y = 720.0f;
	viewport.width = 1280.0f;
	viewport.height = -720.0f;
	viewport.maxDepth = 1.0f;
	viewport.minDepth = 0.0f;//-1~1
	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = 1280u;
	scissor.extent.height = 720u;
	VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {};
	pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	pipelineViewportStateCreateInfo.viewportCount = 1;
	pipelineViewportStateCreateInfo.scissorCount = 1;
	pipelineViewportStateCreateInfo.pViewports = &viewport;
	pipelineViewportStateCreateInfo.pScissors = &scissor;

	VkPipelineInputAssemblyStateCreateInfo pipelineIAStateCreateInfo = {};
	pipelineIAStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	pipelineIAStateCreateInfo.topology = inPrimitiveType;

	VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {};
	pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	pipelineRasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;//stream out,transform feedback
	pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;
	pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	pipelineRasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;//
	pipelineRasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	pipelineRasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;

	VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo = {};
	pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	pipelineMultisampleStateCreateInfo.minSampleShading = 1.0f;

	VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo = {};
	pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	pipelineDepthStencilStateCreateInfo.depthTestEnable = true;
	pipelineDepthStencilStateCreateInfo.depthWriteEnable = true;
	pipelineDepthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	pipelineDepthStencilStateCreateInfo.depthBoundsTestEnable = false;
	pipelineDepthStencilStateCreateInfo.minDepthBounds = 0.0f;
	pipelineDepthStencilStateCreateInfo.maxDepthBounds = 1.0f;
	pipelineDepthStencilStateCreateInfo.stencilTestEnable = false;
	pipelineDepthStencilStateCreateInfo.front = {};
	pipelineDepthStencilStateCreateInfo.back = {};

	VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {};
	pipelineColorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	pipelineColorBlendAttachmentState.blendEnable = false;

	VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {};
	pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	pipelineColorBlendStateCreateInfo.attachmentCount = 1;
	pipelineColorBlendStateCreateInfo.blendConstants[0] = 0.0f;
	pipelineColorBlendStateCreateInfo.blendConstants[1] = 0.0f;
	pipelineColorBlendStateCreateInfo.blendConstants[2] = 0.0f;
	pipelineColorBlendStateCreateInfo.blendConstants[3] = 0.0f;
	pipelineColorBlendStateCreateInfo.logicOpEnable = false;
	pipelineColorBlendStateCreateInfo.pAttachments = &pipelineColorBlendAttachmentState;

	VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo[2];
	pipelineShaderStageCreateInfo[0] = {};
	pipelineShaderStageCreateInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineShaderStageCreateInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	pipelineShaderStageCreateInfo[0].module = inVS;
	pipelineShaderStageCreateInfo[0].pName = "main";
	pipelineShaderStageCreateInfo[1] = {};
	pipelineShaderStageCreateInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineShaderStageCreateInfo[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	pipelineShaderStageCreateInfo[1].module = inFS;
	pipelineShaderStageCreateInfo[1].pName = "main";
	// ubo -> write descriptor set -> descriptor set

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.renderPass = GetSwapChainRenderPass();
	graphicsPipelineCreateInfo.stageCount = 2;//vertex->tcs->tes->geometry->fragment->
	graphicsPipelineCreateInfo.basePipelineIndex = -1;
	graphicsPipelineCreateInfo.pVertexInputState = &pipelinVertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &pipelineDynamicStateCreateInfo;
	graphicsPipelineCreateInfo.pViewportState = &pipelineViewportStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &pipelineIAStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &pipelineMultisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &pipelineDepthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &pipelineColorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pStages = pipelineShaderStageCreateInfo;
	graphicsPipelineCreateInfo.layout = sUberShaderParameterDescription.mPipelineLayout;
	VkPipeline pipeline;
	vkCreateGraphicsPipelines(sDevice, nullptr, 1, &graphicsPipelineCreateInfo, nullptr, &pipeline);
	return pipeline;

}
VkPipeline CreatePSOVGF(
	const std::vector<VkVertexInputBindingDescription>& inVertexInputBindingDescriptions,
	const std::vector<VkVertexInputAttributeDescription>& inVertexInputAttributeDescriptions,
	const VkShaderModule inVS, const VkShaderModule inGS, const VkShaderModule inFS) {
	VkPipelineVertexInputStateCreateInfo pipelinVertexInputStateCreateInfo = {};
	//pos,texcoord,normal,tangent|pos,texcoord,normal,tangent|pos,texcoord,normal,tangent|..
	pipelinVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	pipelinVertexInputStateCreateInfo.vertexBindingDescriptionCount = uint32_t(inVertexInputBindingDescriptions.size());
	pipelinVertexInputStateCreateInfo.vertexAttributeDescriptionCount = uint32_t(inVertexInputAttributeDescriptions.size());
	pipelinVertexInputStateCreateInfo.pVertexBindingDescriptions = inVertexInputBindingDescriptions.data();
	pipelinVertexInputStateCreateInfo.pVertexAttributeDescriptions = inVertexInputAttributeDescriptions.data();
	//glViewport,glScissor
	VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo = {};
	pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	pipelineDynamicStateCreateInfo.dynamicStateCount = 0;
	pipelineDynamicStateCreateInfo.pDynamicStates = nullptr;//viewport,scissor
	VkViewport viewport = {};//ndc -> viewport : -1~1 -> 0.0 ~ canvas size
	viewport.x = 0.0f;
	viewport.y = 720.0f;
	viewport.width = 1280.0f;
	viewport.height = -720.0f;
	viewport.maxDepth = 1.0f;
	viewport.minDepth = 0.0f;//-1~1
	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = 1280u;
	scissor.extent.height = 720u;
	VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {};
	pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	pipelineViewportStateCreateInfo.viewportCount = 1;
	pipelineViewportStateCreateInfo.scissorCount = 1;
	pipelineViewportStateCreateInfo.pViewports = &viewport;
	pipelineViewportStateCreateInfo.pScissors = &scissor;

	VkPipelineInputAssemblyStateCreateInfo pipelineIAStateCreateInfo = {};
	pipelineIAStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	pipelineIAStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {};
	pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	pipelineRasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;//stream out,transform feedback
	pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;
	pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	pipelineRasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;//
	pipelineRasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	pipelineRasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;

	VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo = {};
	pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	pipelineMultisampleStateCreateInfo.minSampleShading = 1.0f;

	VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo = {};
	pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	pipelineDepthStencilStateCreateInfo.depthTestEnable = true;
	pipelineDepthStencilStateCreateInfo.depthWriteEnable = true;
	pipelineDepthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	pipelineDepthStencilStateCreateInfo.depthBoundsTestEnable = false;
	pipelineDepthStencilStateCreateInfo.minDepthBounds = 0.0f;
	pipelineDepthStencilStateCreateInfo.maxDepthBounds = 1.0f;
	pipelineDepthStencilStateCreateInfo.stencilTestEnable = false;
	pipelineDepthStencilStateCreateInfo.front = {};
	pipelineDepthStencilStateCreateInfo.back = {};

	VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {};
	pipelineColorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	pipelineColorBlendAttachmentState.blendEnable = false;

	VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {};
	pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	pipelineColorBlendStateCreateInfo.attachmentCount = 1;
	pipelineColorBlendStateCreateInfo.blendConstants[0] = 0.0f;
	pipelineColorBlendStateCreateInfo.blendConstants[1] = 0.0f;
	pipelineColorBlendStateCreateInfo.blendConstants[2] = 0.0f;
	pipelineColorBlendStateCreateInfo.blendConstants[3] = 0.0f;
	pipelineColorBlendStateCreateInfo.logicOpEnable = false;
	pipelineColorBlendStateCreateInfo.pAttachments = &pipelineColorBlendAttachmentState;

	VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo[3];
	pipelineShaderStageCreateInfo[0] = {};
	pipelineShaderStageCreateInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineShaderStageCreateInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	pipelineShaderStageCreateInfo[0].module = inVS;
	pipelineShaderStageCreateInfo[0].pName = "main";
	pipelineShaderStageCreateInfo[1] = {};
	pipelineShaderStageCreateInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineShaderStageCreateInfo[1].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
	pipelineShaderStageCreateInfo[1].module = inGS;
	pipelineShaderStageCreateInfo[1].pName = "main";
	pipelineShaderStageCreateInfo[2] = {};
	pipelineShaderStageCreateInfo[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineShaderStageCreateInfo[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	pipelineShaderStageCreateInfo[2].module = inFS;
	pipelineShaderStageCreateInfo[2].pName = "main";
	// ubo -> write descriptor set -> descriptor set

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.renderPass = GetSwapChainRenderPass();
	graphicsPipelineCreateInfo.stageCount = _countof(pipelineShaderStageCreateInfo);//vertex->tcs->tes->geometry->fragment->
	graphicsPipelineCreateInfo.basePipelineIndex = -1;
	graphicsPipelineCreateInfo.pVertexInputState = &pipelinVertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &pipelineDynamicStateCreateInfo;
	graphicsPipelineCreateInfo.pViewportState = &pipelineViewportStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &pipelineIAStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &pipelineMultisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &pipelineDepthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &pipelineColorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pStages = pipelineShaderStageCreateInfo;
	graphicsPipelineCreateInfo.layout = sUberShaderParameterDescription.mPipelineLayout;
	VkPipeline pipeline;
	vkCreateGraphicsPipelines(sDevice, nullptr, 1, &graphicsPipelineCreateInfo, nullptr, &pipeline);
	return pipeline;
}
VkPipeline CreatePSOVTF(VkRenderPass inRenderPass,
	const std::vector<VkVertexInputBindingDescription>& inVertexInputBindingDescriptions,
	const std::vector<VkVertexInputAttributeDescription>& inVertexInputAttributeDescriptions,
	const VkShaderModule inVS, const VkShaderModule inTCS, const VkShaderModule inTES, const VkShaderModule inFS) {
	VkPipelineVertexInputStateCreateInfo pipelinVertexInputStateCreateInfo = {};
	//pos,texcoord,normal,tangent|pos,texcoord,normal,tangent|pos,texcoord,normal,tangent|..
	pipelinVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	pipelinVertexInputStateCreateInfo.vertexBindingDescriptionCount = uint32_t(inVertexInputBindingDescriptions.size());
	pipelinVertexInputStateCreateInfo.vertexAttributeDescriptionCount = uint32_t(inVertexInputAttributeDescriptions.size());
	pipelinVertexInputStateCreateInfo.pVertexBindingDescriptions = inVertexInputBindingDescriptions.data();
	pipelinVertexInputStateCreateInfo.pVertexAttributeDescriptions = inVertexInputAttributeDescriptions.data();
	//glViewport,glScissor
	VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo = {};
	pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	pipelineDynamicStateCreateInfo.dynamicStateCount = 0;
	pipelineDynamicStateCreateInfo.pDynamicStates = nullptr;//viewport,scissor
	VkViewport viewport = {};//ndc -> viewport : -1~1 -> 0.0 ~ canvas size
	viewport.x = 0.0f;
	//viewport.y = 720.0f;
	//viewport.width = 1280.0f;
	//viewport.height = -720.0f;
	viewport.y = 0.0f;
	viewport.width = 1280.0f;
	viewport.height = 720.0f;
	viewport.maxDepth = 1.0f;
	viewport.minDepth = 0.0f;//-1~1
	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = 1280u;
	scissor.extent.height = 720u;
	VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {};
	pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	pipelineViewportStateCreateInfo.viewportCount = 1;
	pipelineViewportStateCreateInfo.scissorCount = 1;
	pipelineViewportStateCreateInfo.pViewports = &viewport;
	pipelineViewportStateCreateInfo.pScissors = &scissor;

	VkPipelineInputAssemblyStateCreateInfo pipelineIAStateCreateInfo = {};
	pipelineIAStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	pipelineIAStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;

	VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {};
	pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	pipelineRasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;//stream out,transform feedback
	pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_LINE;
	pipelineRasterizationStateCreateInfo.lineWidth = 2.0f;
	pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	pipelineRasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;//
	pipelineRasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	pipelineRasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;

	VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo = {};
	pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	pipelineMultisampleStateCreateInfo.minSampleShading = 1.0f;

	VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo = {};
	pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	pipelineDepthStencilStateCreateInfo.depthTestEnable = true;
	pipelineDepthStencilStateCreateInfo.depthWriteEnable = true;
	pipelineDepthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	pipelineDepthStencilStateCreateInfo.depthBoundsTestEnable = false;
	pipelineDepthStencilStateCreateInfo.minDepthBounds = 0.0f;
	pipelineDepthStencilStateCreateInfo.maxDepthBounds = 1.0f;
	pipelineDepthStencilStateCreateInfo.stencilTestEnable = false;
	pipelineDepthStencilStateCreateInfo.front = {};
	pipelineDepthStencilStateCreateInfo.back = {};

	VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {};
	pipelineColorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	pipelineColorBlendAttachmentState.blendEnable = false;

	VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {};
	pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	pipelineColorBlendStateCreateInfo.attachmentCount = 1;
	pipelineColorBlendStateCreateInfo.blendConstants[0] = 0.0f;
	pipelineColorBlendStateCreateInfo.blendConstants[1] = 0.0f;
	pipelineColorBlendStateCreateInfo.blendConstants[2] = 0.0f;
	pipelineColorBlendStateCreateInfo.blendConstants[3] = 0.0f;
	pipelineColorBlendStateCreateInfo.logicOpEnable = false;
	pipelineColorBlendStateCreateInfo.pAttachments = &pipelineColorBlendAttachmentState;

	VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo[4];
	pipelineShaderStageCreateInfo[0] = {};
	pipelineShaderStageCreateInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineShaderStageCreateInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	pipelineShaderStageCreateInfo[0].module = inVS;
	pipelineShaderStageCreateInfo[0].pName = "main";

	pipelineShaderStageCreateInfo[1] = {};
	pipelineShaderStageCreateInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineShaderStageCreateInfo[1].stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
	pipelineShaderStageCreateInfo[1].module = inTCS;
	pipelineShaderStageCreateInfo[1].pName = "main";

	pipelineShaderStageCreateInfo[2] = {};
	pipelineShaderStageCreateInfo[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineShaderStageCreateInfo[2].stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
	pipelineShaderStageCreateInfo[2].module = inTES;
	pipelineShaderStageCreateInfo[2].pName = "main";

	pipelineShaderStageCreateInfo[3] = {};
	pipelineShaderStageCreateInfo[3].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineShaderStageCreateInfo[3].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	pipelineShaderStageCreateInfo[3].module = inFS;
	pipelineShaderStageCreateInfo[3].pName = "main";
	// ubo -> write descriptor set -> descriptor set

	VkPipelineTessellationStateCreateInfo pipelineTessellationStateCreateInfo = {};
	pipelineTessellationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
	pipelineTessellationStateCreateInfo.patchControlPoints = 4;

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.renderPass = inRenderPass;
	graphicsPipelineCreateInfo.stageCount = _countof(pipelineShaderStageCreateInfo);//vertex->tcs->tes->geometry->fragment->
	graphicsPipelineCreateInfo.basePipelineIndex = -1;
	graphicsPipelineCreateInfo.pVertexInputState = &pipelinVertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &pipelineDynamicStateCreateInfo;
	graphicsPipelineCreateInfo.pViewportState = &pipelineViewportStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &pipelineIAStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &pipelineMultisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &pipelineDepthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &pipelineColorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pStages = pipelineShaderStageCreateInfo;
	graphicsPipelineCreateInfo.pTessellationState = &pipelineTessellationStateCreateInfo;
	graphicsPipelineCreateInfo.layout = sUberShaderParameterDescription.mPipelineLayout;
	VkPipeline pipeline;
	vkCreateGraphicsPipelines(sDevice, nullptr, 1, &graphicsPipelineCreateInfo, nullptr, &pipeline);
	return pipeline;
}
VkShaderModule CompileShader(const char* inFilePath) {
	FILE* pFile = nullptr;
	errno_t err = fopen_s(&pFile, inFilePath, "rb");
	if (err == 0) {
		fseek(pFile, 0, SEEK_END);
		long fileSize = ftell(pFile);
		rewind(pFile);
		unsigned char* fileContent = new unsigned char[fileSize];
		fread(fileContent, 1, fileSize, pFile);
		fclose(pFile);
		VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
		shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderModuleCreateInfo.pCode = (uint32_t*)fileContent;
		shaderModuleCreateInfo.codeSize = fileSize;
		VkShaderModule shader;
		if (VK_SUCCESS != vkCreateShaderModule(GetVulkanDevice(),
			&shaderModuleCreateInfo, nullptr, &shader)) {
			printf("Create Shader failed %s\n", inFilePath);
		}
		return shader;
	}
	return nullptr;
}
VkFramebuffer* GetSwapChainFrameBuffers() {
	return sSwapChainFBOs;
}
void TransferImageLayout(VkCommandBuffer inCommandBuffer, VkImage inImage,
	VkImageSubresourceRange inSubresourceRange,
	VkImageLayout inOldLayout, VkAccessFlags inOldAccessFlags, VkPipelineStageFlags inOld,
	VkImageLayout inNewLayout, VkAccessFlags inNewAccessFlags, VkPipelineStageFlags inNew) {
	VkImageMemoryBarrier imageMemoryBarrier = {};
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.oldLayout = inOldLayout;
	imageMemoryBarrier.newLayout = inNewLayout;
	imageMemoryBarrier.srcAccessMask = inOldAccessFlags;
	imageMemoryBarrier.dstAccessMask = inNewAccessFlags;
	imageMemoryBarrier.image = inImage;
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.subresourceRange = inSubresourceRange;
	vkCmdPipelineBarrier(inCommandBuffer,
		inOld,
		inNew, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
}
void SubmitBufferDataToImage(VkCommandBuffer inCommandBuffer,
	VkBuffer inBuffer, VkImage inImage, int inWidth, int inHeight, int inFaceIndex) {
	VkBufferImageCopy bufferImageCopy = {};
	bufferImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bufferImageCopy.imageSubresource.baseArrayLayer = inFaceIndex;//face : px,nx,py,ny,pz,nz
	bufferImageCopy.imageSubresource.layerCount = 1;
	bufferImageCopy.imageSubresource.mipLevel = 0;

	bufferImageCopy.imageOffset = { 0,0,0 };
	bufferImageCopy.imageExtent = { uint32_t(inWidth),uint32_t(inHeight),1 };

	vkCmdCopyBufferToImage(inCommandBuffer, inBuffer, inImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopy);
}
void TextureSubData(VkImage inTargetImage, const void* inPixelData,
	int inImageWidth, int inImageHeight, int inImageSizeinBytes) {//RGBA32F
	//command buffer
	VkCommandBuffer commandBuffer = CreateCommandBuffer();
	BeginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	//image -> transfer dst,barrier

	VkImageSubresourceRange imageSubresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT ,0,1,0,1 };
	TransferImageLayout(commandBuffer, inTargetImage, imageSubresourceRange,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

	//buffer -> cpu 
	Buffer* tempBuffer = GenBufferObject(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, nullptr, inImageSizeinBytes);
	void* memPtr = nullptr;
	vkMapMemory(sDevice, tempBuffer->mMemory, 0, inImageSizeinBytes, 0, &memPtr);
	memcpy(memPtr, inPixelData, inImageSizeinBytes);
	vkUnmapMemory(sDevice, tempBuffer->mMemory);
	//copy image data from temp buffer -> image devicel local memory
	SubmitBufferDataToImage(commandBuffer, tempBuffer->mBuffer, inTargetImage, inImageWidth, inImageHeight, 0);
	//image -> shader read,barrier
	imageSubresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT ,0,1,0,1 };
	TransferImageLayout(commandBuffer, inTargetImage, imageSubresourceRange,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

	vkEndCommandBuffer(commandBuffer);//fence
	VkFence fence;
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	vkCreateFence(sDevice, &fenceCreateInfo, nullptr, &fence);
	vkResetFences(sDevice, 1, &fence);
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	vkQueueSubmit(GetGraphicQueue(), 1, &submitInfo, fence);
	VkResult result = vkWaitForFences(sDevice, 1, &fence, true, 1000000000000);
	if (result == VK_SUCCESS) {
		printf("upload image to texture success!\n");
	}
	vkDestroyBuffer(sDevice, tempBuffer->mBuffer, nullptr);
	vkFreeMemory(sDevice, tempBuffer->mMemory, nullptr);
	vkDestroyFence(sDevice, fence, nullptr);
	vkFreeCommandBuffers(sDevice, sCommandPool, 1, &commandBuffer);
}
void SubmitCubeMapData(VkImage inTargetImage, void** inPixelData,
	int inImageWidth, int inImageHeight, int inImageSizeinBytes) {
	//command buffer
	VkCommandBuffer commandBuffer = CreateCommandBuffer();
	BeginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	//image -> transfer dst,barrier
	//buffer -> cpu 
	Buffer* tempBuffer[6];
	for (int i = 0; i < 6; i++) {
		VkImageSubresourceRange imageSubresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT ,0,1,uint32_t(i),1 };
		TransferImageLayout(commandBuffer, inTargetImage, imageSubresourceRange,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

		tempBuffer[i] = GenBufferObject(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, inPixelData[i], inImageSizeinBytes);
		SubmitBufferDataToImage(commandBuffer, tempBuffer[i]->mBuffer, inTargetImage, inImageWidth, inImageHeight, i);
		TransferImageLayout(commandBuffer, inTargetImage, imageSubresourceRange,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

	}
	//image -> shader read,barrier
	vkEndCommandBuffer(commandBuffer);//fence
	VkFence fence;
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	vkCreateFence(sDevice, &fenceCreateInfo, nullptr, &fence);
	vkResetFences(sDevice, 1, &fence);
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	vkQueueSubmit(GetGraphicQueue(), 1, &submitInfo, fence);
	VkResult result = vkWaitForFences(sDevice, 1, &fence, true, 1000000000000);
	if (result == VK_SUCCESS) {
		printf("upload image to texture success!\n");
	}
	for (int i = 0; i < 6; i++) {
		vkDestroyBuffer(sDevice, tempBuffer[i]->mBuffer, nullptr);
		vkFreeMemory(sDevice, tempBuffer[i]->mMemory, nullptr);
	}
	vkDestroyFence(sDevice, fence, nullptr);
	vkFreeCommandBuffers(sDevice, sCommandPool, 1, &commandBuffer);
}
VkSampler GenSampler(VkFilter inMinFilter, VkFilter inMagFilter,
	VkSamplerAddressMode inWrapU, VkSamplerAddressMode inWrapV, VkSamplerAddressMode inWrapW) {
	VkSampler sampler;
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.minFilter = inMinFilter;
	samplerCreateInfo.magFilter = inMagFilter;
	samplerCreateInfo.anisotropyEnable = false;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerCreateInfo.addressModeU = inWrapU;
	samplerCreateInfo.addressModeV = inWrapV;
	samplerCreateInfo.addressModeW = inWrapW;
	vkCreateSampler(sDevice, &samplerCreateInfo, nullptr, &sampler);
	return sampler;
}
void BeginEvent(VkCommandBuffer inCommandBuffer, const char* inName) {
	VkDebugUtilsLabelEXT label = {};
	label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	label.pLabelName = inName;
	label.color[0] = 1.0f;
	label.color[1] = 1.0f;
	label.color[2] = 1.0f;
	label.color[3] = 1.0f;

	__vkCmdBeginDebugUtilsLabelEXT(inCommandBuffer, &label);
}
void EndEvent(VkCommandBuffer inCommandBuffer) {
	__vkCmdEndDebugUtilsLabelEXT(inCommandBuffer);
}
void SetObjectName(VkObjectType inType,void*inObject, const char* inName) {
	VkDebugUtilsObjectNameInfoEXT nameInfo = {};
	nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	nameInfo.objectType = inType;
	nameInfo.objectHandle = reinterpret_cast<uint64_t>(inObject);
	nameInfo.pObjectName = inName;
	__vkSetDebugUtilsObjectNameEXT(sDevice, &nameInfo);
}
void GlobalConstants::SetProjectionMatrix(const float* inMatrix) {
	memcpy(mProjectionMatrix, inMatrix, sizeof(mProjectionMatrix));
}
void GlobalConstants::SetViewMatrix(float* inMatrix) {
	inMatrix[12] = 0.0f;
	inMatrix[13] = 0.0f;
	inMatrix[14] = 0.0f;
	memcpy(mViewMatrix, inMatrix, sizeof(mViewMatrix));
}
void GlobalConstants::SetModelMatrix(const float* inMatrix) {
	memcpy(mModelMatrix, inMatrix, sizeof(mModelMatrix));
}
void GlobalConstants::SetMisc0(unsigned int x, unsigned int y, unsigned int z, unsigned int w) {
	mMisc0[0] = x;
	mMisc0[1] = y;
	mMisc0[2] = z;
	mMisc0[3] = w;
}

void GlobalConstants::SetCameraPositionWS(float inX, float inY, float inZ, float inW) {
	mCameraPositionWS[0] = inX;
	mCameraPositionWS[1] = inY;
	mCameraPositionWS[2] = inZ;
	mCameraPositionWS[3] = inW;
}
void GlobalConstants::SetCameraViewDirectionWS(float inX, float inY, float inZ, float inW) {
	mViewDirectionWS[0] = inX;
	mViewDirectionWS[1] = inY;
	mViewDirectionWS[2] = inZ;
	mViewDirectionWS[3] = inW;
}
#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include<vulkan/vulkan.h>
#ifdef _WINDOWS
#include<Windows.h>
#include <vector>
struct InitVulkanUserData {
	HINSTANCE mApplicationInstance;
	HWND mHWND;
};
#endif
struct GlobalConstants {
	union {
		struct {
			float mProjectionMatrix[16];
			float mViewMatrix[16];//view -> null translate
			float mModelMatrix[16];
			unsigned int mMisc0[4];//0xFFFFFFFFu
			float mCameraPositionWS[4];//x,y,z,(w lodscale)
			float mViewDirectionWS[4];//x,y,z,(w lodscale for hard/soft rasterize)
		};
		float mData[1024];
	};
	void SetProjectionMatrix(const float*inMatrix);
	void SetViewMatrix(float* inMatrix);
	void SetModelMatrix(const float* inMatrix);
	void SetMisc0(unsigned int x, unsigned int y, unsigned int z, unsigned int w);
	void SetCameraPositionWS(float inX, float inY, float inZ, float inW = 0.0f);
	void SetCameraViewDirectionWS(float inX, float inY, float inZ, float inW = 0.0f);
};
struct Texture {
	VkImage mImage;
	VkDeviceMemory mMemory;
	VkImageView mImageView;
	VkFormat mFormat;
	VkImageAspectFlags mImageAspectFlag;
	Texture() {
		mImage = nullptr;
		mMemory = nullptr;
		mImageView = nullptr;
		mFormat = VK_FORMAT_UNDEFINED;
		mImageAspectFlag = VK_IMAGE_ASPECT_NONE;
	}
};
struct Texture2D : public Texture {
	int mWidth, mHeight;
	int mChannelCount;
	Texture2D() {
		mWidth = 0;
		mHeight = 0;
		mChannelCount = 0;
	}
};
struct Buffer {
	VkBuffer mBuffer;
	VkDeviceMemory mMemory;
	int mSize;
	Buffer();
	~Buffer();
};
struct ShaderParameterDescription {
	VkDescriptorSetLayout mDescriptorSetLayout;
	VkPipelineLayout mPipelineLayout;
};
bool InitVulkan(void* inUserData, int inCanvasWidth, int inCanvasHeight);
VkCommandBuffer CreateCommandBuffer(
	VkCommandBufferLevel inCommandBufferLevel = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
void BeginCommandBuffer(VkCommandBuffer inCommandBuffer,
	VkCommandBufferUsageFlagBits inUsage);
uint32_t BeginSwapChainRenderPass(VkCommandBuffer inCommandBuffer);
void EndSwapChainRenderPass(VkCommandBuffer inCommandBuffer);
VkQueue GetGraphicQueue();
VkDevice GetVulkanDevice();
VkPhysicalDevice GetPhysicalDevice();
VkRenderPass GetSwapChainRenderPass();
Buffer* GenBufferObject(VkBufferUsageFlags inBufferUsageFlag,
	VkMemoryPropertyFlagBits inMemoryPropertyFlagBits, const void* inData = nullptr, int inLen = 0);
void BufferSubData(Buffer* buffer, const  void* data, VkDeviceSize size);
ShaderParameterDescription* GetUberPassShaderParameterDescription();
VkPipeline CreatePSO(VkRenderPass inRenderPass, VkPrimitiveTopology inPrimitiveType,
	const std::vector<VkVertexInputBindingDescription>& inVertexInputBindingDescriptions,
	const std::vector<VkVertexInputAttributeDescription>& inVertexInputAttributeDescriptions,
	const VkShaderModule inVS, const VkShaderModule inFS);
VkPipeline CreatePSOVGF(
	const std::vector<VkVertexInputBindingDescription>& inVertexInputBindingDescriptions,
	const std::vector<VkVertexInputAttributeDescription>& inVertexInputAttributeDescriptions,
	const VkShaderModule inVS, const VkShaderModule inGS, const VkShaderModule inFS);
VkPipeline CreatePSOVTF(VkRenderPass inRenderPass,
	const std::vector<VkVertexInputBindingDescription>& inVertexInputBindingDescriptions,
	const std::vector<VkVertexInputAttributeDescription>& inVertexInputAttributeDescriptions,
	const VkShaderModule inVS, const VkShaderModule inTCS, const VkShaderModule inTES,
	const VkShaderModule inFS);
VkShaderModule CompileShader(const char* inFilePath);
VkFramebuffer* GetSwapChainFrameBuffers();
void TransferImageLayout(VkCommandBuffer inCommandBuffer, VkImage inImage,
	VkImageSubresourceRange inSubresourceRange,
	VkImageLayout inOldLayout, VkAccessFlags inOldAccessFlags, VkPipelineStageFlags inOld,
	VkImageLayout inNewLayout, VkAccessFlags inNewAccessFlags, VkPipelineStageFlags inNew);
void GenImage(Texture* inOutTexture, int inWidth, int inHeight,
	VkImageUsageFlags inUsage, VkMemoryPropertyFlagBits inPreferedMemoryPropertyFlagBits);
void SubmitBufferDataToImage(VkCommandBuffer inCommandBuffer,
	VkBuffer inBuffer, VkImage inImage, int inWidth, int inHeight, int inFaceIndex);
void TextureSubData(VkImage inTargetImage, const void* inPixelData,
	int inImageWidth, int inImageHeight, int inImageSizeinBytes);
VkImageView GenImageView2D(VkImage inImage, VkFormat inFormat, VkImageAspectFlags inImageAspectFlag);
VkSampler GenSampler(VkFilter inMinFilter = VK_FILTER_LINEAR,
	VkFilter inMagFilter = VK_FILTER_LINEAR,
	VkSamplerAddressMode inWrapU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
	VkSamplerAddressMode inWrapV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
	VkSamplerAddressMode inWrapW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
void GenImageCubeMap(Texture* inOutTexture, int inWidth, int inHeight,
	VkImageUsageFlags inUsage, VkMemoryPropertyFlagBits inPreferedMemoryPropertyFlagBits);
void SubmitCubeMapData(VkImage inTargetImage, void** inPixelData,
	int inImageWidth, int inImageHeight, int inImageSizeinBytes);
VkImageView GenImageViewCubeMap(VkImage inImage, VkFormat inFormat,
	VkImageAspectFlags inImageAspectFlag);
void BeginEvent(VkCommandBuffer inCommandBuffer, const char* inName);
void EndEvent(VkCommandBuffer inCommandBuffer);
void SetObjectName(VkObjectType inType, void* inObject, const char * inName);
struct ScopedEvent {
	VkCommandBuffer mCommandBuffer;
	ScopedEvent(VkCommandBuffer inCommandBuffer, LPCSTR inName) :mCommandBuffer(inCommandBuffer) {
		BeginEvent(inCommandBuffer,inName);
	}
	~ScopedEvent() {
		EndEvent(mCommandBuffer);
	}
};
#define EVENT_VAR_INNER(CommandBuffer, inName, line) _scopedEvent_##line(CommandBuffer, inName)
#define EventVar(CommandBuffer,inName,n) EVENT_VAR_INNER(CommandBuffer, inName, n)
#define SCOPED_EVENT(CommandBuffer,inName) \
        ScopedEvent EventVar(CommandBuffer,inName,__LINE__)
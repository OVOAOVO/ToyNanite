#pragma once
#include "Vulkan.h"
#include <string>
enum class ERenderPassType {
	ERPT_GRAPHICS,
	ERPT_COMPUTE
};
class RenderPass {
public:
	ERenderPassType mRenderPassType;
	union {
		struct {
			VkShaderModule mVertexShader;
			VkShaderModule mFragmentShader;
		};
		VkShaderModule mComputeShader;
	};
	VkPipeline mPSO;
	VkPipelineLayout mPipelineLayout;
	VkDescriptorSet mDescriptorSet;
	VkDescriptorPool mDescriptorPool;
	std::vector<VkDescriptorSetLayoutBinding> mDescriptorSetLayoutBindings;
	std::vector<VkDescriptorPoolSize> mDescriptorPoolSizes;
	std::vector<Texture2D*> mTextures,mOutputTextures;
	std::vector<Buffer*> mBuffers,mOutputBuffers,mUniformBuffers;
	std::vector<VkWriteDescriptorSet> mWriteDescriptorSets;
	int mDispatchX, mDispatchY, mDispatchZ;
	uint32_t mViewportWidth, mViewportHeight;
	VkRenderPass mRenderPass;
	VkFramebuffer mFrameBuffer;
	std::string mName;
	RenderPass(ERenderPassType inRPT,const char *inName):mRenderPassType(inRPT),mName(inName), mDispatchX(1), mDispatchY(1), mDispatchZ(1), mViewportWidth(0u),mViewportHeight(0u){}
	void SetVSPS(const char* inVSPath, const char* inFSPath);
	void SetUniformBufferObject(int inBindingPoint,Buffer* inUBO);
	void SetCS(const char* inCSPath);
	void SetComputeImage(int inBindingPoint, Texture2D* inImage, bool inIsOutputResource=false);
	void SetSSBO(int inBindingPoint, Buffer* inBuffer, bool inIsOutputResource = false);
	void SetComputeDispatchArgs(int inX,int inY,int inZ);
	void Build(uint32_t inCanvasWidth = 0u,uint32_t inCanvasHeight=0u);
	void Execute();
	void ExecuteIndirect(Buffer*inIndirectBuffer);
};
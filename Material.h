#pragma once
#include "Vulkan.h"
class Material{
public:
	VkDescriptorSet mDescriptorSet;
	VkDescriptorPool mDescriptorPool;
	VkPipeline mPSO;
	VkShaderModule mVertexShader;
	VkShaderModule mTCSShader;
	VkShaderModule mTESShader;
	VkShaderModule mGeometryShader;
	VkShaderModule mFragmentShader;
	int mPSOType;
	VkPrimitiveTopology mPrimitiveType;
	Material();
	void Init(const char* inVSPath, const char* inFSPath);
	void InitVGF(const char* inVSPath, const char* inGSPath, const char* inFSPath);
	void InitVTF(const char* inVSPath, const char* inTCSPath, const char* inTESPath, const char* inFSPath);
	void SetUBO(int inBindingPoint,VkBuffer inUBO,int inUBOSize);
	void SetTexture(int inBindingPoint, VkImageView inImageView, VkSampler inSampler);
	void SetTexture2D(int inBindingPoint, int inDstArrayIndex, VkImageView inImageView, VkSampler inSampler);
	void Active(VkCommandBuffer inCommandBuffer, VkRenderPass inRenderPass);
};


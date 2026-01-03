#include "Material.h"
#include "StaticMesh.h"
Material::Material() {
	mDescriptorSet = nullptr;
	mPSO = nullptr;
	mPrimitiveType = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}
void Material::Init(const char*inVSPath,const char*inFSPath) {
	mPSOType = 0;
	mVertexShader = CompileShader(inVSPath);
	mFragmentShader = CompileShader(inFSPath);
	ShaderParameterDescription* parameterDescription = GetUberPassShaderParameterDescription();

	VkDescriptorPoolSize descriptorPoolSize[2] = {};
	descriptorPoolSize[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorPoolSize[0].descriptorCount = 32;//32 descriptor : ubo,texture,sampler
	descriptorPoolSize[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorPoolSize[1].descriptorCount = 32;//32 descriptor : ubo,texture,sampler

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.maxSets = 1;
	descriptorPoolCreateInfo.poolSizeCount = _countof(descriptorPoolSize);
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSize;

	vkCreateDescriptorPool(GetVulkanDevice(),
		&descriptorPoolCreateInfo, nullptr, &mDescriptorPool);

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.descriptorPool = mDescriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = 1;
	descriptorSetAllocateInfo.pSetLayouts = &parameterDescription->mDescriptorSetLayout;
	vkAllocateDescriptorSets(GetVulkanDevice(),
		&descriptorSetAllocateInfo, &mDescriptorSet);
}
void Material::InitVGF(const char* inVSPath, const char* inGSPath, const char* inFSPath) {
	mPSOType = 1;
	mVertexShader = CompileShader(inVSPath);
	mGeometryShader = CompileShader(inGSPath);
	mFragmentShader = CompileShader(inFSPath);

	ShaderParameterDescription* parameterDescription = GetUberPassShaderParameterDescription();

	VkDescriptorPoolSize descriptorPoolSize[2] = {};
	descriptorPoolSize[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorPoolSize[0].descriptorCount = 32;//32 descriptor : ubo,texture,sampler
	descriptorPoolSize[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorPoolSize[1].descriptorCount = 32;//32 descriptor : ubo,texture,sampler

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.maxSets = 1;
	descriptorPoolCreateInfo.poolSizeCount = _countof(descriptorPoolSize);
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSize;

	vkCreateDescriptorPool(GetVulkanDevice(),
		&descriptorPoolCreateInfo, nullptr, &mDescriptorPool);

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.descriptorPool = mDescriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = 1;
	descriptorSetAllocateInfo.pSetLayouts = &parameterDescription->mDescriptorSetLayout;
	vkAllocateDescriptorSets(GetVulkanDevice(),
		&descriptorSetAllocateInfo, &mDescriptorSet);
}
void Material::InitVTF(const char* inVSPath, const char* inTCSPath, const char* inTESPath, const char* inFSPath) {
	mPSOType = 2;
	mVertexShader = CompileShader(inVSPath);
	mTCSShader = CompileShader(inTCSPath);
	mTESShader = CompileShader(inTESPath);
	mFragmentShader = CompileShader(inFSPath);

	ShaderParameterDescription* parameterDescription = GetUberPassShaderParameterDescription();

	VkDescriptorPoolSize descriptorPoolSize[2] = {};
	descriptorPoolSize[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorPoolSize[0].descriptorCount = 32;//32 descriptor : ubo,texture,sampler
	descriptorPoolSize[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorPoolSize[1].descriptorCount = 32;//32 descriptor : ubo,texture,sampler

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.maxSets = 1;
	descriptorPoolCreateInfo.poolSizeCount = _countof(descriptorPoolSize);
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSize;

	vkCreateDescriptorPool(GetVulkanDevice(),
		&descriptorPoolCreateInfo, nullptr, &mDescriptorPool);

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.descriptorPool = mDescriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = 1;
	descriptorSetAllocateInfo.pSetLayouts = &parameterDescription->mDescriptorSetLayout;
	vkAllocateDescriptorSets(GetVulkanDevice(),
		&descriptorSetAllocateInfo, &mDescriptorSet);
}
void Material::SetUBO(int inBindingPoint, VkBuffer inUBO, int inUBOSize) {
	VkDescriptorBufferInfo bufferInfo[1];
	bufferInfo[0].buffer = inUBO;
	bufferInfo[0].offset = 0;
	bufferInfo[0].range = inUBOSize;

	VkWriteDescriptorSet writeDescriptorSet[1];
	writeDescriptorSet[0] = {};
	writeDescriptorSet[0].descriptorCount = 1;
	writeDescriptorSet[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeDescriptorSet[0].dstArrayElement = 0;
	writeDescriptorSet[0].dstBinding = inBindingPoint;
	writeDescriptorSet[0].dstSet = mDescriptorSet;//->descriptor pool
	writeDescriptorSet[0].pBufferInfo = &bufferInfo[0];
	writeDescriptorSet[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

	vkUpdateDescriptorSets(GetVulkanDevice(), 1, writeDescriptorSet, 0, nullptr);
}
void Material::SetTexture(int inBindingPoint, VkImageView inImageView, VkSampler inSampler) {
	VkDescriptorImageInfo imageInfo[1];
	imageInfo[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo[0].imageView = inImageView;
	imageInfo[0].sampler = inSampler;

	VkWriteDescriptorSet writeDescriptorSet[1];
	writeDescriptorSet[0] = {};
	writeDescriptorSet[0].descriptorCount = 1;
	writeDescriptorSet[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeDescriptorSet[0].dstArrayElement = 0;
	writeDescriptorSet[0].dstBinding = inBindingPoint;
	writeDescriptorSet[0].dstSet = mDescriptorSet;//->descriptor pool

	writeDescriptorSet[0].pImageInfo = imageInfo;
	writeDescriptorSet[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

	vkUpdateDescriptorSets(GetVulkanDevice(), 1, writeDescriptorSet, 0, nullptr);
}
void Material::SetTexture2D(int inBindingPoint,int inDstArrayIndex,VkImageView inImageView, VkSampler inSampler) {
	VkDescriptorImageInfo imageInfo[1];
	imageInfo[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo[0].imageView = inImageView;
	imageInfo[0].sampler = inSampler;

	VkWriteDescriptorSet writeDescriptorSet[1];
	writeDescriptorSet[0] = {};
	writeDescriptorSet[0].descriptorCount = 1;
	writeDescriptorSet[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeDescriptorSet[0].dstArrayElement = inDstArrayIndex;
	writeDescriptorSet[0].dstBinding = inBindingPoint;
	writeDescriptorSet[0].dstSet = mDescriptorSet;//->descriptor pool

	writeDescriptorSet[0].pImageInfo = imageInfo;
	writeDescriptorSet[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

	vkUpdateDescriptorSets(GetVulkanDevice(), 1, writeDescriptorSet, 0, nullptr);
}
void Material::Active(VkCommandBuffer inCommandBuffer, VkRenderPass inRenderPass) {
	if (mPSO == nullptr) {
		//input assembly
		if (mPSOType == 0) {
			mPSO = CreatePSO(inRenderPass,mPrimitiveType,StaticMesh::mVertexInputBindingDescriptions,
				StaticMesh::mVertexInputAttributeDescriptions,
				mVertexShader, mFragmentShader);
		}
		else {
			mPSO = CreatePSOVTF(inRenderPass, StaticMesh::mVertexInputBindingDescriptions,
				StaticMesh::mVertexInputAttributeDescriptions,
				mVertexShader, mTCSShader, mTESShader, mFragmentShader);
		}
	}
	ShaderParameterDescription* parameterDescription = GetUberPassShaderParameterDescription();

	vkCmdBindPipeline(inCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSO);
	vkCmdBindDescriptorSets(inCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, parameterDescription->mPipelineLayout,
		0, 1, &mDescriptorSet, 0, nullptr);//
}
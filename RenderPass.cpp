#include "RenderPass.h"
void RenderPass::SetVSPS(const char* inVSPath, const char* inFSPath) {
	mVertexShader = CompileShader(inVSPath);
	mFragmentShader = CompileShader(inFSPath);
}
void RenderPass::SetCS(const char* inCSPath) {
	mComputeShader = CompileShader(inCSPath);
}
void RenderPass::SetComputeImage(int inBindingPoint, Texture2D* inImage, bool inIsOutputResource) {
	VkDescriptorSetLayoutBinding descriptorSetLayoutBinding;
	descriptorSetLayoutBinding.binding = inBindingPoint;
	descriptorSetLayoutBinding.descriptorCount = 1;
	descriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	descriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	mDescriptorSetLayoutBindings.push_back(descriptorSetLayoutBinding);

	VkDescriptorImageInfo *imageInfo=new VkDescriptorImageInfo;
	imageInfo->imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageInfo->imageView = inImage->mImageView;
	imageInfo->sampler = nullptr;

	VkWriteDescriptorSet writeDescriptorSet;
	writeDescriptorSet = {};
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writeDescriptorSet.dstArrayElement = 0;
	writeDescriptorSet.dstBinding = inBindingPoint;
	writeDescriptorSet.pImageInfo = imageInfo;
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

	mWriteDescriptorSets.push_back(writeDescriptorSet);

	mTextures.push_back(inImage);
	if (inIsOutputResource) {
		mOutputTextures.push_back(inImage);
	}
}
void RenderPass::SetSSBO(int inBindingPoint, Buffer* inBuffer, bool inIsOutputResource) {
	VkDescriptorSetLayoutBinding descriptorSetLayoutBinding;
	descriptorSetLayoutBinding.binding = inBindingPoint;
	descriptorSetLayoutBinding.descriptorCount = 1;
	descriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	mDescriptorSetLayoutBindings.push_back(descriptorSetLayoutBinding);
	VkDescriptorBufferInfo *bufferInfo=new VkDescriptorBufferInfo;
	bufferInfo->buffer = inBuffer->mBuffer;
	bufferInfo->offset = 0;
	bufferInfo->range = inBuffer->mSize;

	VkWriteDescriptorSet writeDescriptorSet;
	writeDescriptorSet = {};
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writeDescriptorSet.dstArrayElement = 0;
	writeDescriptorSet.dstBinding = inBindingPoint;
	writeDescriptorSet.pBufferInfo = bufferInfo;
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

	mWriteDescriptorSets.push_back(writeDescriptorSet);

	mBuffers.push_back(inBuffer);
	if (inIsOutputResource) {
		mOutputBuffers.push_back(inBuffer);
	}
}
void RenderPass::SetUniformBufferObject(int inBindingPoint, Buffer* inBuffer) {
	VkDescriptorSetLayoutBinding descriptorSetLayoutBinding;
	descriptorSetLayoutBinding.binding = inBindingPoint;
	descriptorSetLayoutBinding.descriptorCount = 1;
	descriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
	mDescriptorSetLayoutBindings.push_back(descriptorSetLayoutBinding);
	VkDescriptorBufferInfo* bufferInfo = new VkDescriptorBufferInfo;
	bufferInfo->buffer = inBuffer->mBuffer;
	bufferInfo->offset = 0;
	bufferInfo->range = inBuffer->mSize;

	VkWriteDescriptorSet writeDescriptorSet;
	writeDescriptorSet = {};
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeDescriptorSet.dstArrayElement = 0;
	writeDescriptorSet.dstBinding = inBindingPoint;
	writeDescriptorSet.pBufferInfo = bufferInfo;
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

	mWriteDescriptorSets.push_back(writeDescriptorSet);

	mUniformBuffers.push_back(inBuffer);
}
void RenderPass::SetComputeDispatchArgs(int inX, int inY, int inZ) {
	mDispatchX = inX;
	mDispatchY = inY;
	mDispatchZ = inZ;
}
void RenderPass::Build(uint32_t inCanvasWidth, uint32_t inCanvasHeight) {
	VkDevice device = GetVulkanDevice();
	if (mRenderPassType == ERenderPassType::ERPT_COMPUTE) {
		VkDescriptorSetLayout descriptorSetLayout;
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
		descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutCreateInfo.bindingCount = mDescriptorSetLayoutBindings.size();
		descriptorSetLayoutCreateInfo.pBindings = mDescriptorSetLayoutBindings.data();
		vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout);

		VkPipelineLayout pipelineLayout;
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
		pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
		pipelineLayoutCreateInfo.setLayoutCount = 1;
		pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
		vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
		mPipelineLayout = pipelineLayout;
		VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo = {};
		pipelineShaderStageCreateInfo = {};
		pipelineShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipelineShaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		pipelineShaderStageCreateInfo.module = mComputeShader;
		pipelineShaderStageCreateInfo.pName = "main";

		VkDescriptorSet descriptorSet;
		std::vector<VkDescriptorPoolSize> descriptorPoolSizes;
		if (mUniformBuffers.size() > 0) {
			VkDescriptorPoolSize descriptorPoolSize = {};
			descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorPoolSize.descriptorCount = mUniformBuffers.size();
			descriptorPoolSizes.push_back(descriptorPoolSize);
		}
		if (mTextures.size() > 0) {
			VkDescriptorPoolSize descriptorPoolSize = {};
			descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			descriptorPoolSize.descriptorCount = mTextures.size();
			descriptorPoolSizes.push_back(descriptorPoolSize);
		}
		if (mBuffers.size() > 0) {
			VkDescriptorPoolSize descriptorPoolSize = {};
			descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorPoolSize.descriptorCount = mBuffers.size();
			descriptorPoolSizes.push_back(descriptorPoolSize);
		}

		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
		descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolCreateInfo.maxSets = 1;
		descriptorPoolCreateInfo.poolSizeCount = descriptorPoolSizes.size();
		descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();

		VkDescriptorPool descriptorPool;
		vkCreateDescriptorPool(GetVulkanDevice(), &descriptorPoolCreateInfo, nullptr, &descriptorPool);

		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.descriptorPool = descriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;
		vkAllocateDescriptorSets(GetVulkanDevice(), &descriptorSetAllocateInfo, &descriptorSet);
		mDescriptorSet = descriptorSet;
		for (auto& writeDescriptorSet : mWriteDescriptorSets) {
			writeDescriptorSet.dstSet = mDescriptorSet;
		}

		VkComputePipelineCreateInfo computePipelineCreateInfo = {};
		computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		computePipelineCreateInfo.stage = pipelineShaderStageCreateInfo;
		computePipelineCreateInfo.layout = pipelineLayout;

		vkCreateComputePipelines(device, nullptr, 1, &computePipelineCreateInfo, nullptr, &mPSO);
	}
	else {
		mViewportWidth = inCanvasWidth;
		mViewportHeight = inCanvasHeight;
		VkDescriptorSetLayout descriptorSetLayout;
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
		descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutCreateInfo.bindingCount = mDescriptorSetLayoutBindings.size();
		descriptorSetLayoutCreateInfo.pBindings = mDescriptorSetLayoutBindings.data();
		vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout);

		VkPipelineLayout pipelineLayout;
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
		pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
		pipelineLayoutCreateInfo.setLayoutCount = 1;
		pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
		vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
		mPipelineLayout = pipelineLayout;

		VkPipelineVertexInputStateCreateInfo pipelinVertexInputStateCreateInfo = {};
		pipelinVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		pipelinVertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
		pipelinVertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
		pipelinVertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
		pipelinVertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;
		
		VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo = {};
		pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		pipelineDynamicStateCreateInfo.dynamicStateCount = 0;
		pipelineDynamicStateCreateInfo.pDynamicStates = nullptr;
		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = float(inCanvasWidth);
		viewport.height = float(inCanvasHeight);
		viewport.maxDepth = 1.0f;
		viewport.minDepth = 0.0f;
		VkRect2D scissor = {};
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = inCanvasWidth;
		scissor.extent.height = inCanvasHeight;
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
		pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
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
		pipelineShaderStageCreateInfo[0].module = mVertexShader;
		pipelineShaderStageCreateInfo[0].pName = "main";
		pipelineShaderStageCreateInfo[1] = {};
		pipelineShaderStageCreateInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipelineShaderStageCreateInfo[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		pipelineShaderStageCreateInfo[1].module = mFragmentShader;
		pipelineShaderStageCreateInfo[1].pName = "main";
		// ubo -> write descriptor set -> descriptor set

		VkDescriptorSet descriptorSet;
		std::vector<VkDescriptorPoolSize> descriptorPoolSizes;
		if (mUniformBuffers.size() > 0) {
			VkDescriptorPoolSize descriptorPoolSize = {};
			descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorPoolSize.descriptorCount = mUniformBuffers.size();
			descriptorPoolSizes.push_back(descriptorPoolSize);
		}
		if (mTextures.size() > 0) {
			VkDescriptorPoolSize descriptorPoolSize = {};
			descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			descriptorPoolSize.descriptorCount = mTextures.size();
			descriptorPoolSizes.push_back(descriptorPoolSize);
		}
		if (mBuffers.size() > 0) {
			VkDescriptorPoolSize descriptorPoolSize = {};
			descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorPoolSize.descriptorCount = mBuffers.size();
			descriptorPoolSizes.push_back(descriptorPoolSize);
		}

		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
		descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolCreateInfo.maxSets = 1;
		descriptorPoolCreateInfo.poolSizeCount = descriptorPoolSizes.size();
		descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();

		VkDescriptorPool descriptorPool;
		vkCreateDescriptorPool(GetVulkanDevice(), &descriptorPoolCreateInfo, nullptr, &descriptorPool);

		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.descriptorPool = descriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;
		vkAllocateDescriptorSets(GetVulkanDevice(), &descriptorSetAllocateInfo, &descriptorSet);
		mDescriptorSet = descriptorSet;
		for (auto& writeDescriptorSet : mWriteDescriptorSets) {
			writeDescriptorSet.dstSet = mDescriptorSet;
		}

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		VkRenderPassCreateInfo renderPassCreateInfo = {};
		renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCreateInfo.attachmentCount = 0;
		renderPassCreateInfo.subpassCount = 1;
		renderPassCreateInfo.pSubpasses = &subpass;
		vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &mRenderPass);

		{
			VkFramebufferCreateInfo frameBufferCreateInfo = {};
			frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			frameBufferCreateInfo.renderPass = mRenderPass;
			frameBufferCreateInfo.width = inCanvasWidth;
			frameBufferCreateInfo.height = inCanvasHeight;
			frameBufferCreateInfo.layers = 1;

			vkCreateFramebuffer(device, &frameBufferCreateInfo, nullptr, &mFrameBuffer);
		}
		VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
		graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		graphicsPipelineCreateInfo.renderPass = mRenderPass;
		graphicsPipelineCreateInfo.stageCount = 2;
		graphicsPipelineCreateInfo.basePipelineIndex = -1;
		graphicsPipelineCreateInfo.pVertexInputState = &pipelinVertexInputStateCreateInfo;
		graphicsPipelineCreateInfo.pDynamicState = &pipelineDynamicStateCreateInfo;
		graphicsPipelineCreateInfo.pViewportState = &pipelineViewportStateCreateInfo;
		graphicsPipelineCreateInfo.pInputAssemblyState = &pipelineIAStateCreateInfo;
		graphicsPipelineCreateInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
		graphicsPipelineCreateInfo.pMultisampleState = &pipelineMultisampleStateCreateInfo;
		graphicsPipelineCreateInfo.pDepthStencilState = nullptr;
		graphicsPipelineCreateInfo.pColorBlendState = &pipelineColorBlendStateCreateInfo;
		graphicsPipelineCreateInfo.pStages = pipelineShaderStageCreateInfo;
		graphicsPipelineCreateInfo.layout = mPipelineLayout;
		vkCreateGraphicsPipelines(device, nullptr, 1, &graphicsPipelineCreateInfo, nullptr, &mPSO);
	}
}
void RenderPass::Execute() {
	VkDevice device = GetVulkanDevice();
	VkCommandBuffer commandBuffer = CreateCommandBuffer();
	BeginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	if(mRenderPassType==ERenderPassType::ERPT_COMPUTE)
	{
		{
			SCOPED_EVENT(commandBuffer, mName.c_str());
			vkUpdateDescriptorSets(device, mWriteDescriptorSets.size(), mWriteDescriptorSets.data(), 0, nullptr);
			for (auto& outputImage : mOutputTextures) {
				VkImageSubresourceRange imageSubresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT ,0,1,0,1 };
				TransferImageLayout(commandBuffer, outputImage->mImage, imageSubresourceRange,
					VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_NONE,
					VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
					VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT,
					VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
			}

			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mPSO);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
				mPipelineLayout,
				0, 1, &mDescriptorSet, 0, nullptr);
			//dispatch compute
			vkCmdDispatch(commandBuffer, mDispatchX, mDispatchY, mDispatchZ);
			for (auto& outputImage : mOutputTextures) {
				VkImageSubresourceRange imageSubresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT ,0,1,0,1 };
				TransferImageLayout(commandBuffer, outputImage->mImage, imageSubresourceRange,
					VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT,
					VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
					VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
			}
		}
		vkEndCommandBuffer(commandBuffer);
		VkFence fence;
		VkFenceCreateInfo fenceCreateInfo = {};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		vkCreateFence(device, &fenceCreateInfo, nullptr, &fence);
		vkResetFences(device, 1, &fence);
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		vkQueueSubmit(GetGraphicQueue(), 1, &submitInfo, fence);
		VkResult result = vkWaitForFences(device, 1, &fence, true, 1000000000000);
		if (result != VK_SUCCESS) {
			printf("EXECUTE Failed!\n");
		}
	}
}
void RenderPass::ExecuteIndirect(Buffer* inIndirectBuffer) {
	VkDevice device = GetVulkanDevice();
	VkCommandBuffer commandBuffer = CreateCommandBuffer();
	BeginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	{
		SCOPED_EVENT(commandBuffer, mName.c_str());
		vkUpdateDescriptorSets(device, mWriteDescriptorSets.size(), mWriteDescriptorSets.data(), 0, nullptr);
		VkClearValue clearValues[2];
		clearValues[0].color = { 0.0f,0.0f,0.0f,1.0f };
		clearValues[1].depthStencil = { 1.0f,0u };
		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.framebuffer = mFrameBuffer;
		renderPassBeginInfo.pClearValues = clearValues;
		renderPassBeginInfo.renderArea.offset = { 0,0 };
		renderPassBeginInfo.renderArea.extent = { mViewportWidth,mViewportHeight };
		renderPassBeginInfo.renderPass = mRenderPass;

		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSO);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout,
			0, 1, &mDescriptorSet, 0, nullptr);

		vkCmdDrawIndirect(commandBuffer, inIndirectBuffer->mBuffer, 0, 1, 16);
	}

	vkCmdEndRenderPass(commandBuffer);
	vkEndCommandBuffer(commandBuffer);
	VkFence fence;
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	vkCreateFence(device, &fenceCreateInfo, nullptr, &fence);
	vkResetFences(device, 1, &fence);
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	vkQueueSubmit(GetGraphicQueue(), 1, &submitInfo, fence);
	VkResult result = vkWaitForFences(device, 1, &fence, true, 1000000000000);
	if (result != VK_SUCCESS) {
		printf("EXECUTE Failed!\n");
	}
}
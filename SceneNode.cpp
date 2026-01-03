#include "SceneNode.h"
#include "quaternion.h"

SceneNode::SceneNode():mScale(1.0f) {
	mbNeedUpdate = true;
	mbGeneratedDrawCommand = false;
	mCachedDrawCommand = nullptr;
}
void SceneNode::SetPosition(float inX, float inY, float inZ) {
	mPosition.x = inX;
	mPosition.y = inY;
	mPosition.z = inZ;
	mbNeedUpdate = true;
}
void SceneNode::SetRotation(float inX, float inY, float inZ) {

}
void SceneNode::SetScale(float inX, float inY, float inZ) {
	mScale.x = inX;
	mScale.y = inY;
	mScale.z = inZ;
	mbNeedUpdate = true;
}
static bool sUseCahedCommandBuffer = false;
void SceneNode::Draw(VkCommandBuffer inCommandBuffer, VkRenderPass inRenderPass, matrix4& inProjectionMatrix, matrix4& inViewMatrix) {
	if (mbNeedUpdate) {
		matrix3 scaleMatrix;
		scaleMatrix.LoadIdentity();
		scaleMatrix.SetScale(mScale.x, mScale.y, mScale.z);
		matrix3 lt3x3=scaleMatrix*quaternion(mRotation.x, mRotation.y, mRotation.z).toMatrix3();
		mModelMatrix.LoadIdentity();
		mModelMatrix.SetLeftTop3x3(lt3x3);
		mModelMatrix.Tranlate(mPosition.x, mPosition.y, mPosition.z);
		mNormalMatrix = mModelMatrix.Invert();
		mNormalMatrix.Transpose();
		if (mUBO == nullptr) {
			mUBO = GenBufferObject(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,nullptr,sizeof(matrix4)*1024);
			mStaticMesh->mMaterial.SetUBO(0, mUBO->mBuffer, sizeof(matrix4) * 1024);
			mUBO1 = GenBufferObject(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, nullptr, sizeof(matrix4) * 1024);
			mStaticMesh->mMaterial.SetUBO(1, mUBO1->mBuffer, sizeof(matrix4) * 1024);
		}
		{
			VkDevice device = GetVulkanDevice();
			void* memPtr = nullptr;
			vkMapMemory(device, mUBO->mMemory, 0, sizeof(matrix4) * 1024, 0, &memPtr);
			memcpy(memPtr, &mModelMatrix, sizeof(matrix4));
			memcpy(((char*)memPtr) + sizeof(matrix4), &mNormalMatrix, sizeof(matrix4));
			vkUnmapMemory(device, mUBO->mMemory);
		}
		{
			float ubo1data[] = {
				1.0f,0.0f,0.0f,0.0f,
				-1.0f,0.0f,0.0f,0.0f,
				1.0f,0.0f,0.0f,0.0f
			};
			VkDevice device = GetVulkanDevice();
			void* memPtr = nullptr;
			vkMapMemory(device, mUBO1->mMemory, 0, sizeof(matrix4) * 1024, 0, &memPtr);
			memcpy(memPtr, &ubo1data, sizeof(ubo1data));
			vkUnmapMemory(device, mUBO1->mMemory);
		}

		mbNeedUpdate = false;
	}
	if (sUseCahedCommandBuffer) {
		GenerateDrawCommand(inProjectionMatrix,inViewMatrix);
	}
	else {
		if (mStaticMesh != nullptr) {
			mStaticMesh->mMaterial.Active(inCommandBuffer,inRenderPass);
			mStaticMesh->Draw(inCommandBuffer);
		}
	}
}
void SceneNode::GenerateDrawCommand(matrix4& inProjectionMatrix, matrix4& inViewMatrix) {
	if (mbGeneratedDrawCommand) {
		return;
	}
	mbGeneratedDrawCommand = true;
	mCachedDrawCommand = new VkCommandBuffer[2];
	VkFramebuffer* swapChainFrameBuffer = GetSwapChainFrameBuffers();
	ShaderParameterDescription* parameterDescription = GetUberPassShaderParameterDescription();
	for (int i = 0; i < 2; i++) {
		mCachedDrawCommand[i] = CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_SECONDARY);
		VkCommandBufferInheritanceInfo commandBufferInheritanceInfo = {};
		commandBufferInheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		commandBufferInheritanceInfo.framebuffer = swapChainFrameBuffer[i];
		commandBufferInheritanceInfo.renderPass = GetSwapChainRenderPass();
		VkCommandBufferBeginInfo commandBufferBeginInfo = {};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;//
		commandBufferBeginInfo.pInheritanceInfo = &commandBufferInheritanceInfo;
		vkBeginCommandBuffer(mCachedDrawCommand[i], &commandBufferBeginInfo);
		vkCmdPushConstants(mCachedDrawCommand[i], parameterDescription->mPipelineLayout,
			VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(matrix4), &inProjectionMatrix);
		vkCmdPushConstants(mCachedDrawCommand[i], parameterDescription->mPipelineLayout,
			VK_SHADER_STAGE_VERTEX_BIT, sizeof(matrix4), sizeof(matrix4), &inViewMatrix);
		if (mStaticMesh != nullptr) {
			mStaticMesh->mMaterial.Active(mCachedDrawCommand[i],nullptr);
			mStaticMesh->Draw(mCachedDrawCommand[i]);
		}
		vkEndCommandBuffer(mCachedDrawCommand[i]);
	}
}
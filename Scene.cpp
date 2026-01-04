#include "Scene.h"
#include "Vulkan.h"
#include <stdio.h>
#include "matrix4.h"
#include "quaternion.h"
#include "RenderPass.h"
#include "StaticMesh.h"
#include "SceneNode.h"
#define _4MB 4194304

static Buffer* sBVHBuffer, *sEchoBuffer, *sMainAndPostNodeAndClusterBatches;
static RenderPass* sInitPass;

unsigned char* LoadFileContent(const char* inFilePath, size_t& outFileSize) {
	FILE* file = nullptr;
	errno_t err = fopen_s(&file, inFilePath, "rb");
	if (err != 0) {
		return nullptr;
	}
	fseek(file, 0, SEEK_END);
	outFileSize = ftell(file);
	fseek(file, 0, SEEK_SET);
	unsigned char* fileContent = new unsigned char[outFileSize];
	fread(fileContent, 1, outFileSize, file);
	fclose(file);
	return fileContent;
}
void InitScene(int inCanvasWidth, int inCanvasHeight) {
	sEchoBuffer = GenBufferObject(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, nullptr, _4MB);
	sMainAndPostNodeAndClusterBatches = GenBufferObject(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, nullptr, _4MB);
	SetObjectName(VK_OBJECT_TYPE_BUFFER, sMainAndPostNodeAndClusterBatches->mBuffer, "MainAndPostNodeAndClusterBatches");
	{
		size_t fileSize = 0;
		unsigned char* fileContent = LoadFileContent("Res/HierarchyBuffer.data", fileSize);
		sBVHBuffer = GenBufferObject(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, fileContent, fileSize);
		delete[] fileContent;
		SetObjectName(VK_OBJECT_TYPE_BUFFER, sBVHBuffer->mBuffer, "HierarchyBuffer");
	}

	//InitPass ->(Unreal) RasterClear
	{
		sInitPass = new RenderPass(ERenderPassType::ERPT_COMPUTE, "Init");
		sInitPass->SetSSBO(0, sBVHBuffer);//Shader Storage Buffer Object
		sInitPass->SetSSBO(1, sEchoBuffer, true);
		sInitPass->SetSSBO(2, sMainAndPostNodeAndClusterBatches, true);
		sInitPass->SetCS("Res/Shaders/Init.spv");
		sInitPass->SetComputeDispatchArgs(1, 1, 1);
		sInitPass->Build();
	}
}
void RenderOneFrame(float inFrameTimeInSecond) {
	sInitPass->Execute();
	VkCommandBuffer commandBuffer = CreateCommandBuffer();
	BeginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	{
		SCOPED_EVENT(commandBuffer, "SwapChain");
		BeginSwapChainRenderPass(commandBuffer);
		ShaderParameterDescription* parameterDescription = GetUberPassShaderParameterDescription();
	}
	EndSwapChainRenderPass(commandBuffer);
}
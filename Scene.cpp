#include "Scene.h"
#include "Vulkan.h"
#include <stdio.h>
#include "matrix4.h"
#include "quaternion.h"
#include "RenderPass.h"
#include "StaticMesh.h"
#include "SceneNode.h"
#define _4MB 4194304
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

}
void RenderOneFrame(float inFrameTimeInSecond) {
	VkCommandBuffer commandBuffer = CreateCommandBuffer();
	BeginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	{
		SCOPED_EVENT(commandBuffer, "SwapChain");
		BeginSwapChainRenderPass(commandBuffer);
		ShaderParameterDescription* parameterDescription = GetUberPassShaderParameterDescription();
	}
	EndSwapChainRenderPass(commandBuffer);
}
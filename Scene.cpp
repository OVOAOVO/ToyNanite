#include "Scene.h"
#include "Vulkan.h"
#include <stdio.h>
#include "matrix4.h"
#include "quaternion.h"
#include "RenderPass.h"
#include "StaticMesh.h"
#include "SceneNode.h"
#define _4MB 4194304

//NOTO:Road
//init args/visBuffer64
//node and cluster cull
//cluster cull
//hw/sw
//cluster color(rgba32float)
//swapChain

static float sLODScale, sLODScaleHW;
static float4 sCameraPositionWS(-330.f, 330.f, -330.f), sCameraTargetPositionWS(0.f, 80.f ,0.f);
static matrix4 sProjectionMatrix, sViewMatrix, sModelMatrix;
static GlobalConstants sGlobalConstantsData;

static Buffer* sGlobalConstantsBuffer,* sBVHBuffer, *sEchoBuffer, *sWorkArgsBuffer[2], * sMainAndPostNodeAndClusterBatches,
				*sVisBuffer64, *sNaniteMesh, *sVisiableClusterSHWH;
static Texture2D* sVisualizationTexture;
static RenderPass* sInitPass, *sNodeAndClusterCullPass[4], *sClusterCullPass, *sHWRasterizePass, *sVisualizePass;
static SceneNode* sFSQNode;

static int sCurrentMipLevelIndex = 0;
static int sAvaliableMipLevels[] = { 0 ,1, 2 ,3 ,4 ,5 ,6 ,7 ,8 ,9 ,10 };

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
	StaticMesh::Init();
	sProjectionMatrix.Perspective(90.f, float(inCanvasWidth) / float(inCanvasHeight), 10.f, 10000.f);
	sViewMatrix.LookAt(sCameraPositionWS, sCameraTargetPositionWS, float4(0.f, 1.f, 0.f));
	matrix3 scaleMatrix;
	scaleMatrix.LoadIdentity();
	matrix3 lt3x3 = scaleMatrix * quaternion(180.f, 0.f, 0.f).toMatrix3();
	sModelMatrix.LoadIdentity();
	sModelMatrix.SetLeftTop3x3(lt3x3);

	float ViewToPixels = 0.5f * sProjectionMatrix.v[5] * float(inCanvasHeight);
	sLODScale = ViewToPixels / 1.f;
	sLODScaleHW = ViewToPixels / 32.f;

	sGlobalConstantsData.SetProjectionMatrix(sProjectionMatrix.v);
	sGlobalConstantsData.SetViewMatrix(sViewMatrix.v);
	sGlobalConstantsData.SetModelMatrix(sModelMatrix.v);
	sGlobalConstantsData.SetMisc0(sAvaliableMipLevels[sCurrentMipLevelIndex], 0, 0, 0);

	float4 viewDirection = sCameraTargetPositionWS - sCameraPositionWS;
	viewDirection.Normalize();
	sGlobalConstantsData.SetCameraPositionWS(sCameraPositionWS.x, sCameraPositionWS.y, sCameraPositionWS.z, sLODScale);
	sGlobalConstantsData.SetCameraViewDirectionWS(viewDirection.x, viewDirection.y, viewDirection.z, sLODScaleHW);
	
	sGlobalConstantsBuffer = GenBufferObject(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, nullptr, 65536);
	SetObjectName(VK_OBJECT_TYPE_BUFFER, sGlobalConstantsBuffer->mBuffer, "GlobalConstants");

	{
		sVisualizationTexture = new Texture2D;
		sVisualizationTexture->mFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
		sVisualizationTexture->mImageAspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
		GenImage(sVisualizationTexture, inCanvasWidth, inCanvasHeight,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		sVisualizationTexture->mImageView = GenImageView2D(sVisualizationTexture->mImage, sVisualizationTexture->mFormat, sVisualizationTexture->mImageAspectFlag);
		sVisualizationTexture->mWidth = inCanvasWidth;
		sVisualizationTexture->mHeight = inCanvasHeight;
		sVisualizationTexture->mChannelCount = 4;
		SetObjectName(VK_OBJECT_TYPE_IMAGE, sVisualizationTexture->mImage, "VisualizationTexture");
	}

	sEchoBuffer = GenBufferObject(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, nullptr, _4MB);
	SetObjectName(VK_OBJECT_TYPE_BUFFER, sEchoBuffer->mBuffer, "EchoBuffer");

	sVisBuffer64 = GenBufferObject(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, nullptr, inCanvasWidth*inCanvasHeight*sizeof(uint64_t));
	SetObjectName(VK_OBJECT_TYPE_BUFFER, sVisBuffer64->mBuffer, "VisBuffer64");

	sWorkArgsBuffer[0] = GenBufferObject(
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, nullptr, _4MB);
	SetObjectName(VK_OBJECT_TYPE_BUFFER, sWorkArgsBuffer[0]->mBuffer, "WorkArgsBuffer[0]");

	sWorkArgsBuffer[1] = GenBufferObject(
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, nullptr, _4MB);
	SetObjectName(VK_OBJECT_TYPE_BUFFER, sWorkArgsBuffer[1]->mBuffer, "WorkArgsBuffer[1]");

	sMainAndPostNodeAndClusterBatches = GenBufferObject(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, nullptr, _4MB);
	SetObjectName(VK_OBJECT_TYPE_BUFFER, sMainAndPostNodeAndClusterBatches->mBuffer, "MainAndPostNodeAndClusterBatches");
	
	sVisiableClusterSHWH = GenBufferObject(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, nullptr, _4MB);
	SetObjectName(VK_OBJECT_TYPE_BUFFER, sVisiableClusterSHWH->mBuffer, "VisiableClusterSHWH");

	{
		size_t fileSize = 0;
		unsigned char* fileContent = LoadFileContent("Res/mitsuba.bvh", fileSize);
		sBVHBuffer = GenBufferObject(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, fileContent, fileSize);
		delete[] fileContent;
		SetObjectName(VK_OBJECT_TYPE_BUFFER, sBVHBuffer->mBuffer, "HierarchyBuffer");

		fileContent = LoadFileContent("Res/mitsuba.nanitemesh", fileSize);
		sNaniteMesh = GenBufferObject(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, fileContent, fileSize);
		delete[] fileContent;
		SetObjectName(VK_OBJECT_TYPE_BUFFER, sBVHBuffer->mBuffer, "NaniteMesh");
	}

	//InitPass ->(Unreal) RasterClear
	{
		sInitPass = new RenderPass(ERenderPassType::ERPT_COMPUTE, "Init");
		sInitPass->SetSSBO(0, sWorkArgsBuffer[0], true);
		sInitPass->SetSSBO(1, sWorkArgsBuffer[1], true);
		sInitPass->SetSSBO(2, sMainAndPostNodeAndClusterBatches, true);
		sInitPass->SetSSBO(3, sVisBuffer64, true);
		sInitPass->SetCS("Res/Shaders/Init.spv");
		sInitPass->SetComputeDispatchArgs(int(ceilf(float(inCanvasWidth) / 8.f)), int(ceilf(float(inCanvasHeight) / 8.f)), 1);
		sInitPass->Build();
	}

	//node and cluster cull
	for (int i = 0; i < 4; i++) {
		char szName[128] = { 0 };
		sprintf_s(szName, "NodeAndClusterCull_%d", i);
		int inputWorkArgsIndex = i % 2;
		int outputWorkArgsIndex = (i + 1) % 2;
		sNodeAndClusterCullPass[i] = new RenderPass(ERenderPassType::ERPT_COMPUTE, szName);
		sNodeAndClusterCullPass[i]->SetSSBO(0, sBVHBuffer);
		sNodeAndClusterCullPass[i]->SetSSBO(1, sEchoBuffer, true);
		sNodeAndClusterCullPass[i]->SetSSBO(2, sMainAndPostNodeAndClusterBatches, true);
		sNodeAndClusterCullPass[i]->SetSSBO(3, sWorkArgsBuffer[inputWorkArgsIndex]);
		sNodeAndClusterCullPass[i]->SetSSBO(4, sWorkArgsBuffer[outputWorkArgsIndex], true);
		sNodeAndClusterCullPass[i]->SetUniformBufferObject(5, sGlobalConstantsBuffer);
		sNodeAndClusterCullPass[i]->SetSSBO(6, sNaniteMesh);
		sNodeAndClusterCullPass[i]->SetCS("Res/Shaders/NodeAndClusterCull.spv");
		sNodeAndClusterCullPass[i]->SetComputeDispatchArgs(1, 1, 1);
		sNodeAndClusterCullPass[i]->Build();
	}

	//ClusterCull Pass
	{
		sClusterCullPass = new RenderPass(ERenderPassType::ERPT_COMPUTE, "ClusterCull");
		sClusterCullPass->SetUniformBufferObject(0, sGlobalConstantsBuffer);
		sClusterCullPass->SetSSBO(1, sMainAndPostNodeAndClusterBatches);
		sClusterCullPass->SetSSBO(2, sVisiableClusterSHWH, true);
		sClusterCullPass->SetSSBO(3, sWorkArgsBuffer[0]);
		sClusterCullPass->SetSSBO(4, sNaniteMesh);
		sClusterCullPass->SetCS("Res/Shaders/ClusterCull.spv");
		sClusterCullPass->SetComputeDispatchArgs(1, 1, 1);
		sClusterCullPass->Build();
	}


	//HWRasterize Pass
	{
		sHWRasterizePass = new RenderPass(ERenderPassType::ERPT_GRAPHICS, "HWRasterize");
		sHWRasterizePass->SetUniformBufferObject(0, sGlobalConstantsBuffer);
		sHWRasterizePass->SetSSBO(1, sNaniteMesh);
		sHWRasterizePass->SetSSBO(2, sVisiableClusterSHWH);
		sHWRasterizePass->SetSSBO(3, sVisBuffer64, true);
		sHWRasterizePass->SetVSPS(
			"Res/Shaders/HWRasterizeVS.spv", 
			"Res/Shaders/HWRasterizeFS.spv");
		sHWRasterizePass->Build(inCanvasWidth, inCanvasHeight);
	}

	//Visualize Pass
	{
		sVisualizePass = new RenderPass(ERenderPassType::ERPT_COMPUTE, "Visualize");
		sVisualizePass->SetSSBO(0, sVisBuffer64);
		sVisualizePass->SetComputeImage(1, sVisualizationTexture, true);
		sVisualizePass->SetCS("Res/Shaders/Visualize.spv");
		sVisualizePass->SetComputeDispatchArgs(int(ceilf(float(inCanvasWidth) / 8.f)), int(ceilf(float(inCanvasHeight) / 8.f)), 1);
		sVisualizePass->Build();
	}

	{
		sFSQNode = new SceneNode;
		StaticMesh* staticMesh = new StaticMesh;
		staticMesh->SetVertexCount(4);

		staticMesh->SetPosition(0, -1.f, -1.f, 0.f, 1.f);
		staticMesh->SetTexcoord(0, 0.f, 0.f, 0.f, 0.f);

		staticMesh->SetPosition(1, 1.f, -1.f, 0.f, 1.f);
		staticMesh->SetTexcoord(1, 1.f, 0.f, 0.f, 0.f);

		staticMesh->SetPosition(2, -1.f, 1.f, 0.f, 1.f);
		staticMesh->SetTexcoord(2, 0.f, 1.f, 0.f, 0.f);

		staticMesh->SetPosition(3, 1.f, 1.f, 0.f, 1.f);
		staticMesh->SetTexcoord(3, 1.f, 1.f, 0.f, 0.f);

		staticMesh->mVBO = GenBufferObject(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, staticMesh->mVertexData, sizeof(StaticMeshVertexData) * 4);
		sFSQNode->mStaticMesh = staticMesh;
		staticMesh->mMaterial.Init("Res/Shaders/SwapChainVS.spv","Res/Shaders/SwapChainFS.spv");
		staticMesh->mMaterial.mPrimitiveType = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		staticMesh->mMaterial.SetTexture2D(2, 0,sVisualizationTexture->mImageView, GenSampler());
	}
}

void RenderOneFrame(float inFrameTimeInSecond) {
	float speed = 400.0f;
	float dt = inFrameTimeInSecond;

	float worldUp[3] = { 0,1,0 };

	// Right = normalize(cross(ViewDir, Up))
	float right[3] = {
		sGlobalConstantsData.mViewDirectionWS[1] * worldUp[2] -
		sGlobalConstantsData.mViewDirectionWS[2] * worldUp[1],

		sGlobalConstantsData.mViewDirectionWS[2] * worldUp[0] -
		sGlobalConstantsData.mViewDirectionWS[0] * worldUp[2],

		sGlobalConstantsData.mViewDirectionWS[0] * worldUp[1] -
		sGlobalConstantsData.mViewDirectionWS[1] * worldUp[0],
	};

	float len = sqrtf(right[0] * right[0] + right[1] * right[1] + right[2] * right[2]);
	right[0] /= len; right[1] /= len; right[2] /= len;

	if (GetAsyncKeyState('W'))
		for (int i = 0; i < 3; i++)
			sGlobalConstantsData.mCameraPositionWS[i] += sGlobalConstantsData.mViewDirectionWS[i] * speed * dt;

	if (GetAsyncKeyState('S'))
		for (int i = 0; i < 3; i++)
			sGlobalConstantsData.mCameraPositionWS[i] -= sGlobalConstantsData.mViewDirectionWS[i] * speed * dt;

	if (GetAsyncKeyState('A'))
		for (int i = 0; i < 3; i++)
			sGlobalConstantsData.mCameraPositionWS[i] -= right[i] * speed * dt;

	if (GetAsyncKeyState('D'))
		for (int i = 0; i < 3; i++)
			sGlobalConstantsData.mCameraPositionWS[i] += right[i] * speed * dt;

	BufferSubData(sGlobalConstantsBuffer, &sGlobalConstantsData, sizeof(GlobalConstants));
	sInitPass->Execute();
	for (int i = 0; i < 4; i++) {
		sNodeAndClusterCullPass[i]->Execute();
	}
	sClusterCullPass->Execute();
	sHWRasterizePass->ExecuteIndirect(sWorkArgsBuffer[0]);
	sVisualizePass->Execute();
	VkCommandBuffer commandBuffer = CreateCommandBuffer();
	BeginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	{
		static bool bIsConverted = false;
		if (bIsConverted == false)
		{
			bIsConverted = true;
			VkImageSubresourceRange imageSubresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT ,0,1,0,1 };
			TransferImageLayout(commandBuffer, sVisualizationTexture->mImage, imageSubresourceRange,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_NONE,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		}

		SCOPED_EVENT(commandBuffer, "SwapChain");
		BeginSwapChainRenderPass(commandBuffer);
		//ShaderParameterDescription* parameterDescription = GetUberPassShaderParameterDescription();
		sFSQNode->Draw(commandBuffer, GetSwapChainRenderPass(), sProjectionMatrix, sViewMatrix);
	}
	EndSwapChainRenderPass(commandBuffer);
}

void OnKeyUp(unsigned int inKeyCode)
{
	if (inKeyCode == VK_UP)
	{
		printf("up arrow\n");
		sCurrentMipLevelIndex++;
		if (sCurrentMipLevelIndex == _countof(sAvaliableMipLevels))
		{
			sCurrentMipLevelIndex = 0;
		}
	}
	else if (inKeyCode == VK_DOWN)
	{
		printf("down arrow\n");
		sCurrentMipLevelIndex--;
		if (sCurrentMipLevelIndex < 0)
		{
			sCurrentMipLevelIndex = _countof(sAvaliableMipLevels) - 1;
		}
	}
	sGlobalConstantsData.SetMisc0(sAvaliableMipLevels[sCurrentMipLevelIndex], 0, 0, 0);
}

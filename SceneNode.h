#pragma once
#include "matrix4.h"
#include "StaticMesh.h"
class SceneNode{
public:
	float4 mPosition;
	float4 mRotation;
	float4 mScale;
	bool mbNeedUpdate;
	matrix4 mModelMatrix;
	matrix4 mNormalMatrix;
	StaticMesh* mStaticMesh;
	Buffer* mUBO;
	Buffer* mUBO1;
	bool mbGeneratedDrawCommand;
	VkCommandBuffer* mCachedDrawCommand;
	SceneNode();
	void SetPosition(float inX, float inY, float inZ);
	void SetRotation(float inX, float inY, float inZ);
	void SetScale(float inX, float inY, float inZ);
	void Draw(VkCommandBuffer inCommandBuffer,VkRenderPass inRenderPass, matrix4& inProjectionMatrix, matrix4& inViewMatrix);
	void GenerateDrawCommand(matrix4&inProjectionMatrix,matrix4&inViewMatrix);
};


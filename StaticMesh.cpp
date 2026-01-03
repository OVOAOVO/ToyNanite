#include "StaticMesh.h"

std::vector<VkVertexInputBindingDescription> StaticMesh::mVertexInputBindingDescriptions;
std::vector<VkVertexInputAttributeDescription> StaticMesh::mVertexInputAttributeDescriptions;
void StaticMesh::Init() {
	mVertexInputBindingDescriptions.resize(1);
	mVertexInputBindingDescriptions[0] = {};
	mVertexInputBindingDescriptions[0].binding = 0;//0 slot ->
	mVertexInputBindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	mVertexInputBindingDescriptions[0].stride = 4 * 4 * sizeof(float);

	mVertexInputAttributeDescriptions.resize(4);
	mVertexInputAttributeDescriptions[0].binding = 0;
	mVertexInputAttributeDescriptions[0].location = 0;
	mVertexInputAttributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;//vec4
	mVertexInputAttributeDescriptions[0].offset = 0;

	mVertexInputAttributeDescriptions[1].binding = 0;
	mVertexInputAttributeDescriptions[1].location = 1;
	mVertexInputAttributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;//vec4
	mVertexInputAttributeDescriptions[1].offset = sizeof(float) * 4;

	mVertexInputAttributeDescriptions[2].binding = 0;
	mVertexInputAttributeDescriptions[2].location = 2;
	mVertexInputAttributeDescriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;//vec4
	mVertexInputAttributeDescriptions[2].offset = sizeof(float) * 8;

	mVertexInputAttributeDescriptions[3].binding = 0;//slot
	mVertexInputAttributeDescriptions[3].location = 3;
	mVertexInputAttributeDescriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;//vec4
	mVertexInputAttributeDescriptions[3].offset = sizeof(float) * 12;
}

void StaticMesh::SetVertexCount(int inVertexCount) {
	mVertexCount = inVertexCount;
	mVertexData = new StaticMeshVertexData[inVertexCount];
}
void StaticMesh::SetPosition(int inIndex, float inX, float inY, float inZ, float inW ) {
	mVertexData[inIndex].mPosition.x = inX;
	mVertexData[inIndex].mPosition.y = inY;
	mVertexData[inIndex].mPosition.z = inZ;
	mVertexData[inIndex].mPosition.w = inW;
}
void StaticMesh::SetTexcoord(int inIndex, float inX, float inY, float inZ, float inW) {
	mVertexData[inIndex].mTexcoord.x = inX;
	mVertexData[inIndex].mTexcoord.y = inY;
	mVertexData[inIndex].mTexcoord.z = inZ;
	mVertexData[inIndex].mTexcoord.w = inW;
}
void StaticMesh::SetNormal(int inIndex, float inX, float inY, float inZ, float inW) {
	mVertexData[inIndex].mNormal.x = inX;
	mVertexData[inIndex].mNormal.y = inY;
	mVertexData[inIndex].mNormal.z = inZ;
	mVertexData[inIndex].mNormal.w = inW;
}
void StaticMesh::SetTangent(int inIndex, float inX, float inY, float inZ, float inW) {
	mVertexData[inIndex].mTangent.x = inX;
	mVertexData[inIndex].mTangent.y = inY;
	mVertexData[inIndex].mTangent.z = inZ;
	mVertexData[inIndex].mTangent.w = inW;
}
void StaticMesh::InitFromFile(const char* inFilePath) {
	FILE* pFile = nullptr;
	fopen_s(&pFile,inFilePath, "rb");
	if (pFile != nullptr) {
		int temp = 0;
		fread(&temp, 4, 1, pFile);
		mVertexCount = temp;
		mVertexData = new StaticMeshVertexData[temp];
		fread(mVertexData, 1, sizeof(StaticMeshVertexData) * temp, pFile);
		mVBO = GenBufferObject(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			mVertexData, sizeof(StaticMeshVertexData) * mVertexCount);
		while (!feof(pFile)) {
			fread(&temp, 4, 1, pFile);
			if (feof(pFile)) {
				break;
			}
			char name[256] = {0};
			fread(name, 1, temp, pFile);
			fread(&temp, 4, 1, pFile);
			SubMesh* submesh = new SubMesh;
			submesh->mIndexCount = temp;
			submesh->mIndexes = new unsigned int[temp];
			fread(submesh->mIndexes, 1, sizeof(unsigned int) * temp, pFile);
			submesh->mIBO = GenBufferObject(VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, submesh->mIndexes,
				sizeof(unsigned int) * submesh->mIndexCount);
			mSubMeshes.insert(std::pair<std::string, SubMesh*>(name, submesh));
		}
		fclose(pFile);
	}
}
void StaticMesh::Draw(VkCommandBuffer inCommandBuffer) {
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(inCommandBuffer, 0, 1, &mVBO->mBuffer, offsets);
	if (false == mSubMeshes.empty()) {
		auto iter = mSubMeshes.begin();
		auto iterEnd = mSubMeshes.end();
		while (iter!=iterEnd)
		{
			SubMesh* submesh = iter->second;
			vkCmdBindIndexBuffer(inCommandBuffer, submesh->mIBO->mBuffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(inCommandBuffer, submesh->mIndexCount, 1, 0, 0, 0);
			iter++;
		}
	}
	else {
		vkCmdDraw(inCommandBuffer, mVertexCount, 1, 0, 0);
	}
}
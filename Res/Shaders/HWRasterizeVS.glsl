#version 450
layout(binding=0)uniform GlobalConstants {
	mat4 mProjectionMatrix;
	mat4 mViewMatrix;//View => translate
	mat4 mModelMatrix;
	uvec4 mMisc0;//0xFFFFFFFFu
	vec4 mNanite_ViewOrigin;//x,y,z,w => lodScale
	vec4 mNanite_ViewForward;//x,y,z,w => lodScaleHW
}U_GlobalConstants;
layout(std430,binding=1)readonly buffer FClusterPageData{
    uint mData[];
}ClusterPageData;
layout(std430,binding=2)readonly buffer FVisibleClusterSHWH{
    uint mData[];
}VisibleClusterSHWH;
layout(location=0)flat out uvec4 V_PackedData;
struct ClusterInfo{
	uint mBaseOffset;
	uint mIndexOffsetLocal;
	uint mIndexCount;
	vec4 mLODBounds;
	float mLODError;
	float mEdgeLength;
};
ClusterInfo GetClusterInfo(uint inPageIndex,uint inClusterIndex){
	uint pageCount=ClusterPageData.mData[0];
	uint pageBaseOffsetInBytes=ClusterPageData.mData[1u+inPageIndex];
	uint pageBaseOffset=pageBaseOffsetInBytes/4;
	uint clusterCountOnPage=ClusterPageData.mData[pageBaseOffset];
	uint clusterBaseOffsetInBytesLocal=ClusterPageData.mData[pageBaseOffset+1u+inClusterIndex];
	uint clusterBaseOffset=pageBaseOffset+1u+clusterCountOnPage+clusterBaseOffsetInBytesLocal/4;
	uint clusterIndexDataOffsetLocal=ClusterPageData.mData[clusterBaseOffset]/4;
	uint clusterIndexCount=ClusterPageData.mData[clusterBaseOffset+1u];
	uvec4 lodBounds=uvec4(
		ClusterPageData.mData[clusterBaseOffset+2u],
		ClusterPageData.mData[clusterBaseOffset+3u],
		ClusterPageData.mData[clusterBaseOffset+4u],
		ClusterPageData.mData[clusterBaseOffset+5u]
	);
	uint lodErrorAndEdgeLength=ClusterPageData.mData[clusterBaseOffset+6u];
	vec2 unpacked2Half          = unpackHalf2x16(lodErrorAndEdgeLength);

	ClusterInfo clusterInfo;
	clusterInfo.mBaseOffset=clusterBaseOffset;
	clusterInfo.mIndexOffsetLocal=clusterIndexDataOffsetLocal;
	clusterInfo.mIndexCount=clusterIndexCount;
	clusterInfo.mLODBounds=uintBitsToFloat(lodBounds);
	clusterInfo.mLODError = unpacked2Half.x;
	clusterInfo.mEdgeLength = unpacked2Half.y;

	return clusterInfo;
} 
void main(){
	uint clusterIndex=gl_InstanceIndex;
	uint vertexIndex=gl_VertexIndex;
	uint pageIndex=VisibleClusterSHWH.mData[clusterIndex*2];
	uint clusterIndexOnPage=VisibleClusterSHWH.mData[clusterIndex*2+1];

	ClusterInfo clusterInfo=GetClusterInfo(pageIndex,clusterIndexOnPage);
	uint currentVertexIndexOffsetBase=clusterInfo.mBaseOffset+clusterInfo.mIndexOffsetLocal;
	uint currentVertexIndexOffset=currentVertexIndexOffsetBase+vertexIndex;
	uint currentIndexInCluster=ClusterPageData.mData[currentVertexIndexOffset];
	uint currentClusterPositionDataOffsetBase=clusterInfo.mBaseOffset+7u;
	uint currentVertexPositionDataOffset=currentClusterPositionDataOffsetBase+currentIndexInCluster*3u;

	vec3 positionMS=uintBitsToFloat(
		uvec3(
			ClusterPageData.mData[currentVertexPositionDataOffset],
			ClusterPageData.mData[currentVertexPositionDataOffset+1],
			ClusterPageData.mData[currentVertexPositionDataOffset+2]
		)
	);
	vec4 positionCS=vec4(0.0f,0.0f,0.0f,0.0f);
	if(vertexIndex < clusterInfo.mIndexCount){
		vec4 positionWS=U_GlobalConstants.mModelMatrix*vec4(positionMS,1.0f);
		positionWS=vec4(positionWS.xyz-U_GlobalConstants.mNanite_ViewOrigin.xyz,1.0f);
		vec4 positionVS=U_GlobalConstants.mViewMatrix*positionWS;
		positionCS=U_GlobalConstants.mProjectionMatrix*positionVS;
		V_PackedData.x=(pageIndex << 8) | (clusterIndexOnPage+1);
	}
    gl_Position=positionCS;
}
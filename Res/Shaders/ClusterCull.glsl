#version 450
layout(local_size_x=1,local_size_y=1,local_size_z=1)in;

layout(binding=0)uniform GlobalConstants {
	mat4 mProjectionMatrix;
	mat4 mViewMatrix;//View => translate
	mat4 mModelMatrix;
	uvec4 mMisc0;//0xFFFFFFFFu
	vec4 mNanite_ViewOrigin;//x,y,z,w => lodScale
	vec4 mNanite_ViewForward;//x,y,z,w => lodScaleHW
}U_GlobalConstants;
layout(std430,binding=1)buffer FMainAndPostNodeAndClusterBatches{
    uint mData[];
}MainAndPostNodeAndClusterBatches;//21
layout(std430,binding=2)buffer FVisibleClusterSHWH{
    uint mData[];
}VisibleClusterSHWH;
layout(std430,binding=3)buffer FWorkArgs0{
    uint mData[];
}WorkArgs0;
layout(std430,binding=4)readonly buffer FClusterPageData{
    uint mData[];
}ClusterPageData;
struct ClusterInfo{
	uint mBaseOffset;
	uint mIndexOffsetLocal;
	uint mIndexCount;
	vec4 mLODBounds;
	float mLODError;
	float mEdgeLength;
};
ClusterInfo GetClusterInfo(uint inPageIndex,uint inClusterIndex){
	ClusterInfo clusterInfo;
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
	clusterInfo.mBaseOffset=clusterBaseOffset;
	clusterInfo.mIndexOffsetLocal=clusterIndexDataOffsetLocal;
	clusterInfo.mIndexCount=clusterIndexCount;
	clusterInfo.mLODBounds=uintBitsToFloat(lodBounds);
	vec2 unpacked2Half          = unpackHalf2x16(lodErrorAndEdgeLength);
	clusterInfo.mLODError = unpacked2Half.x;
	clusterInfo.mEdgeLength = unpacked2Half.y;
	return clusterInfo;
}
vec2 GetProjectedEdgeScales(vec4 Bounds)
{
	if(U_GlobalConstants.mProjectionMatrix[3][3] >= 1.0f )
	{
		return vec2(1.0f, 1.0f);
	}
	vec3 Center = Bounds.xyz;
	float Radius = Bounds.w;

	float ZNear = 10.0f;
	float DistToClusterSq = dot(Center,Center);	// camera origin in (0,0,0)
	
	float Z = dot(U_GlobalConstants.mNanite_ViewForward.xyz, Center);
	float XSq = DistToClusterSq - Z * Z;
	float X = sqrt( max(0.0f, XSq) );
	float DistToTSq = DistToClusterSq - Radius * Radius;
	float DistToT = sqrt( max(0.0f, DistToTSq) );
	float ScaledCosTheta = DistToT;
	float ScaledSinTheta = Radius;
	float ScaleToUnit = 1.0f/ DistToClusterSq;
	float By = (  ScaledSinTheta * X + ScaledCosTheta * Z ) * ScaleToUnit;
	float Ty = ( -ScaledSinTheta * X + ScaledCosTheta * Z ) * ScaleToUnit;
	
	float MinZ = max( Z - Radius, ZNear );
	float MaxZ = max( Z + Radius, ZNear );
	float MinCosAngle = Ty;
	float MaxCosAngle = By;

	if(Z + Radius > ZNear)
		return vec2( MinZ * MinCosAngle, MaxZ * MaxCosAngle );
	else
		return vec2( 0.0f, 0.0f );
}
void main(){//
	uint clusterCount=WorkArgs0.mData[1];
	uint outputOffset=0u;
	for(int i=0;i<clusterCount;i++){
		uint pageIndex=MainAndPostNodeAndClusterBatches.mData[1024+i*2];
		uint clusterIndex=MainAndPostNodeAndClusterBatches.mData[1024+i*2+1];
		ClusterInfo clusterInfo=GetClusterInfo(pageIndex,clusterIndex);
		{
			vec4 boundingSphereWS=clusterInfo.mLODBounds;
			vec4 positionWS=U_GlobalConstants.mModelMatrix*vec4(boundingSphereWS.xyz,1.0f);
			boundingSphereWS.xyz=positionWS.xyz-U_GlobalConstants.mNanite_ViewOrigin.xyz;
			float lodScale=U_GlobalConstants.mNanite_ViewOrigin.w;
			float lodScaleHW=U_GlobalConstants.mNanite_ViewForward.w;
			float ProjectedEdgeScale = GetProjectedEdgeScales(boundingSphereWS).x;
			bool bVisible = ProjectedEdgeScale >  clusterInfo.mLODError * lodScale;
			if(bVisible){
				if(ProjectedEdgeScale < abs(clusterInfo.mEdgeLength) * lodScaleHW){
					//hw
				}else{
					//sw
				}
				VisibleClusterSHWH.mData[outputOffset*2]=pageIndex;
				VisibleClusterSHWH.mData[outputOffset*2+1]=clusterIndex;
				outputOffset++;
			}
		}
	}
	WorkArgs0.mData[1]=outputOffset;
}
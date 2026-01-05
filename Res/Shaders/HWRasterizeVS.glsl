#version 450
layout(binding=0)uniform GlobalConstants {
	mat4 mProjectionMatrix;
	mat4 mViewMatrix;//View => translate
	mat4 mModelMatrix;
	uvec4 mMisc0;//0xFFFFFFFFu
	vec4 mNanite_ViewOrigin;//x,y,z,w => lodScale
	vec4 mNanite_ViewForward;//x,y,z,w => lodScaleHW
};
layout(std430,binding=1)buffer FClusterPageData{
    uint mData[];
}ClusterPageData;
layout(std430,binding=2)buffer FVisibleClusterSHWH{
    uint mData[];
}VisibleClusterSHWH;

void main(){
    gl_Position=vec4(0.0f,0.0f,0.0f,0.0f);
}
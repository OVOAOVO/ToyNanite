#version 450
#extension GL_ARB_gpu_shader_int64 : enable

layout(local_size_x=8,local_size_y=8,local_size_z=1)in;

layout(std430,binding=0)buffer FWorkArgs0{
    uint mData[];
}WorkArgs0;
layout(std430,binding=1)buffer FWorkArgs1{
    uint mData[];
}WorkArgs1;
layout(std430,binding=2)buffer FMainAndPostNodeAndClusterBatches{
    uint mData[];
}MainAndPostNodeAndClusterBatches;
layout(std430,binding=3)buffer FVisBuffer64{
    uint64_t mData[];
}VisBuffer64;
void main(){
	ivec2 texcoord=ivec2(gl_GlobalInvocationID.xy);
	if(any(greaterThanEqual(texcoord,ivec2(1280,720)))){
		return ;
	}
	if(texcoord.x==0&&texcoord.y==0){
		WorkArgs0.mData[0]=384u;
		WorkArgs0.mData[1]=0u;
		WorkArgs0.mData[2]=0u;
		WorkArgs0.mData[3]=0u;
		WorkArgs0.mData[5]=0u;
		WorkArgs0.mData[6]=1u;
		WorkArgs1.mData[0]=384u;
		WorkArgs1.mData[1]=0u;
		WorkArgs1.mData[2]=0u;
		WorkArgs1.mData[3]=0u;
		WorkArgs1.mData[5]=0u;
		WorkArgs1.mData[6]=1u;
	}
	int pixelIndex=texcoord.y*1280+texcoord.x;
	VisBuffer64.mData[pixelIndex]=0xFFFFFFFF00000000ul;
}
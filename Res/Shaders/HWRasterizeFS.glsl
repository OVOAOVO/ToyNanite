#version 450
#extension GL_ARB_gpu_shader_int64 : enable
layout(std430,binding=3)buffer FVisBuffer64{
    uint64_t mData[];
}VisBuffer64;

void main(){
    ivec2 texcoord=ivec2(gl_FragCoord.xy);
    int pixelIndex=texcoord.y*1280+texcoord.x;
    VisBuffer64.mData[pixelIndex]=0;
}
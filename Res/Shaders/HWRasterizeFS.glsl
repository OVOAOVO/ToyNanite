#version 450
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_EXT_shader_atomic_int64 : enable
layout(std430,binding=3)buffer FVisBuffer64{
    uint64_t mData[];
}VisBuffer64;
layout(location=0)flat in uvec4 V_PackedData;
void main(){
    ivec2 texcoord=ivec2(gl_FragCoord.xy);
    float z=gl_FragCoord.z;
    uint64_t pixelDepth=floatBitsToUint(z);
    uint64_t pixelValue=V_PackedData.x;
    uint64_t outputPixel=(pixelDepth<<32)|pixelValue;
    int pixelIndex=texcoord.y*1280+texcoord.x;
    atomicMin(VisBuffer64.mData[pixelIndex],outputPixel);
}
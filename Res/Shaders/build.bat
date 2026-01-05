glslc -fshader-stage=compute -o Init.spv .\Init.glsl
glslc -fshader-stage=compute -o NodeAndClusterCull.spv .\NodeAndClusterCull.glsl
glslc -fshader-stage=vertex -o SwapChainVS.spv .\SwapChainVS.glsl
glslc -fshader-stage=fragment -o SwapChainFS.spv .\SwapChainFS.glsl
glslc -fshader-stage=compute -o Visualize.spv .\Visualize.glsl
glslc -fshader-stage=compute -o ClusterCull.spv .\ClusterCull.glsl
glslc -fshader-stage=vertex -o HWRasterizeVS.spv .\HWRasterizeVS.glsl
glslc -fshader-stage=fragment -o HWRasterizeFS.spv .\HWRasterizeFS.glsl
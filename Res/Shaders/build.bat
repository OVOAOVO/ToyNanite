glslc -fshader-stage=compute -o Init.spv .\Init.glsl
glslc -fshader-stage=compute -o NodeAndClusterCull.spv .\NodeAndClusterCull.glsl
glslc -fshader-stage=vertex -o SwapChainVS.spv .\SwapChainVS.glsl
glslc -fshader-stage=fragment -o SwapChainFS.spv .\SwapChainFS.glsl
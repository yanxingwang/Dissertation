
#ifndef PER_FRAME_CONSTANTS_HLSL
#define PER_FRAME_CONSTANTS_HLSL

cbuffer PerFrameConstants : register(b0)
{
    float4x4	mCameraWorldViewProj;
    float4x4	mCameraWorldView;
    float4x4	mCameraViewProj;
    float4x4	mCameraProj;
    float4		mCameraNearFar;
    uint4		mFramebufferDimensions;
};

#endif
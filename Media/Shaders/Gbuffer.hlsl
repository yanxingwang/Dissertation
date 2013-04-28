
#ifndef GBUFFER_HLSL
#define GBUFFER_HLSL

#include "ShaderBase.hlsl"

// GBuffer and related common utilities and structures
struct GBuffer
{
    float4 normal_specular	: SV_Target0;
    float4 albedo			: SV_Target1;
    float2 positionZGrad	: SV_Target2;
};

float2 EncodeSphereMap(float3 n)
{
    float oneMinusZ = 1.0f - n.z;
    float p = sqrt(n.x * n.x + n.y * n.y + oneMinusZ * oneMinusZ);
    return n.xy / p * 0.5f + 0.5f;
}

//--------------------------------------------------------------------------------------
// G-buffer rendering
//--------------------------------------------------------------------------------------
void GBufferPS(GeometryVSOut input, out GBuffer outputGBuffer)
{
    SurfaceData surface = ComputeSurfaceDataFromGeometry(input);
    outputGBuffer.normal_specular = float4(EncodeSphereMap(surface.normal),
                                           surface.specularAmount,
                                           surface.specularPower);
    outputGBuffer.albedo = surface.albedo;
    outputGBuffer.positionZGrad = float2(ddx_coarse(surface.positionView.z),
                                         ddy_coarse(surface.positionView.z));
}

void GBufferAlphaTestPS(GeometryVSOut input, out GBuffer outputGBuffer)
{
    GBufferPS(input, outputGBuffer);
    
    // Alpha test
    clip(outputGBuffer.albedo.a - 0.3f);

    // Always use face normal for alpha tested stuff since it's double-sided
    outputGBuffer.normal_specular.xy = EncodeSphereMap(normalize(ComputeFaceNormal(input.positionView)));
}

#endif
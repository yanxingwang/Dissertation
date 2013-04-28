
#ifndef SKYBOX_TONE_HLSL
#define SKYBOX_TONE_HLSL

#include "ShaderBase.hlsl"
#include "Gbuffer.hlsl"

//--------------------------------------------------------------------------------------
// Tone mapping, post processing, skybox, etc.
// Rendered using skybox geometry, hence the naming
//--------------------------------------------------------------------------------------
TextureCube<float4> gSkyboxTexture : register(t5);
Texture2DMS<float, MSAA_SAMPLES> gDepthTexture : register(t6);

// This is the regular multisampled lit texture
Texture2DMS<float4, MSAA_SAMPLES> gLitTexture : register(t7);
// Since compute shaders cannot write to multisampled UAVs, this texture is used by
// the CS paths. It stores each sample separately in rows (i.e. y).
StructuredBuffer<uint2> gLitTextureFlat : register(t8);

struct SkyboxVSOut
{
    float4 positionViewport : SV_Position;
    float3 skyboxCoord : skyboxCoord;
};

SkyboxVSOut SkyboxVS(GeometryVSIn input)
{
    SkyboxVSOut output;
    
    // NOTE: Don't translate skybox and make sure depth == 1 (no clipping)
    output.positionViewport = mul(float4(input.position, 0.0f), mCameraViewProj).xyww;
    output.skyboxCoord = input.position;
    
    return output;
}

float4 SkyboxPS(SkyboxVSOut input) : SV_Target0
{
    // Use the flattened MSAA lit buffer if provided
    uint2 dims;
    gLitTextureFlat.GetDimensions(dims.x, dims.y);
    bool useFlatLitBuffer = dims.x > 0;
    
    uint2 coords = input.positionViewport.xy;

    float3 lit = float3(0.0f, 0.0f, 0.0f);
    float skyboxSamples = 0.0f;
    #if MSAA_SAMPLES <= 1
    [unroll]
    #endif
    for (unsigned int sampleIndex = 0; sampleIndex < MSAA_SAMPLES; ++sampleIndex) {
        float depth = gDepthTexture.Load(coords, sampleIndex);

        // Check for skybox case (NOTE: complementary Z!)
        if (depth <= 0.0f) {
            ++skyboxSamples;
        } else {
            float3 sampleLit;
            [branch] if (useFlatLitBuffer) {
                sampleLit = UnpackRGBA16(gLitTextureFlat[GetFramebufferSampleAddress(coords, sampleIndex)]).xyz;
            } else {
                sampleLit = gLitTexture.Load(coords, sampleIndex).xyz;
            }

            // Tone map each sample separately (identity for now) and accumulate
            lit += sampleLit;
        }
    }

    // If necessary, add skybox contribution
    [branch] if (skyboxSamples > 0) {
        float3 skybox = gSkyboxTexture.Sample(gDiffuseSampler, input.skyboxCoord).xyz;
        // Tone map and accumulate
        lit += skyboxSamples * skybox;
    }

    // Resolve MSAA samples (simple box filter)
    return float4(lit * rcp(MSAA_SAMPLES), 1.0f);
}


#endif
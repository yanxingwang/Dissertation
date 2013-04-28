
#ifndef LIGHTING_HLSL
#define LIGHTING_HLSL

#include "ShaderBase.hlsl"
#include "Gbuffer.hlsl"

float3 DecodeSphereMap(float2 e)
{
    float2 tmp = e - e * e;
    float f = tmp.x + tmp.y;
    float m = sqrt(4.0f * f - 1.0f);
    
    float3 n;
    n.xy = m * (e * 4.0f - 2.0f);
    n.z  = 3.0f - 8.0f * f;
    return n;
}

// Above values PLUS depth buffer (last element)
Texture2DMS<float4, MSAA_SAMPLES> gGBufferTextures[4] : register(t0);
StructuredBuffer<PointLight> gLight : register(t5);

float3 ComputePositionViewFromZ(float2 positionScreen,
                                float viewSpaceZ)
{
    float2 screenSpaceRay = float2(positionScreen.x / mCameraProj._11,
                                   positionScreen.y / mCameraProj._22);
    
    float3 positionView;
    positionView.z = viewSpaceZ;
    // Solve the two projection equations
    positionView.xy = screenSpaceRay.xy * positionView.z;
    
    return positionView;
}

SurfaceData ComputeSurfaceDataFromGBufferSample(uint2 positionViewport, uint sampleIndex)
{
    // Load the raw data from the GBuffer
    GBuffer rawData;
    rawData.normal_specular = gGBufferTextures[0].Load(positionViewport.xy, sampleIndex).xyzw;
    rawData.albedo = gGBufferTextures[1].Load(positionViewport.xy, sampleIndex).xyzw;
    rawData.positionZGrad = gGBufferTextures[2].Load(positionViewport.xy, sampleIndex).xy;
    float zBuffer = gGBufferTextures[3].Load(positionViewport.xy, sampleIndex).x;
    
    float2 gbufferDim;
    uint dummy;
    gGBufferTextures[0].GetDimensions(gbufferDim.x, gbufferDim.y, dummy);
    
    // Compute screen/clip-space position and neighbour positions
    // NOTE: Mind DX11 viewport transform and pixel center!
    // NOTE: This offset can actually be precomputed on the CPU but it's actually slower to read it from
    // a constant buffer than to just recompute it.
    float2 screenPixelOffset = float2(2.0f, -2.0f) / gbufferDim;
    float2 positionScreen = (float2(positionViewport.xy) + 0.5f) * screenPixelOffset.xy + float2(-1.0f, 1.0f);
    float2 positionScreenX = positionScreen + float2(screenPixelOffset.x, 0.0f);
    float2 positionScreenY = positionScreen + float2(0.0f, screenPixelOffset.y);
        
    // Decode into reasonable outputs
    SurfaceData data;
        
    // Unproject depth buffer Z value into view space
    float viewSpaceZ = mCameraProj._43 / (zBuffer - mCameraProj._33);

    data.positionView = ComputePositionViewFromZ(positionScreen, viewSpaceZ);
    data.positionViewDX = ComputePositionViewFromZ(positionScreenX, viewSpaceZ + rawData.positionZGrad.x) - data.positionView;
    data.positionViewDY = ComputePositionViewFromZ(positionScreenY, viewSpaceZ + rawData.positionZGrad.y) - data.positionView;

    data.normal = DecodeSphereMap(rawData.normal_specular.xy);
    data.albedo = rawData.albedo;

    data.specularAmount = rawData.normal_specular.z;
    data.specularPower = rawData.normal_specular.w;
    
    return data;
}

void ComputeSurfaceDataFromGBufferAllSamples(uint2 positionViewport,
                                             out SurfaceData surface[MSAA_SAMPLES])
{
    // Load data for each sample
    // Most of this time only a small amount of this data is actually used so unrolling
    // this loop ensures that the compiler has an easy time with the dead code elimination.
    [unroll] for (uint i = 0; i < MSAA_SAMPLES; ++i) {
        surface[i] = ComputeSurfaceDataFromGBufferSample(positionViewport, i);
    }
}

// As below, we separate this for diffuse/specular parts for convenience in deferred lighting
void AccumulatePhongBRDF(float3 normal,
                         float3 lightDir,
                         float3 viewDir,
                         float3 lightContrib,
                         float specularPower,
                         inout float3 litDiffuse,
                         inout float3 litSpecular)
{
    // Simple Phong
    float NdotL = dot(normal, lightDir);
    [flatten] if (NdotL > 0.0f) {
        float3 r = reflect(lightDir, normal);
        float RdotV = max(0.0f, dot(r, viewDir));
        float specular = pow(RdotV, specularPower);

        litDiffuse += lightContrib * NdotL;
        litSpecular += lightContrib * specular;
    }
}

// Accumulates separate "diffuse" and "specular" components of lighting from a given
// This is not possible for all BRDFs but it works for our simple Phong example here
// and this separation is convenient for deferred lighting.
// Uses an in-out for accumulation to avoid returning and accumulating 0
void AccumulateBRDFDiffuseSpecular(SurfaceData surface, PointLight light,
                                   inout float3 litDiffuse,
                                   inout float3 litSpecular)
{
    float3 directionToLight = light.positionView - surface.positionView;
    float distanceToLight = length(directionToLight);

    [branch] if (distanceToLight < light.attenuationEnd) {
        float attenuation = linstep(light.attenuationEnd, light.attenuationBegin, distanceToLight);
        directionToLight *= rcp(distanceToLight);       // A full normalize/RSQRT might be as fast here anyways...
        
        AccumulatePhongBRDF(surface.normal, directionToLight, normalize(surface.positionView),
            attenuation * light.color, surface.specularPower, litDiffuse, litSpecular);
    }
}

// Uses an in-out for accumulation to avoid returning and accumulating 0
void AccumulateBRDF(SurfaceData surface, PointLight light,
                    inout float3 lit)
{
    float3 directionToLight = light.positionView - surface.positionView;
    float distanceToLight = length(directionToLight);

    [branch] if (distanceToLight < light.attenuationEnd) {
        float attenuation = linstep(light.attenuationEnd, light.attenuationBegin, distanceToLight);
        directionToLight *= rcp(distanceToLight);       // A full normalize/RSQRT might be as fast here anyways...

        float3 litDiffuse = float3(0.0f, 0.0f, 0.0f);
        float3 litSpecular = float3(0.0f, 0.0f, 0.0f);
        AccumulatePhongBRDF(surface.normal, directionToLight, normalize(surface.positionView),
            attenuation * light.color, surface.specularPower, litDiffuse, litSpecular);
        
        lit += surface.albedo.rgb * (litDiffuse + surface.specularAmount * litSpecular);
    }
}

// Check if a given pixel can be shaded at pixel frequency (i.e. they all come from
// the same surface) or if they require per-sample shading
bool RequiresPerSampleShading(SurfaceData surface[MSAA_SAMPLES])
{
    bool perSample = false;

    const float maxZDelta = abs(surface[0].positionViewDX.z) + abs(surface[0].positionViewDY.z);
    const float minNormalDot = 0.99f;        // Allow ~8 degree normal deviations

    [unroll] for (uint i = 1; i < MSAA_SAMPLES; ++i) {
        // Using the position derivatives of the triangle, check if all of the sample depths
        // could possibly have come from the same triangle/surface
        perSample = perSample || 
            abs(surface[i].positionView.z - surface[0].positionView.z) > maxZDelta;

        // Also flag places where the normal is different
        perSample = perSample || 
            dot(surface[i].normal, surface[0].normal) < minNormalDot;
    }

    return perSample;
}

float4 BasicLoop(FullScreenTriangleVSOut input, uint sampleIndex)
{
    uint totalLights, dummy;
    gLight.GetDimensions(totalLights, dummy);
    
    float3 lit = float3(0.0f, 0.0f, 0.0f);
    
	SurfaceData surface = ComputeSurfaceDataFromGBufferSample(uint2(input.positionViewport.xy), sampleIndex);
	
	// Avoid shading skybox/background pixels
	if (surface.positionView.z < mCameraNearFar.y) {
	    for (uint lightIndex = 0; lightIndex < totalLights; ++lightIndex) {
	        PointLight light = gLight[lightIndex];
	        AccumulateBRDF(surface, light, lit);
	    }
	}

    return float4(lit, 1.0f);
}

float4 BasicLoopPS(FullScreenTriangleVSOut input) : SV_Target
{
    // Shade only sample 0
    return BasicLoop(input, 0);
} 

#endif
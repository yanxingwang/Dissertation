
#include "ShaderBase.hlsl"

float4 DiffusePS( GeometryVSOut input) : SV_Target
{
	return gDiffuseTexture.Sample(gDiffuseSampler, input.texCoord);
}
#pragma once

struct PointLight
{
	D3DXVECTOR3 positionView;
	float attenuationBegin;
	D3DXVECTOR3 color;
	float attenuationEnd;
};

struct PointLightInitTransform
{
	float radius;
	float angle;
	float height;
	float animationSpeed;
};

// Flat framebuffer RGBA16-encoded
struct FramebufferFlatElement
{
	unsigned int rb;
	unsigned int ga;
};


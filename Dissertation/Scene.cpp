#include "DXUT.h"
#include "Scene.h"
#include "../Media/Shaders/Defines.h"

Scene::Scene(ID3D11Device* pDevice):
	mLightBuffer(NULL),
	mTotalTime(0),
	mSkyboxSRV(NULL)
{
	mMeshOpaque.Create(pDevice, L"..\\media\\Sponza\\sponza_dds.sdkmesh");
	mMeshSkybox.Create(pDevice, L"..\\media\\Skybox\\Skybox.sdkmesh");

	// load skybox texture
	ID3D11Resource* resource = NULL;
	HRESULT hr;
	hr = D3DX11CreateTextureFromFile(pDevice, L"..\\media\\Skybox\\Clouds.dds", 0, 0, &resource, 0);
	assert(SUCCEEDED(hr));
	pDevice->CreateShaderResourceView(resource, 0, &mSkyboxSRV);
	resource->Release();
}

Scene::~Scene(void)
{
	mMeshOpaque.Destroy();
	SAFE_DELETE(mLightBuffer);
	SAFE_RELEASE(mSkyboxSRV);
}

void Scene::setActiveLights( ID3D11Device* d3dDevice, unsigned int activeLights )
{
	mActiveLights = activeLights;

	delete mLightBuffer;
	mLightBuffer = new StructuredBuffer<PointLight>(d3dDevice, activeLights, D3D11_BIND_SHADER_RESOURCE, true);

	// Make sure all the active lights are set up
	moveLights(0.0f);
}

void Scene::moveLights( float elapsedTime )
{
	mTotalTime += elapsedTime;

	// Update positions of active lights
	for (unsigned int i = 0; i < mActiveLights; ++i) {
		const PointLightInitTransform& initTransform = mLightInitialTransform[i];
		float angle = initTransform.angle + mTotalTime * initTransform.animationSpeed;
		mPointLightPositionWorld[i] = D3DXVECTOR3(
			initTransform.radius * std::cos(angle),
			initTransform.height,
			initTransform.radius * std::sin(angle));
	}
}

void Scene::initLightParameters( ID3D11Device* d3dDevice )
{
	mPointLightParameters.resize(MAX_LIGHTS);
	mLightInitialTransform.resize(MAX_LIGHTS);
	mPointLightPositionWorld.resize(MAX_LIGHTS);

	// Use a constant seed for consistency
	std::tr1::mt19937 rng(1337);

	std::tr1::uniform_real<float> radiusNormDist(0.0f, 0.6f);
	const float maxRadius = 90;
	std::tr1::uniform_real<float> angleDist(0.0f, 2.0f * D3DX_PI); 
	std::tr1::uniform_real<float> heightDist(0.0f, 20.0f);
	std::tr1::uniform_real<float> animationSpeedDist(2.0f, 20.0f);
	std::tr1::uniform_int<int> animationDirection(0, 1);
	std::tr1::uniform_real<float> hueDist(0.0f, 1.0f);
	std::tr1::uniform_real<float> intensityDist(0.1f, 0.5f);
	std::tr1::uniform_real<float> attenuationDist(25, 30);
	const float attenuationStartFactor = 0.8f;

	for (unsigned int i = 0; i < MAX_LIGHTS; ++i) {
		PointLight& params = mPointLightParameters[i];
		PointLightInitTransform& init = mLightInitialTransform[i];

		init.radius = std::sqrt(radiusNormDist(rng)) * maxRadius;
		init.angle = angleDist(rng);
		init.height = heightDist(rng);
		// Normalize by arc length
		init.animationSpeed = (animationDirection(rng) * 2 - 1) * animationSpeedDist(rng) / init.radius;

		// HSL->RGB, vary light hue
		params.color = intensityDist(rng) * D3DXVECTOR3(1,1,1);//HueToRGB(hueDist(rng));
		params.attenuationEnd = attenuationDist(rng);
		params.attenuationBegin = attenuationStartFactor * params.attenuationEnd;
	}
}

void Scene::initLights( ID3D11Device* d3dDevice )
{
	initLightParameters(d3dDevice);
	setActiveLights(d3dDevice, MAX_LIGHTS);
}

ID3D11ShaderResourceView* Scene::updateLights( ID3D11DeviceContext* d3dDeviceContext, const D3DXMATRIXA16& cameraView )
{
	// Transform light world positions into view space and store in our parameters array
	D3DXVec3TransformCoordArray(&mPointLightParameters[0].positionView, sizeof(PointLight),
		&mPointLightPositionWorld[0], sizeof(D3DXVECTOR3), &cameraView, mActiveLights);

	// Copy light list into shader buffer
	{
		PointLight* light = mLightBuffer->MapDiscard(d3dDeviceContext);
		for (unsigned int i = 0; i < mActiveLights; ++i)
			light[i] = mPointLightParameters[i];
		mLightBuffer->Unmap(d3dDeviceContext);
	}

	return mLightBuffer->GetShaderResource();
}

void Scene::preRender(D3DXMATRIXA16& worldViewProj)
{
	if (getOpaqueMesh().IsLoaded()) 
		getOpaqueMesh().ComputeInFrustumFlags(worldViewProj);

	if (getTransparentMesh().IsLoaded()) 
		getTransparentMesh().ComputeInFrustumFlags(worldViewProj);
}

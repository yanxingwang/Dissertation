
#include "SDKmesh.h"
#include "Light.h"
#include "Buffer.h"

#pragma once
class Scene
{
public:
	Scene(ID3D11Device* pDevice);
	~Scene(void);

public:

	CDXUTSDKMesh&				getOpaqueMesh() { return mMeshOpaque; }

	CDXUTSDKMesh&				getTransparentMesh() {return mMeshAlpha; }

	CDXUTSDKMesh&				getSkyboxMesh() {return mMeshSkybox; }

	void						initLights(ID3D11Device* d3dDevice);

	void						setActiveLights(ID3D11Device* d3dDevice, unsigned int activeLights);

	void						moveLights(float elapsedTime);

	ID3D11ShaderResourceView*	updateLights(ID3D11DeviceContext* d3dDeviceContext,
									const D3DXMATRIXA16& cameraView);

	ID3D11ShaderResourceView*	getSkyboxSRV() const { return mSkyboxSRV; }

	void						preRender(D3DXMATRIXA16& worldViewProj);

private:

	void						initLightParameters(ID3D11Device* d3dDevice);

	float						mTotalTime;

	CDXUTSDKMesh				mMeshOpaque;

	CDXUTSDKMesh				mMeshAlpha;

	CDXUTSDKMesh				mMeshSkybox;

	ID3D11ShaderResourceView*	mSkyboxSRV;

	// Lighting state
	unsigned int mActiveLights;
	vector<PointLightInitTransform> mLightInitialTransform;
	vector<PointLight> mPointLightParameters;
	vector<D3DXVECTOR3> mPointLightPositionWorld;

	StructuredBuffer<PointLight>* mLightBuffer;
};



#include "SDKmesh.h"

#pragma once
class Scene
{
public:
	Scene(ID3D11Device* pDevice);
	~Scene(void);

public:

	CDXUTSDKMesh&	getOpaqueMesh() { return mMeshOpaque; }

	CDXUTSDKMesh&	getTransparentMesh() {return mMeshAlpha; }

private:

	CDXUTSDKMesh mMeshOpaque;

	CDXUTSDKMesh mMeshAlpha;
};


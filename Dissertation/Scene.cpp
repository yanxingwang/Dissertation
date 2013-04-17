#include "DXUT.h"
#include "Scene.h"

Scene::Scene(ID3D11Device* pDevice)
{
	mMeshOpaque.Create(pDevice, L"..\\media\\Sponza\\sponza_dds.sdkmesh");
}

Scene::~Scene(void)
{
	mMeshOpaque.Destroy();
}

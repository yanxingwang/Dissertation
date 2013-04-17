#include "DXUT.h"
#include "RenderLoop.h"


RenderLoop::RenderLoop(ID3D11Device* pDevice)
	:mDevice(pDevice),
	mRenderScheme(NULL)
{
	init();
}


RenderLoop::~RenderLoop(void)
{
	delete mRenderScheme;
}

void RenderLoop::init()
{
	mScene = shared_ptr<Scene>(new Scene(mDevice));
}

void RenderLoop::render()
{

}

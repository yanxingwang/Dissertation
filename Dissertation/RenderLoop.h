#pragma once

#include "Scene.h"
#include "RenderScheme.h"
#include "DeferredRenderer.h"

class RenderLoop
{
public:
	RenderLoop(ID3D11Device* pDevice);

	~RenderLoop(void);

public:

	void	render();

private:

	void	init();

private:

	ID3D11Device*				mDevice;

	shared_ptr<Scene>			mScene;

	RenderScheme*				mRenderScheme;
};


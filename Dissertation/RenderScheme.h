#pragma once

class RenderLoop;

class RenderScheme
{
public:
	RenderScheme(RenderLoop* renderloop);

	~RenderScheme(void);

public:


public:

	virtual void	onResizedSwapChain(ID3D11Device* d3dDevice,
		const DXGI_SURFACE_DESC* backBufferDesc) = 0;

	virtual void	doRender(ID3D11DeviceContext* d3dDeviceContext,
		const D3D11_VIEWPORT* viewport) = 0;

protected:

	RenderLoop*		mRenderLoop;
};


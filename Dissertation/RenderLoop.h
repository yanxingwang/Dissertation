#pragma once

#include "Scene.h"
#include "RenderScheme.h"
#include "DeferredRenderer.h"
#include "Shader.h"
#include "DXUTcamera.h"
#include "Texture.h"

enum LightCullTechnique {
	CULL_DEFERRED_NONE,
	CULL_COMPUTE_SHADER_TILE,
};

class RenderLoop
{
public:
	RenderLoop(ID3D11Device* pDevice);

	~RenderLoop(void);

public:

	void								render(ID3D11DeviceContext* d3dDeviceContext,
											ID3D11RenderTargetView* backBuffer,
											const D3D11_VIEWPORT* viewport);

	CFirstPersonCamera*					getCamera() const { return mCamera; }

	void								setCamera(CFirstPersonCamera* val) { mCamera = val; }

	void								OnD3D11ResizedSwapChain(ID3D11Device* d3dDevice,
											const DXGI_SURFACE_DESC* backBufferDesc);

private:

	void								init();

	void								loadShaders();

	// Draws geometry into G-buffer
	void								renderGBuffer(ID3D11DeviceContext* d3dDeviceContext,
											const D3D11_VIEWPORT* viewport);

	void								renderLighting(ID3D11DeviceContext* d3dDeviceContext,
											ID3D11ShaderResourceView *lightBufferSRV,
											const D3D11_VIEWPORT* viewport);

	void								renderSkyboxToneMap(ID3D11DeviceContext* d3dDeviceContext,
											ID3D11RenderTargetView* backBuffer,
											ID3D11ShaderResourceView* skybox,
											ID3D11ShaderResourceView* depthSRV,
											const D3D11_VIEWPORT* viewport);

private:

	ID3D11Device*						mDevice;

	shared_ptr<Scene>					mScene;

	shared_ptr<Depth2D>					mDepthBuffer;

	ID3D11DepthStencilView*				mDepthBufferReadOnlyDSV;

	RenderScheme*						mRenderScheme;

	ID3D11Buffer*						mPerFrameConstants;

	ID3D11RasterizerState*				mRasterizerState;

	ID3D11RasterizerState*				mDoubleSidedRasterizerState;

	ID3D11SamplerState*					mDiffuseSampler;

	ID3D11BlendState*					mGeometryBlendState;

	ID3D11BlendState*					mLightingBlendState;

	unsigned int						mMSAASamples;

	VertexShader*						mGeometryVS;

	PixelShader*						mDiffusePS;

	ID3D11InputLayout*					mMeshVertexLayout;

	CFirstPersonCamera*					mCamera;

	D3DXMATRIXA16						mWorldMatrix;

	vector<shared_ptr<Texture2D>>		mGBuffer;

	vector<ID3D11RenderTargetView*>		mGBufferRTV;

	vector<ID3D11ShaderResourceView*>	mGBufferSRV;

	unsigned int						mGBufferWidth;

	unsigned int						mGBufferHeight;

	ID3D11DepthStencilState*			mDepthState;

	ID3D11DepthStencilState*			mWriteStencilState;

	ID3D11DepthStencilState*			mEqualStencilState;

	PixelShader*						mGBufferPS;

	PixelShader*						mGBufferAlphaTestPS;

	VertexShader*						mFullScreenTriangleVS;

	shared_ptr<Texture2D>				mLitBufferPS;

	shared_ptr<StructuredBuffer<FramebufferFlatElement> > mLitBufferCS;

	PixelShader*						mBasicLoopPS;

	VertexShader*						mSkyboxVS;

	PixelShader*						mSkyboxPS;

	ComputeShader*						mTileCS;

	LightCullTechnique					mLightTech;
};


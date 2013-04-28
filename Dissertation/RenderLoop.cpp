#include "DXUT.h"
#include "RenderLoop.h"
#include "../Media/Shaders/Defines.h"

__declspec(align(16))
struct PerFrameConstants
{
	D3DXMATRIX mCameraWorldViewProj;
	D3DXMATRIX mCameraWorldView;
	D3DXMATRIX mCameraViewProj;
	D3DXMATRIX mCameraProj;
	D3DXVECTOR4 mCameraNearFar;

	unsigned int mFramebufferDimensionsX;
	unsigned int mFramebufferDimensionsY;
	unsigned int mFramebufferDimensionsZ;
	unsigned int mFramebufferDimensionsW;
};

RenderLoop::RenderLoop(ID3D11Device* pDevice)
	:mDevice(pDevice),
	mRenderScheme(NULL),
	mPerFrameConstants(NULL),
	mRasterizerState(NULL),
	mDoubleSidedRasterizerState(NULL),
	mDiffuseSampler(NULL),
	mMSAASamples(1),
	mDiffusePS(NULL),
	mMeshVertexLayout(NULL),
	mCamera(NULL),
	mGBufferWidth(0),
	mGBufferHeight(0),
	mGeometryVS(NULL),
	mDepthBufferReadOnlyDSV(NULL),
	mDepthState(NULL),
	mWriteStencilState(NULL),
	mEqualStencilState(NULL),
	mGBufferPS(NULL),
	mGBufferAlphaTestPS(NULL),
	mGeometryBlendState(NULL),
	mLightingBlendState(NULL),
	mFullScreenTriangleVS(NULL),
	mBasicLoopPS(NULL),
	mSkyboxVS(NULL),
	mSkyboxPS(NULL),
	mTileCS(NULL),
	mLightTech(CULL_COMPUTE_SHADER_TILE)
{
	D3DXMatrixIdentity(&mWorldMatrix);
	D3DXMatrixScaling(&mWorldMatrix,0.1f,0.1f,0.1f);
	init();
}

RenderLoop::~RenderLoop(void)
{
	SAFE_DELETE(mRenderScheme);

	SAFE_RELEASE(mPerFrameConstants);
	SAFE_RELEASE(mRasterizerState);
	SAFE_RELEASE(mDoubleSidedRasterizerState);
	SAFE_RELEASE(mDiffuseSampler);
	SAFE_RELEASE(mMeshVertexLayout);
	SAFE_RELEASE(mDepthBufferReadOnlyDSV);
	SAFE_RELEASE(mEqualStencilState);
	SAFE_RELEASE(mWriteStencilState);
	SAFE_RELEASE(mDepthState);
	SAFE_RELEASE(mLightingBlendState);
	SAFE_RELEASE(mGeometryBlendState);

	SAFE_DELETE(mDiffusePS);
	SAFE_DELETE(mGeometryVS);
	SAFE_DELETE(mGBufferAlphaTestPS);
	SAFE_DELETE(mGBufferPS);
	SAFE_DELETE(mFullScreenTriangleVS);
	SAFE_DELETE(mBasicLoopPS);
	SAFE_DELETE(mSkyboxVS);
	SAFE_DELETE(mSkyboxPS);
	SAFE_DELETE(mTileCS);
}

void RenderLoop::init()
{
	mScene = shared_ptr<Scene>(new Scene(mDevice));
	mScene->initLights(mDevice);

	// Create standard rasterizer state
	{
		CD3D11_RASTERIZER_DESC desc(D3D11_DEFAULT);
		mDevice->CreateRasterizerState(&desc, &mRasterizerState);

		desc.CullMode = D3D11_CULL_NONE;
		mDevice->CreateRasterizerState(&desc, &mDoubleSidedRasterizerState);
	}

	// Create constant buffers
	{
		CD3D11_BUFFER_DESC desc(
			sizeof(PerFrameConstants),
			D3D11_BIND_CONSTANT_BUFFER,
			D3D11_USAGE_DYNAMIC,
			D3D11_CPU_ACCESS_WRITE);

		mDevice->CreateBuffer(&desc, 0, &mPerFrameConstants);
	}

	// Create sampler state
	{
		CD3D11_SAMPLER_DESC desc(D3D11_DEFAULT);
		desc.Filter = D3D11_FILTER_ANISOTROPIC;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.MaxAnisotropy = 16;
		mDevice->CreateSamplerState(&desc, &mDiffuseSampler);
	}

	{
		CD3D11_DEPTH_STENCIL_DESC desc(D3D11_DEFAULT);
		// NOTE: Complementary Z => GREATER test
		desc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
		mDevice->CreateDepthStencilState(&desc, &mDepthState);
	}

	// Stencil states for MSAA
	{
		CD3D11_DEPTH_STENCIL_DESC desc(
			FALSE, D3D11_DEPTH_WRITE_MASK_ZERO, D3D11_COMPARISON_GREATER_EQUAL,   // Depth
			TRUE, 0xFF, 0xFF,                                                     // Stencil
			D3D11_STENCIL_OP_REPLACE, D3D11_STENCIL_OP_REPLACE, D3D11_STENCIL_OP_REPLACE, D3D11_COMPARISON_ALWAYS, // Front face stencil
			D3D11_STENCIL_OP_REPLACE, D3D11_STENCIL_OP_REPLACE, D3D11_STENCIL_OP_REPLACE, D3D11_COMPARISON_ALWAYS  // Back face stencil
			);
		mDevice->CreateDepthStencilState(&desc, &mWriteStencilState);
	}
	{
		CD3D11_DEPTH_STENCIL_DESC desc(
			TRUE, D3D11_DEPTH_WRITE_MASK_ZERO, D3D11_COMPARISON_GREATER_EQUAL,    // Depth
			TRUE, 0xFF, 0xFF,                                                     // Stencil
			D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_EQUAL, // Front face stencil
			D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_EQUAL  // Back face stencil
			);
		mDevice->CreateDepthStencilState(&desc, &mEqualStencilState);
	}

	// Create geometry phase blend state
	{
		CD3D11_BLEND_DESC desc(D3D11_DEFAULT);
		mDevice->CreateBlendState(&desc, &mGeometryBlendState);
	}

	// Create lighting phase blend state
	{
		CD3D11_BLEND_DESC desc(D3D11_DEFAULT);
		// Additive blending
		desc.RenderTarget[0].BlendEnable = true;
		desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		mDevice->CreateBlendState(&desc, &mLightingBlendState);
	}

	// load shaders
	loadShaders();
}

void RenderLoop::render(ID3D11DeviceContext* d3dDeviceContext,
						ID3D11RenderTargetView* backBuffer,
						const D3D11_VIEWPORT* viewport)
{
	//mScene->getOpaqueMesh().Render(mDevice);

	D3DXMATRIXA16 cameraProj = *mCamera->GetProjMatrix();
	D3DXMATRIXA16 cameraView = *mCamera->GetViewMatrix();

	D3DXMATRIXA16 cameraViewInv;
	D3DXMatrixInverse(&cameraViewInv, 0, &cameraView);

	// Compute composite matrices
	D3DXMATRIXA16 cameraViewProj = cameraView * cameraProj;
	D3DXMATRIXA16 cameraWorldViewProj = mWorldMatrix * cameraViewProj;

	// Fill in frame constants
	{
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		d3dDeviceContext->Map(mPerFrameConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		PerFrameConstants* constants = static_cast<PerFrameConstants*>(mappedResource.pData);

		constants->mCameraWorldViewProj = cameraWorldViewProj;
		constants->mCameraWorldView = mWorldMatrix * cameraView;
		constants->mCameraViewProj = cameraViewProj;
		constants->mCameraProj = cameraProj;
		// NOTE: Complementary Z => swap near/far back
		constants->mCameraNearFar = D3DXVECTOR4(mCamera->GetFarClip(), mCamera->GetNearClip(), 0.0f, 0.0f);

		constants->mFramebufferDimensionsX = mGBufferWidth;
		constants->mFramebufferDimensionsY = mGBufferHeight;
		constants->mFramebufferDimensionsZ = 0;     // Unused
		constants->mFramebufferDimensionsW = 0;     // Unused

		d3dDeviceContext->Unmap(mPerFrameConstants, 0);
	}

	mScene->preRender(cameraWorldViewProj);

	renderGBuffer(d3dDeviceContext,viewport);

	ID3D11ShaderResourceView *lightBufferSRV = mScene->updateLights(d3dDeviceContext, cameraView);
	renderLighting(d3dDeviceContext,lightBufferSRV,viewport);

	renderSkyboxToneMap(d3dDeviceContext,backBuffer,
		mScene->getSkyboxSRV(),mDepthBuffer->GetShaderResource(), viewport);
}

void RenderLoop::loadShaders()
{
	std::string msaaSamplesStr;
	{
		std::ostringstream oss;
		oss << mMSAASamples;
		msaaSamplesStr = oss.str();
	}

	// Set up macros
	D3D10_SHADER_MACRO defines[] =
	{
		{"MSAA_SAMPLES", msaaSamplesStr.c_str()},
		{0, 0}
	};

	mDiffusePS = new PixelShader(mDevice, L"../Media/Shaders/diffuse.hlsl", "DiffusePS", defines);
	mGeometryVS = new VertexShader(mDevice, L"../Media/Shaders/ShaderBase.hlsl", "GeometryVS", defines);
	mGBufferPS = new PixelShader(mDevice, L"../Media/Shaders/GBuffer.hlsl", "GBufferPS", defines);
	mGBufferAlphaTestPS = new PixelShader(mDevice, L"../Media/Shaders/GBuffer.hlsl", "GBufferAlphaTestPS", defines);
	mFullScreenTriangleVS = new VertexShader(mDevice, L"../Media/Shaders/ShaderBase.hlsl", "FullScreenTriangleVS", defines);
	mBasicLoopPS = new PixelShader(mDevice, L"../Media/Shaders/Lighting.hlsl", "BasicLoopPS", defines);
	mSkyboxVS = new VertexShader(mDevice, L"../Media/Shaders/SkyboxToneMap.hlsl", "SkyboxVS", defines);
	mSkyboxPS = new PixelShader(mDevice, L"../Media/Shaders/SkyboxToneMap.hlsl", "SkyboxPS", defines);
	mTileCS = new ComputeShader(mDevice, L"../Media/Shaders/Tile.hlsl", "ComputeShaderTileCS", defines);

	// Create input layout
	{
		// We need the vertex shader bytecode for this... rather than try to wire that all through the
		// shader interface, just recompile the vertex shader.
		UINT shaderFlags = D3D10_SHADER_ENABLE_STRICTNESS | D3D10_SHADER_PACK_MATRIX_ROW_MAJOR;
		ID3D10Blob *bytecode = 0;
		HRESULT hr = D3DX11CompileFromFile(L"../Media/Shaders/ShaderBase.hlsl", defines, 0, "GeometryVS", "vs_5_0", 
			shaderFlags, 0, 0, &bytecode, 0, 0);
		if (FAILED(hr)) {
			assert(false);      // It worked earlier...
		}

		const D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{"position",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"normal",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"texCoord",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
		};

		mDevice->CreateInputLayout( 
			layout, ARRAYSIZE(layout), 
			bytecode->GetBufferPointer(),
			bytecode->GetBufferSize(), 
			&mMeshVertexLayout);

		bytecode->Release();
	}
}

void RenderLoop::OnD3D11ResizedSwapChain( ID3D11Device* d3dDevice, 
										 const DXGI_SURFACE_DESC* backBufferDesc )
{
	// clean the GBuffer
	mGBuffer.resize(0);
	mGBufferRTV.resize(0);
	mGBufferSRV.resize(0);

	mGBufferWidth = backBufferDesc->Width;
	mGBufferHeight = backBufferDesc->Height;

	mDepthBuffer = nullptr;
	SAFE_RELEASE(mDepthBufferReadOnlyDSV);

	DXGI_SAMPLE_DESC sampleDesc;
	sampleDesc.Count = mMSAASamples;
	sampleDesc.Quality = 0;

	// Depth-Buffer
	mDepthBuffer = shared_ptr<Depth2D>(new Depth2D(
		d3dDevice, mGBufferWidth, mGBufferHeight,
		D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE,
		sampleDesc,
		mMSAASamples > 1    // Include stencil if using MSAA
		));

	// read-only depth stencil view
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC desc;
		mDepthBuffer->GetDepthStencil()->GetDesc(&desc);
		desc.Flags = D3D11_DSV_READ_ONLY_DEPTH;

		d3dDevice->CreateDepthStencilView(mDepthBuffer->GetTexture(), &desc, &mDepthBufferReadOnlyDSV);
	}

	// G-Buffer
	// normal_specular
	mGBuffer.push_back(shared_ptr<Texture2D>(new Texture2D(
		d3dDevice, mGBufferWidth, mGBufferHeight, DXGI_FORMAT_R16G16B16A16_FLOAT,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
		sampleDesc)));

	// albedo
	mGBuffer.push_back(shared_ptr<Texture2D>(new Texture2D(
		d3dDevice, mGBufferWidth, mGBufferHeight, DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
		sampleDesc)));

	// positionZgrad
	mGBuffer.push_back(shared_ptr<Texture2D>(new Texture2D(
		d3dDevice, mGBufferWidth, mGBufferHeight, DXGI_FORMAT_R16G16_FLOAT,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
		sampleDesc)));

	// Set up GBuffer resource list
	mGBufferRTV.resize(mGBuffer.size(), 0);
	mGBufferSRV.resize(mGBuffer.size() + 1, 0);
	for (std::size_t i = 0; i < mGBuffer.size(); ++i) {
		mGBufferRTV[i] = mGBuffer[i]->GetRenderTarget();
		mGBufferSRV[i] = mGBuffer[i]->GetShaderResource();
	}
	// Depth buffer is the last SRV that we use for reading
	mGBufferSRV.back() = mDepthBuffer->GetShaderResource();

	// lit buffers
	mLitBufferPS = shared_ptr<Texture2D>(new Texture2D(
		d3dDevice, mGBufferWidth, mGBufferHeight, DXGI_FORMAT_R16G16B16A16_FLOAT,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
		sampleDesc));

	mLitBufferCS = shared_ptr< StructuredBuffer<FramebufferFlatElement> >(new StructuredBuffer<FramebufferFlatElement>(
		d3dDevice, mGBufferWidth * mGBufferHeight * mMSAASamples,
		D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE));
}

void RenderLoop::renderGBuffer( ID3D11DeviceContext* d3dDeviceContext, const D3D11_VIEWPORT* viewport )
{
	// Clear GBuffer
    // NOTE: We actually only need to clear the depth buffer here since we replace unwritten (i.e. far plane) samples
    // with the skybox. We use the depth buffer to reconstruct position and only in-frustum positions are shaded.
    // NOTE: Complementary Z buffer: clear to 0 (far)!
    d3dDeviceContext->ClearDepthStencilView(mDepthBuffer->GetDepthStencil(),
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);

    d3dDeviceContext->IASetInputLayout(mMeshVertexLayout);

    d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->VSSetShader(mGeometryVS->GetShader(), 0, 0);
    
    d3dDeviceContext->GSSetShader(0, 0, 0);

    d3dDeviceContext->RSSetViewports(1, viewport);

    d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
    d3dDeviceContext->PSSetSamplers(0, 1, &mDiffuseSampler);
    // Diffuse texture set per-material by DXUT mesh routines

    // Set up render GBuffer render targets
    d3dDeviceContext->OMSetDepthStencilState(mDepthState, 0);
    d3dDeviceContext->OMSetRenderTargets(static_cast<UINT>(mGBufferRTV.size()), 
		&mGBufferRTV.front(), mDepthBuffer->GetDepthStencil());
    d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);
    
    // Render opaque geometry
    if (mScene->getOpaqueMesh().IsLoaded()) {
        d3dDeviceContext->RSSetState(mRasterizerState);
        d3dDeviceContext->PSSetShader(mGBufferPS->GetShader(), 0, 0);
        mScene->getOpaqueMesh().Render(d3dDeviceContext, 0);
    }

    // Render alpha tested geometry
    if (mScene->getTransparentMesh().IsLoaded()) {
        d3dDeviceContext->RSSetState(mDoubleSidedRasterizerState);
        d3dDeviceContext->PSSetShader(mGBufferAlphaTestPS->GetShader(), 0, 0);
        mScene->getTransparentMesh().Render(d3dDeviceContext, 0);
    }

    // Cleanup (aka make the runtime happy)
    d3dDeviceContext->OMSetRenderTargets(0, 0, 0);
}

void RenderLoop::renderLighting( ID3D11DeviceContext* d3dDeviceContext, 
								 ID3D11ShaderResourceView *lightBufferSRV, 
								 const D3D11_VIEWPORT* viewport )
{
	// Clear
	const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	d3dDeviceContext->ClearRenderTargetView(mLitBufferPS->GetRenderTarget(), zeros);

	// Full screen triangle setup
	d3dDeviceContext->IASetInputLayout(0);
	d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	d3dDeviceContext->IASetVertexBuffers(0, 0, 0, 0, 0);

	d3dDeviceContext->VSSetShader(mFullScreenTriangleVS->GetShader(), 0, 0);
	d3dDeviceContext->GSSetShader(0, 0, 0);

	d3dDeviceContext->RSSetState(mRasterizerState);
	d3dDeviceContext->RSSetViewports(1, viewport);

	d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
	d3dDeviceContext->PSSetShaderResources(0, static_cast<UINT>(mGBufferSRV.size()), &mGBufferSRV.front());
	d3dDeviceContext->PSSetShaderResources(5, 1, &lightBufferSRV);

	if(mLightTech == CULL_COMPUTE_SHADER_TILE)
	{
		d3dDeviceContext->CSSetConstantBuffers(0, 1, &mPerFrameConstants);
		d3dDeviceContext->CSSetShaderResources(0, static_cast<UINT>(mGBufferSRV.size()), &mGBufferSRV.front());
		d3dDeviceContext->CSSetShaderResources(5, 1, &lightBufferSRV);

		ID3D11UnorderedAccessView *litBufferUAV = mLitBufferCS->GetUnorderedAccess();
		d3dDeviceContext->CSSetUnorderedAccessViews(0, 1, &litBufferUAV, 0);
		d3dDeviceContext->CSSetShader(mTileCS->GetShader(), 0, 0);

		// Dispatch
		unsigned int dispatchWidth = (mGBufferWidth + COMPUTE_SHADER_TILE_GROUP_DIM - 1) / COMPUTE_SHADER_TILE_GROUP_DIM;
		unsigned int dispatchHeight = (mGBufferHeight + COMPUTE_SHADER_TILE_GROUP_DIM - 1) / COMPUTE_SHADER_TILE_GROUP_DIM;
		d3dDeviceContext->Dispatch(dispatchWidth, dispatchHeight, 1);
	}
	else if(mLightTech == CULL_DEFERRED_NONE)
	{
		if (mMSAASamples > 1) {
			// Set stencil mask for samples that require per-sample shading
			// 		d3dDeviceContext->PSSetShader(mRequiresPerSampleShadingPS->GetShader(), 0, 0);
			// 		d3dDeviceContext->OMSetDepthStencilState(mWriteStencilState, 1);
			// 		d3dDeviceContext->OMSetRenderTargets(0, 0, mDepthBufferReadOnlyDSV);
			// 		d3dDeviceContext->Draw(3, 0);
		}

		// Additively blend into back buffer
		ID3D11RenderTargetView * renderTargets[1] = {mLitBufferPS->GetRenderTarget()};
		d3dDeviceContext->OMSetRenderTargets(1, renderTargets, mDepthBufferReadOnlyDSV);
		d3dDeviceContext->OMSetBlendState(mLightingBlendState, 0, 0xFFFFFFFF);

		// Do pixel frequency shading
		d3dDeviceContext->PSSetShader(mBasicLoopPS->GetShader(), 0, 0);
		d3dDeviceContext->OMSetDepthStencilState(mEqualStencilState, 0);
		d3dDeviceContext->Draw(3, 0);

		if (mMSAASamples > 1) {
			// Do sample frequency shading
			// 		d3dDeviceContext->PSSetShader(mBasicLoopPerSamplePS->GetShader(), 0, 0);
			// 		d3dDeviceContext->OMSetDepthStencilState(mEqualStencilState, 1);
			// 		d3dDeviceContext->Draw(3, 0);
		}
	}

	// Cleanup
	d3dDeviceContext->VSSetShader(0, 0, 0);
	d3dDeviceContext->GSSetShader(0, 0, 0);
	d3dDeviceContext->PSSetShader(0, 0, 0);
	d3dDeviceContext->OMSetRenderTargets(0, 0, 0);
	ID3D11ShaderResourceView* nullSRV[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	d3dDeviceContext->VSSetShaderResources(0, 8, nullSRV);
	d3dDeviceContext->PSSetShaderResources(0, 8, nullSRV);
	d3dDeviceContext->CSSetShaderResources(0, 8, nullSRV);
	ID3D11UnorderedAccessView *nullUAV[1] = {0};
	d3dDeviceContext->CSSetUnorderedAccessViews(0, 1, nullUAV, 0);
}

void RenderLoop::renderSkyboxToneMap( ID3D11DeviceContext* d3dDeviceContext, 
									 ID3D11RenderTargetView* backBuffer, ID3D11ShaderResourceView* skybox, 
									 ID3D11ShaderResourceView* depthSRV, const D3D11_VIEWPORT* viewport )
{
	D3D11_VIEWPORT skyboxViewport(*viewport);
	skyboxViewport.MinDepth = 1.0f;
	skyboxViewport.MaxDepth = 1.0f;

	d3dDeviceContext->IASetInputLayout(mMeshVertexLayout);

	d3dDeviceContext->VSSetConstantBuffers(0, 1, &mPerFrameConstants);
	d3dDeviceContext->VSSetShader(mSkyboxVS->GetShader(), 0, 0);

	d3dDeviceContext->RSSetState(mDoubleSidedRasterizerState);
	d3dDeviceContext->RSSetViewports(1, &skyboxViewport);

	d3dDeviceContext->PSSetConstantBuffers(0, 1, &mPerFrameConstants);
	d3dDeviceContext->PSSetSamplers(0, 1, &mDiffuseSampler);
	d3dDeviceContext->PSSetShader(mSkyboxPS->GetShader(), 0, 0);

	d3dDeviceContext->PSSetShaderResources(5, 1, &skybox);
	d3dDeviceContext->PSSetShaderResources(6, 1, &depthSRV);

	ID3D11ShaderResourceView* litViews[2] = {0, 0};

	switch (mLightTech) {
	case CULL_COMPUTE_SHADER_TILE:
		litViews[1] = mLitBufferCS->GetShaderResource();
		break;
	default:
		litViews[0] = mLitBufferPS->GetShaderResource();
		break;
	}

	d3dDeviceContext->PSSetShaderResources(7, 2, litViews);

	d3dDeviceContext->OMSetRenderTargets(1, &backBuffer, 0);
	d3dDeviceContext->OMSetBlendState(mGeometryBlendState, 0, 0xFFFFFFFF);

	mScene->getSkyboxMesh().Render(d3dDeviceContext);

	d3dDeviceContext->OMSetRenderTargets(0, 0, 0);
	ID3D11ShaderResourceView* nullViews[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	d3dDeviceContext->PSSetShaderResources(0, 10, nullViews);
}

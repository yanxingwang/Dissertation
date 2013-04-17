
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"
#include <sstream>

#include "RenderLoop.h"

RenderLoop*	gRenderLoop = NULL;

CFirstPersonCamera gViewerCamera;

D3DXMATRIXA16 gWorldMatrix;

// DXUT GUI stuff
CDXUTDialogResourceManager gDialogResourceManager;
CD3DSettingsDlg gD3DSettingsDlg;
//CDXUTDialog gHUD[HUD_NUM];
CDXUTCheckBox* gAnimateLightCheck = 0;
CDXUTComboBox* gMSAACombo = 0;
CDXUTComboBox* gSceneSelectCombo = 0;
CDXUTComboBox* gCullTechniqueCombo = 0;
CDXUTSlider* gLightsSlider = 0;
CDXUTTextHelper* gTextHelper = 0;

float gAspectRatio;
bool gDisplayUI = true;
bool gZeroNextFrameTime = true;

bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* deviceSettings, void* userContext);
void CALLBACK OnFrameMove(double time, float elapsedTime, void* userContext);
LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* noFurtherProcessing,
						 void* userContext);
void CALLBACK OnKeyboard(UINT character, bool keyDown, bool altDown, void* userContext);
void CALLBACK OnGUIEvent(UINT eventID, INT controlID, CDXUTControl* control, void* userContext);
HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* d3dDevice, const DXGI_SURFACE_DESC* backBufferSurfaceDesc,
									 void* userContext);
HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* d3dDevice, IDXGISwapChain* swapChain,
										 const DXGI_SURFACE_DESC* backBufferSurfaceDesc, void* userContext);
void CALLBACK OnD3D11ReleasingSwapChain(void* userContext);
void CALLBACK OnD3D11DestroyDevice(void* userContext);
void CALLBACK OnD3D11FrameRender(ID3D11Device* d3dDevice, ID3D11DeviceContext* d3dDeviceContext, double time,
								 float elapsedTime, void* userContext);

void InitScene(ID3D11Device* d3dDevice);
void DestroyScene();

void InitUI();
void UpdateUIState();


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, INT nCmdShow)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	DXUTSetCallbackDeviceChanging(ModifyDeviceSettings);
	DXUTSetCallbackMsgProc(MsgProc);
	DXUTSetCallbackKeyboard(OnKeyboard);
	DXUTSetCallbackFrameMove(OnFrameMove);

	DXUTSetCallbackD3D11DeviceCreated(OnD3D11CreateDevice);
	DXUTSetCallbackD3D11SwapChainResized(OnD3D11ResizedSwapChain);
	DXUTSetCallbackD3D11FrameRender(OnD3D11FrameRender);
	DXUTSetCallbackD3D11SwapChainReleasing(OnD3D11ReleasingSwapChain);
	DXUTSetCallbackD3D11DeviceDestroyed(OnD3D11DestroyDevice);

	DXUTSetIsInGammaCorrectMode(true);

	DXUTInit(true, true, 0);
	InitUI();

	DXUTSetCursorSettings(true, true);
	DXUTSetHotkeyHandling(true, true, false);
	DXUTCreateWindow(L"Deferred Shading");
	DXUTCreateDevice(D3D_FEATURE_LEVEL_11_0, true, 1280, 720);
	DXUTMainLoop();

	return DXUTGetExitCode();
}


void InitUI()
{
	// Setup default UI state
	// NOTE: All of these are made directly available in the shader constant buffer
	// This is convenient for development purposes.

	gD3DSettingsDlg.Init(&gDialogResourceManager);

	int width = 200;

	UpdateUIState();
}


void UpdateUIState()
{
	//int technique = PtrToUint(gCullTechniqueCombo->GetSelectedData());
}


void InitScene(ID3D11Device* d3dDevice)
{
	DestroyScene();

	D3DXVECTOR3 cameraEye(0.0f, 0.0f, 0.0f);
	D3DXVECTOR3 cameraAt(0.0f, 0.0f, 0.0f);
	float sceneScaling = 1.0f;
	D3DXVECTOR3 sceneTranslation(0.0f, 0.0f, 0.0f);
	bool zAxisUp = false;

	D3DXMatrixScaling(&gWorldMatrix, sceneScaling, sceneScaling, sceneScaling);
	if (zAxisUp) {
		D3DXMATRIXA16 m;
		D3DXMatrixRotationX(&m, -D3DX_PI / 2.0f);
		gWorldMatrix *= m;
	}
	{
		D3DXMATRIXA16 t;
		D3DXMatrixTranslation(&t, sceneTranslation.x, sceneTranslation.y, sceneTranslation.z);
		gWorldMatrix *= t;
	}

	gViewerCamera.SetViewParams(&cameraEye, &cameraAt);
	gViewerCamera.SetScalers(0.01f, 10.0f);
	gViewerCamera.FrameMove(0.0f);

	// Zero out the elapsed time for the next frame
	gZeroNextFrameTime = true;

	gRenderLoop = new RenderLoop(d3dDevice);
}

void DestroyScene()
{
	delete gRenderLoop;
	gRenderLoop = NULL;
}


bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* deviceSettings, void* userContext)
{
	// For the first device created if its a REF device, optionally display a warning dialog box
	static bool firstTime = true;
	if (firstTime) {
		firstTime = false;
		if (deviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE) {
			DXUTDisplaySwitchingToREFWarning(deviceSettings->ver);
		}
	}

	// We don't currently support framebuffer MSAA
	// Requires multi-frequency shading wrt. the GBuffer that is not yet implemented
	deviceSettings->d3d11.sd.SampleDesc.Count = 1;
	deviceSettings->d3d11.sd.SampleDesc.Quality = 0;

	// Also don't need a depth/stencil buffer... we'll manage that ourselves
	deviceSettings->d3d11.AutoCreateDepthStencil = false;

	return true;
}


void CALLBACK OnFrameMove(double time, float elapsedTime, void* userContext)
{
	if (gZeroNextFrameTime) {
		elapsedTime = 0.0f;
	}

	// Update the camera's position based on user input
	gViewerCamera.FrameMove(elapsedTime);
}


LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* noFurtherProcessing,
						 void* userContext)
{
	// Pass messages to dialog resource manager calls so GUI state is updated correctly
	*noFurtherProcessing = gDialogResourceManager.MsgProc(hWnd, uMsg, wParam, lParam );
	if (*noFurtherProcessing) {
		return 0;
	}

	// Pass messages to settings dialog if its active
	if (gD3DSettingsDlg.IsActive()) {
		gD3DSettingsDlg.MsgProc(hWnd, uMsg, wParam, lParam);
		return 0;
	}

	// Pass all remaining windows messages to camera so it can respond to user input
	gViewerCamera.HandleMessages(hWnd, uMsg, wParam, lParam);

	return 0;
}


void CALLBACK OnKeyboard(UINT character, bool keyDown, bool altDown, void* userContext)
{
	if(keyDown) {
		switch (character) {
		case VK_F9:
			// Toggle display of UI on/off
			gDisplayUI = !gDisplayUI;
			break;
		}
	}
}


void CALLBACK OnGUIEvent(UINT eventID, INT controlID, CDXUTControl* control, void* userContext)
{
	UpdateUIState();
}


void CALLBACK OnD3D11DestroyDevice(void* userContext)
{
	DestroyScene();

	gDialogResourceManager.OnD3D11DestroyDevice();
	gD3DSettingsDlg.OnD3D11DestroyDevice();
	DXUTGetGlobalResourceCache().OnDestroyDevice();
	SAFE_DELETE(gTextHelper);
}


HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* d3dDevice, const DXGI_SURFACE_DESC* backBufferSurfaceDesc,
									 void* userContext)
{    
	ID3D11DeviceContext* d3dDeviceContext = DXUTGetD3D11DeviceContext();
	gDialogResourceManager.OnD3D11CreateDevice(d3dDevice, d3dDeviceContext);
	gD3DSettingsDlg.OnD3D11CreateDevice(d3dDevice);
	gTextHelper = new CDXUTTextHelper(d3dDevice, d3dDeviceContext, &gDialogResourceManager, 15);

	gViewerCamera.SetRotateButtons(true, false, false);
	gViewerCamera.SetDrag(true);
	gViewerCamera.SetEnableYAxisMovement(true);

	InitScene(d3dDevice);

	return S_OK;
}


HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* d3dDevice, IDXGISwapChain* swapChain,
										 const DXGI_SURFACE_DESC* backBufferSurfaceDesc, void* userContext)
{
	HRESULT hr;

	V_RETURN(gDialogResourceManager.OnD3D11ResizedSwapChain(d3dDevice, backBufferSurfaceDesc));
	V_RETURN(gD3DSettingsDlg.OnD3D11ResizedSwapChain(d3dDevice, backBufferSurfaceDesc));

	gAspectRatio = backBufferSurfaceDesc->Width / (float)backBufferSurfaceDesc->Height;

	// NOTE: Complementary Z (1-z) buffer used here, so swap near/far!
	gViewerCamera.SetProjParams(D3DX_PI / 4.0f, gAspectRatio, 300.0f, 0.05f);

	return S_OK;
}


void CALLBACK OnD3D11ReleasingSwapChain(void* userContext)
{
	gDialogResourceManager.OnD3D11ReleasingSwapChain();
}


void CALLBACK OnD3D11FrameRender(ID3D11Device* d3dDevice, ID3D11DeviceContext* d3dDeviceContext, double time,
								 float elapsedTime, void* userContext)
{
	if (gZeroNextFrameTime) {
		elapsedTime = 0.0f;
	}
	gZeroNextFrameTime = false;

	if (gD3DSettingsDlg.IsActive()) {
		gD3DSettingsDlg.OnRender(elapsedTime);
		return;
	}

	ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
	
	// do render
	gRenderLoop->render();

	D3D11_VIEWPORT viewport;
	viewport.Width    = static_cast<float>(DXUTGetDXGIBackBufferSurfaceDesc()->Width);
	viewport.Height   = static_cast<float>(DXUTGetDXGIBackBufferSurfaceDesc()->Height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;

	if (gDisplayUI) {
	}
}

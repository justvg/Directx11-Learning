#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dinput.h>
#include <DirectXColors.h>

#include "math.hpp"

#include <WICTextureLoader.h>
#include <DDSTextureLoader.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

#include <stdio.h>

#define Assert(Expression) if((!Expression)) { *(int *)0 = 0; }
#define ArrayCount(Array) (sizeof(Array)/sizeof((Array)[0]))

struct d3d_app
{
	ID3D11Device *Device;
	ID3D11DeviceContext *ImmediateContext;
	IDXGISwapChain *SwapChain;
	ID3D11Texture2D *DepthStencilBuffer;
	ID3D11RenderTargetView *RenderTargetView;
	ID3D11DepthStencilView *DepthStencilView;

	UINT WindowWidth, WindowHeight;
};

global_variable bool GlobalRunning;
global_variable bool GlobalWindowIsFocused;
global_variable d3d_app GlobalDirect3D;
global_variable LARGE_INTEGER GlobalPerfCounterFrequency;

inline LARGE_INTEGER
GetWallClock(void)
{
	LARGE_INTEGER Result;
	QueryPerformanceCounter(&Result);
	return(Result);
}

inline real32 
GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
	real32 Result = (End.QuadPart - Start.QuadPart) / (real32)GlobalPerfCounterFrequency.QuadPart;
	return(Result);
}

LRESULT CALLBACK 
MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
	LRESULT Result = 0;

	switch (Message)
	{
		case WM_CLOSE:
		{
			GlobalRunning = false;
		} break;

		case WM_DESTROY:
		{
			GlobalRunning = false;
		} break;

		case WM_ACTIVATEAPP:
        {
            if(WParam)
            {
                ShowCursor(FALSE);
                GlobalWindowIsFocused = true;
            }
            else
            {
                ShowCursor(TRUE);
                GlobalWindowIsFocused = false;
            }
        } break;

		case WM_SIZE:
		{
			GlobalDirect3D.WindowWidth = LOWORD(LParam);
			GlobalDirect3D.WindowHeight = HIWORD(LParam);
		} break;

		case WM_GETMINMAXINFO:
		{
			((MINMAXINFO *)LParam)->ptMinTrackSize.x = 200;
			((MINMAXINFO *)LParam)->ptMinTrackSize.y = 200;
		} break;

		default:
		{
			Result = DefWindowProc(Window, Message, WParam, LParam);
		} break;
	}

	return(Result);
}

struct game_button_state
{
	uint32_t HalfTransitionCount;
	bool EndedDown;
};

struct game_input
{
	int32_t MouseX, MouseY;
	int32_t DeltaMouseX, DeltaMouseY;

	union
	{
		game_button_state Buttons[4];
		struct
		{
			game_button_state MoveForward;
			game_button_state MoveBack;
			game_button_state MoveLeft;
			game_button_state MoveRight;
		};
	};
};

internal void
ProcessKeyboardMessage(game_button_state *Button, bool IsDown)
{
	if(Button->EndedDown != IsDown)
	{
		Button->EndedDown = IsDown;
		Button->HalfTransitionCount++;
	}
}

internal void
ProcessPendingMessages(game_input *Input)
{
	MSG Message;
	while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
	{
		switch (Message.message)
		{
			case WM_QUIT:
			{
				GlobalRunning = false;
			} break;

			case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                uint32_t VKCode = (uint32_t) Message.wParam;

                bool WasDown = ((Message.lParam & (1 << 30)) != 0);
                bool IsDown = ((Message.lParam & (1 << 31)) == 0);
                
                if (WasDown != IsDown)
                { 
                    if (VKCode == 'W')
                    {
                        ProcessKeyboardMessage(&Input->MoveForward, IsDown);
                    }
                    else if (VKCode == 'A')
                    {
                        ProcessKeyboardMessage(&Input->MoveLeft, IsDown);
                    }
                    else if (VKCode == 'S')
                    {
                        ProcessKeyboardMessage(&Input->MoveBack, IsDown);
                    }
                    else if (VKCode == 'D')
                    {
                        ProcessKeyboardMessage(&Input->MoveRight, IsDown);
                    }
				}
			} break;

			case WM_INPUT:
			{
				RAWINPUT RawInput;
				UINT RawInputSize = sizeof(RawInput);
				GetRawInputData((HRAWINPUT)Message.lParam, RID_INPUT, &RawInput, &RawInputSize, sizeof(RAWINPUTHEADER));

				if(RawInput.header.dwType == RIM_TYPEMOUSE)
				{
					if(RawInput.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)
					{
						Input->DeltaMouseX = RawInput.data.mouse.lLastX - Input->MouseX;
						Input->DeltaMouseY = RawInput.data.mouse.lLastY - Input->MouseY;

						Input->MouseX = RawInput.data.mouse.lLastX;
						Input->MouseY = RawInput.data.mouse.lLastY;
					}
					else if(RawInput.data.mouse.usFlags == MOUSE_MOVE_RELATIVE)
					{
						Input->DeltaMouseX = RawInput.data.mouse.lLastX;
						Input->DeltaMouseY = RawInput.data.mouse.lLastY;

						Input->MouseX += Input->DeltaMouseX;
						Input->MouseY += Input->DeltaMouseY;
					}
				}
			} break;

			default:
			{
				TranslateMessage(&Message);
				DispatchMessage(&Message);
			} break;
		}
	}
}

struct matrix_buffer
{
	mat4 Projection;
	mat4 View;
	mat4 Model;
};

struct vertex
{
	v3 Pos;
	v3 Normal;
	v2 TexCoords;
	v3 Tangent;
	v3 Bitangent;
};

int CALLBACK
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
	QueryPerformanceFrequency(&GlobalPerfCounterFrequency);

	d3d_app *Direct3D = &GlobalDirect3D;
	Direct3D->WindowWidth = 960;
	Direct3D->WindowHeight = 540;

	WNDCLASS WindowClass = {};
	WindowClass.style = CS_VREDRAW | CS_HREDRAW;
	WindowClass.lpfnWndProc = MainWindowCallback;
	WindowClass.hInstance = Instance;
	WindowClass.lpszClassName = "D11 WindowClass";

	if(RegisterClass(&WindowClass))
	{
		HWND Window = CreateWindowEx(0, WindowClass.lpszClassName, "D11",
									 WS_OVERLAPPEDWINDOW | WS_VISIBLE,
									 CW_USEDEFAULT, CW_USEDEFAULT, Direct3D->WindowWidth, Direct3D->WindowHeight,
									 0, 0, Instance, 0);

		if (Window)
		{
			UINT CreateDeviceFlags = 0;
#if DEBUG | _DEBUG
			CreateDeviceFlags = D3D11_CREATE_DEVICE_DEBUG;
#endif

			D3D_FEATURE_LEVEL FeatureLevel;
			HRESULT Hr = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0,
										   CreateDeviceFlags, 0, 0, D3D11_SDK_VERSION,
										   &Direct3D->Device, &FeatureLevel, &Direct3D->ImmediateContext);

			if (FAILED(Hr))
			{
				MessageBoxW(0, L"D3D11CreateDevice failed", 0, 0);
			}

			if (FeatureLevel != D3D_FEATURE_LEVEL_11_0)
			{
				MessageBoxW(0, L"Durect3D Feature Level 11 is unsupported", 0, 0);
			}

			DXGI_SWAP_CHAIN_DESC SwapChainDescription;
			SwapChainDescription.BufferDesc.Width = Direct3D->WindowWidth;
			SwapChainDescription.BufferDesc.Height = Direct3D->WindowHeight;
			SwapChainDescription.BufferDesc.RefreshRate.Numerator = 60;
			SwapChainDescription.BufferDesc.RefreshRate.Denominator = 1;
			SwapChainDescription.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			SwapChainDescription.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
			SwapChainDescription.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
			SwapChainDescription.SampleDesc.Count = 1;
			SwapChainDescription.SampleDesc.Quality = 0;
			SwapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			SwapChainDescription.BufferCount = 1;
			SwapChainDescription.OutputWindow = Window;
			SwapChainDescription.Windowed = true;
			SwapChainDescription.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
			SwapChainDescription.Flags = 0;

			IDXGIDevice *DXGIDevice = 0;
			Direct3D->Device->QueryInterface(__uuidof(IDXGIDevice), (void **)&DXGIDevice);
			IDXGIAdapter *DXGIAdapter = 0;
			DXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void **)&DXGIAdapter);
			IDXGIFactory *DXGIFactory = 0;
			DXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void **)&DXGIFactory);
			
			DXGIFactory->CreateSwapChain(Direct3D->Device, &SwapChainDescription, &Direct3D->SwapChain);
			
			DXGIDevice->Release();
			DXGIAdapter->Release();
			DXGIFactory->Release();

			ID3D11Texture2D *BackBuffer;
			Direct3D->SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&BackBuffer);
			Direct3D->Device->CreateRenderTargetView(BackBuffer, 0, &Direct3D->RenderTargetView);
			BackBuffer->Release();

			D3D11_TEXTURE2D_DESC DepthStencilDescription;
			DepthStencilDescription.Width = Direct3D->WindowWidth;
			DepthStencilDescription.Height = Direct3D->WindowHeight;
			DepthStencilDescription.MipLevels = 1;
			DepthStencilDescription.ArraySize = 1;
			DepthStencilDescription.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			DepthStencilDescription.SampleDesc.Count = 1;
			DepthStencilDescription.SampleDesc.Quality = 0;
			DepthStencilDescription.Usage = D3D11_USAGE_DEFAULT;
			DepthStencilDescription.BindFlags = D3D11_BIND_DEPTH_STENCIL;
			DepthStencilDescription.CPUAccessFlags = 0;
			DepthStencilDescription.MiscFlags = 0;

			Direct3D->Device->CreateTexture2D(&DepthStencilDescription, 0, &Direct3D->DepthStencilBuffer);
			Direct3D->Device->CreateDepthStencilView(Direct3D->DepthStencilBuffer, 0, &Direct3D->DepthStencilView);

			Direct3D->ImmediateContext->OMSetRenderTargets(1, &Direct3D->RenderTargetView, Direct3D->DepthStencilView);

			D3D11_VIEWPORT ViewPort;
			ViewPort.TopLeftX = 0.0f;
			ViewPort.TopLeftY = 0.0f;
			ViewPort.Width = (real32)Direct3D->WindowWidth;
			ViewPort.Height = (real32)Direct3D->WindowHeight;
			ViewPort.MinDepth = 0.0f;
			ViewPort.MaxDepth = 1.0f;

			Direct3D->ImmediateContext->RSSetViewports(1, &ViewPort);


			// NOTE(georgy): Compile and create default shaders
			ID3D10Blob *VSBuffer;
			ID3D10Blob *PSBuffer;
			ID3D10Blob *CompilationMessages = 0;

			HRESULT ShaderCompileResult = D3DCompileFromFile(L"shaders/DefaultVS.hlsl", 0, 0, "VS", "vs_5_0", D3D10_SHADER_DEBUG | D3D10_SHADER_SKIP_OPTIMIZATION,
															 0, &VSBuffer, &CompilationMessages);
			if(CompilationMessages)
			{
				MessageBox(0, (char *)CompilationMessages->GetBufferPointer(), 0, 0);
				CompilationMessages->Release();
			}
			if (FAILED(Hr))
			{
				MessageBox(0, "D3D11CompileFromFile failed", 0, 0);
			}

			ShaderCompileResult = D3DCompileFromFile(L"shaders/DefaultPS.hlsl", 0, 0, "PS", "ps_5_0", D3D10_SHADER_DEBUG | D3D10_SHADER_SKIP_OPTIMIZATION,
															 0, &PSBuffer, &CompilationMessages);
			if(CompilationMessages)
			{
				MessageBox(0, (char *)CompilationMessages->GetBufferPointer(), 0, 0);
				CompilationMessages->Release();
			}
			if (FAILED(Hr))
			{
				MessageBox(0, "D3D11CompileFromFile failed", 0, 0);
			}

			ID3D11VertexShader *VS;
			ID3D11PixelShader *PS;
			Direct3D->Device->CreateVertexShader(VSBuffer->GetBufferPointer(), VSBuffer->GetBufferSize(), 0, &VS);
			Direct3D->Device->CreatePixelShader(PSBuffer->GetBufferPointer(), PSBuffer->GetBufferSize(), 0, &PS);

			// NOTE(georgy): Create rasterizer state
			ID3D11RasterizerState *RasterizerState;
			
			D3D11_RASTERIZER_DESC RasterizerStateDescr;
  			RasterizerStateDescr.FillMode = D3D11_FILL_SOLID;
  			RasterizerStateDescr.CullMode = D3D11_CULL_BACK;
  			RasterizerStateDescr.FrontCounterClockwise = TRUE;
  			RasterizerStateDescr.DepthBias = 0;
  			RasterizerStateDescr.DepthBiasClamp = 0;
  			RasterizerStateDescr.SlopeScaledDepthBias = 0;
  			RasterizerStateDescr.DepthClipEnable = TRUE;
  			RasterizerStateDescr.ScissorEnable = FALSE;
  			RasterizerStateDescr.MultisampleEnable = FALSE;
  			RasterizerStateDescr.AntialiasedLineEnable = FALSE;

			Direct3D->Device->CreateRasterizerState(&RasterizerStateDescr, &RasterizerState);

			// NOTE(georgy): Create depth stencil state
			ID3D11DepthStencilState *DepthStencilState;

			D3D11_DEPTH_STENCIL_DESC DepthStencilStateDescr;
			DepthStencilStateDescr.DepthEnable = true;
			DepthStencilStateDescr.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			DepthStencilStateDescr.DepthFunc = D3D11_COMPARISON_LESS;
			DepthStencilStateDescr.StencilEnable = false;
			DepthStencilStateDescr.StencilReadMask = 0;
			DepthStencilStateDescr.StencilWriteMask = 0;
			DepthStencilStateDescr.FrontFace = {};
			DepthStencilStateDescr.BackFace = {};

			Direct3D->Device->CreateDepthStencilState(&DepthStencilStateDescr, &DepthStencilState);

			// NOTE(georgy): Create blend state
			ID3D11BlendState *BlendState;

			D3D11_BLEND_DESC BlendStateDescr = {};
  			BlendStateDescr.AlphaToCoverageEnable = FALSE;
  			BlendStateDescr.IndependentBlendEnable = FALSE;
			BlendStateDescr.RenderTarget[0].BlendEnable = FALSE;
			BlendStateDescr.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			BlendStateDescr.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			BlendStateDescr.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			BlendStateDescr.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			BlendStateDescr.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
			BlendStateDescr.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			BlendStateDescr.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

			Direct3D->Device->CreateBlendState(&BlendStateDescr, &BlendState);

			// NOTE(georgy): Create vertex buffer for a triangle

			v3 Pos0 = V3(-1.0f, -1.0f, 0.0f);
			v3 Pos1 = V3(1.0f, -1.0f, 0.0f);
			v3 Pos2 = V3(-1.0f, 1.0f, 0.0f);
			v3 Pos3 = V3(1.0f, 1.0f, 0.0f);

			v2 UV0 = V2(0.0f, 1.0f);
			v2 UV1 = V2(1.0f, 1.0f);
			v2 UV2 = V2(0.0f, 0.0f);
			v2 UV3 = V2(1.0f, 0.0f);

			v3 Edge1 = Pos3 - Pos2;
			v3 Edge2 = Pos0 - Pos2;
			v2 DeltaUV1 = UV3 - UV2;
			v2 DeltaUV2 = UV0 - UV2;

			v3 Tangent;
			Tangent.x = (DeltaUV2.y * Edge1.x - DeltaUV1.y * Edge2.x);
			Tangent.y = (DeltaUV2.y * Edge1.y - DeltaUV1.y * Edge2.y);
			Tangent.z = (DeltaUV2.y * Edge1.z - DeltaUV1.y * Edge2.z);
			Tangent.Normalize();

			v3 Bitangent;
			Bitangent.x = (-DeltaUV2.x * Edge1.x + DeltaUV1.x * Edge2.x);
			Bitangent.y = (-DeltaUV2.x * Edge1.y + DeltaUV1.x * Edge2.y);
			Bitangent.z = (-DeltaUV2.x * Edge1.z + DeltaUV1.x * Edge2.z);
			Bitangent.Normalize();

			vertex QuadVertices[] =
			{
				{ Pos0, V3(0.0f, 0.0f, -1.0f), UV0, Tangent, Bitangent },
				{ Pos1, V3(0.0f, 0.0f, -1.0f), UV1, Tangent, Bitangent },
				{ Pos2, V3(0.0f, 0.0f, -1.0f), UV2, Tangent, Bitangent },
				{ Pos3, V3(0.0f, 0.0f, -1.0f), UV3, Tangent, Bitangent }
			};

			ID3D11Buffer *VertexBuffer;

			D3D11_BUFFER_DESC VertexBufferDescr;
			VertexBufferDescr.ByteWidth = sizeof(QuadVertices);
			VertexBufferDescr.Usage = D3D11_USAGE_IMMUTABLE;
			VertexBufferDescr.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			VertexBufferDescr.CPUAccessFlags = 0;
			VertexBufferDescr.MiscFlags = 0;
			VertexBufferDescr.StructureByteStride = 0;

			D3D11_SUBRESOURCE_DATA VertexBufferInitData;
			VertexBufferInitData.pSysMem = QuadVertices;
  			VertexBufferInitData.SysMemPitch = 0;
			VertexBufferInitData.SysMemSlicePitch = 0;

			Direct3D->Device->CreateBuffer(&VertexBufferDescr, &VertexBufferInitData, &VertexBuffer);

			// NOTE(georgy): Create input layout
			ID3D11InputLayout *InputLayout;
			D3D11_INPUT_ELEMENT_DESC InputLayoutDescription[] = 
			{
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 3*sizeof(float), D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 6*sizeof(float), D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 8*sizeof(float), D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 11*sizeof(float), D3D11_INPUT_PER_VERTEX_DATA, 0},
			};
			Direct3D->Device->CreateInputLayout(InputLayoutDescription, ArrayCount(InputLayoutDescription), 
												VSBuffer->GetBufferPointer(), VSBuffer->GetBufferSize(), &InputLayout);

			VSBuffer->Release();
			PSBuffer->Release();

			// NOTE(georgy): Create constant buffer for matrices
			ID3D11Buffer *MatrixBuffer;

			D3D11_BUFFER_DESC MatrixBufferDescr;
			MatrixBufferDescr.ByteWidth = sizeof(matrix_buffer);
			MatrixBufferDescr.Usage = D3D11_USAGE_DYNAMIC;
			MatrixBufferDescr.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			MatrixBufferDescr.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			MatrixBufferDescr.MiscFlags = 0;
			MatrixBufferDescr.StructureByteStride = 0;

			Direct3D->Device->CreateBuffer(&MatrixBufferDescr, 0, &MatrixBuffer);

			// NOTE(georgy): Create constant buffer for camera info
			ID3D11Buffer *CameraInfoBuffer;

			D3D11_BUFFER_DESC CameraInfoBufferDescr;
			CameraInfoBufferDescr.ByteWidth = sizeof(v4);
			CameraInfoBufferDescr.Usage = D3D11_USAGE_DYNAMIC;
			CameraInfoBufferDescr.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			CameraInfoBufferDescr.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			CameraInfoBufferDescr.MiscFlags = 0;
			CameraInfoBufferDescr.StructureByteStride = 0;

			Direct3D->Device->CreateBuffer(&CameraInfoBufferDescr, 0, &CameraInfoBuffer);

			// NOTE(georgy): Create texture and sampler state
#if 0
			ID3D11Resource *BrickTexture;
			ID3D11ShaderResourceView *BrickTextureResourceView;
			CreateWICTextureFromFile(Direct3D->Device, Direct3D->ImmediateContext, L"bricks2.jpg", &BrickTexture, &BrickTextureResourceView);

			ID3D11Resource *BrickNormalMap;
			ID3D11ShaderResourceView *BrickNormalMapResourceView;
			CreateWICTextureFromFile(Direct3D->Device, Direct3D->ImmediateContext, L"bricks2_normal.jpg", &BrickNormalMap, &BrickNormalMapResourceView);

			ID3D11Resource *BrickDisplacementMap;
			ID3D11ShaderResourceView *BrickDisplacementMapResourceView;
			CreateWICTextureFromFile(Direct3D->Device, Direct3D->ImmediateContext, L"bricks2_disp.jpg", &BrickDisplacementMap, &BrickDisplacementMapResourceView);
#else
			ID3D11Resource *WoodTexture;
			ID3D11ShaderResourceView *WoodTextureResourceView;
			CreateWICTextureFromFile(Direct3D->Device, Direct3D->ImmediateContext, L"wood.png", &WoodTexture, &WoodTextureResourceView);

			ID3D11Resource *ToyBoxNormalMap;
			ID3D11ShaderResourceView *ToyBoxNormalMapResourceView;
			CreateWICTextureFromFile(Direct3D->Device, Direct3D->ImmediateContext, L"toy_box_normal.png", &ToyBoxNormalMap, &ToyBoxNormalMapResourceView);

			ID3D11Resource *ToyBoxDisplacementMap;
			ID3D11ShaderResourceView *ToyBoxDisplacementMapResourceView;
			CreateWICTextureFromFile(Direct3D->Device, Direct3D->ImmediateContext, L"toy_box_disp.png", &ToyBoxDisplacementMap, &ToyBoxDisplacementMapResourceView);
#endif

			ID3D11SamplerState *SamplerState;
			D3D11_SAMPLER_DESC SamplerDescr;
			SamplerDescr.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			SamplerDescr.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
			SamplerDescr.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
			SamplerDescr.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			SamplerDescr.MipLODBias = 0;
			SamplerDescr.MaxAnisotropy = 1;
			SamplerDescr.ComparisonFunc = D3D11_COMPARISON_NEVER;
			SamplerDescr.MinLOD = -D3D11_FLOAT32_MAX;
			SamplerDescr.MaxLOD = D3D11_FLOAT32_MAX;

			Direct3D->Device->CreateSamplerState(&SamplerDescr, &SamplerState);

			RAWINPUTDEVICE RIDs[1];
			RIDs[0].usUsagePage = 0x01;
            RIDs[0].usUsage = 0x02; // NOTE(georgy): mouse input
            RIDs[0].dwFlags = 0;
            RIDs[0].hwndTarget = Window;

			if(!RegisterRawInputDevices(RIDs, ArrayCount(RIDs), sizeof(RIDs[0])))
            {
                GlobalRunning = false;
                MessageBox(Window, "Can't register input devices.", 0, MB_OK);
            }

			game_input GameInput = {};
			POINT MouseP;
            GetCursorPos(&MouseP);
            ScreenToClient(Window, &MouseP);
            GameInput.MouseX = MouseP.x;
            GameInput.MouseY = MouseP.y;

			v3 CameraPos = V3(0.0f, 0.0f, -3.0f);
			v3 CameraFront = V3(0.0f, 0.0f, 1.0f);
			v3 CameraRight = V3(1.0f, 0.0f, 0.0f);
			float CameraPitch = 0.0f;
			float CameraHead = 0.0f;
			float MouseSensitivity = 0.005f;

			// NOTE(georgy): Game loop
			real32 DeltaTime = 0.016f;
			GlobalRunning = true;
			LARGE_INTEGER LastCounter = GetWallClock();
			while (GlobalRunning)
			{
				if(GlobalWindowIsFocused)
                {
                    RECT ClipRect;
                    GetWindowRect(Window, &ClipRect);
                    LONG ClipRectHeight = ClipRect.bottom - ClipRect.top;
                    LONG ClipRectWidth = ClipRect.right - ClipRect.left;
                    ClipRect.top = ClipRect.bottom = ClipRect.top + ClipRectHeight/2;
                    ClipRect.left = ClipRect.right = ClipRect.left + ClipRectWidth/2;
                    ClipCursor(&ClipRect);
                }
                else
                {
                    ClipCursor(0);
                }

				GameInput.DeltaMouseX = GameInput.DeltaMouseY = 0;
				for(uint32_t ButtonIndex = 0; ButtonIndex < ArrayCount(GameInput.Buttons); ButtonIndex++)
				{
					GameInput.Buttons[ButtonIndex].HalfTransitionCount = 0;					
				}

				ProcessPendingMessages(&GameInput);

				CameraRight = Cross(V3(0.0f, 1.0f, 0.0f), CameraFront);
				if(GameInput.MoveForward.EndedDown)
				{
					CameraPos += 10.0f*CameraFront*DeltaTime;
				}
				if(GameInput.MoveBack.EndedDown)
				{
					CameraPos -= 10.0f*CameraFront*DeltaTime;
				}
				if(GameInput.MoveLeft.EndedDown)
				{
					CameraPos -= 10.0f*CameraRight*DeltaTime;
				}
				if(GameInput.MoveRight.EndedDown)
				{
					CameraPos += 10.0f*CameraRight*DeltaTime;
				}

				CameraHead += MouseSensitivity*GameInput.DeltaMouseX;
				CameraPitch += MouseSensitivity*GameInput.DeltaMouseY;
				CameraPitch = (CameraPitch > 89.0f) ? 89.0f : CameraPitch;
				CameraPitch = (CameraPitch < -89.0f) ? -89.0f : CameraPitch;

				CameraFront = V3(sinf(CameraHead)*cosf(CameraPitch), sinf(-CameraPitch), cosf(CameraHead)*cosf(CameraPitch));


				Direct3D->ImmediateContext->ClearRenderTargetView(Direct3D->RenderTargetView, Colors::Black);
				Direct3D->ImmediateContext->ClearDepthStencilView(Direct3D->DepthStencilView, 
																  D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0.0f);

				Direct3D->ImmediateContext->OMSetRenderTargets(1, &Direct3D->RenderTargetView, Direct3D->DepthStencilView);
				Direct3D->ImmediateContext->OMSetDepthStencilState(DepthStencilState, 0);
				Direct3D->ImmediateContext->RSSetState(RasterizerState);
				Direct3D->ImmediateContext->OMSetBlendState(BlendState, 0, 0xFFFFFFFF);

				Direct3D->ImmediateContext->IASetInputLayout(InputLayout);
				UINT Stride = sizeof(vertex);
				UINT Offset = 0;
				Direct3D->ImmediateContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
				Direct3D->ImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				Direct3D->ImmediateContext->VSSetShader(VS, 0, 0);
				Direct3D->ImmediateContext->PSSetShader(PS, 0, 0);

				D3D11_MAPPED_SUBRESOURCE MappedResource;
				Direct3D->ImmediateContext->Map(MatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
				matrix_buffer *MatrixBufferPtr = (matrix_buffer *)MappedResource.pData;
				MatrixBufferPtr->Model = Identity();
				MatrixBufferPtr->View = LookAt(CameraPos, CameraPos + CameraFront);
				MatrixBufferPtr->Projection = Perspective(45.0f, (real32)Direct3D->WindowWidth / (real32)Direct3D->WindowHeight, 0.1f, 100.0f);
				Direct3D->ImmediateContext->Unmap(MatrixBuffer, 0);
				Direct3D->ImmediateContext->VSSetConstantBuffers(0, 1, &MatrixBuffer);

				Direct3D->ImmediateContext->Map(CameraInfoBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
				v3 *CameraPosShader = (v3 *)MappedResource.pData;
				*CameraPosShader = CameraPos;
				Direct3D->ImmediateContext->Unmap(CameraInfoBuffer, 0);
				Direct3D->ImmediateContext->PSSetConstantBuffers(1, 1, &CameraInfoBuffer);

				Direct3D->ImmediateContext->PSSetShaderResources(0, 1, &WoodTextureResourceView);
				Direct3D->ImmediateContext->PSSetShaderResources(1, 1, &ToyBoxNormalMapResourceView);
				Direct3D->ImmediateContext->PSSetShaderResources(2, 1, &ToyBoxDisplacementMapResourceView);
				Direct3D->ImmediateContext->PSSetSamplers(0, 1, &SamplerState);

				Direct3D->ImmediateContext->Draw(4, 0);

				DeltaTime = GetSecondsElapsed(LastCounter, GetWallClock());
				LastCounter = GetWallClock();
				char FPSBuffer[256];
				//_snprintf_s(FPSBuffer, sizeof(FPSBuffer), "%.02fms/f\n", DeltaTime);
				//OutputDebugString(FPSBuffer);

				Direct3D->SwapChain->Present(0, 0);
			}
		}
	}

	return(0);
}
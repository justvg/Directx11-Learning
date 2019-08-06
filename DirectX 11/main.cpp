#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXColors.h>

#include <vector>

#include "math.hpp"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

#include <stdio.h>

#define Assert(Expression) if((!Expression)) { *(int *)0 = 0; }

struct d3d_app
{
	ID3D11Device *Device;
	ID3D11DeviceContext *ImmediateContext;
	IDXGISwapChain *SwapChain;
	ID3D11Texture2D *DepthStencilBuffer;
	ID3D11RenderTargetView *RenderTargetView;
	ID3D11DepthStencilView *DepthStencilView;

	UINT MSAA4XQuality;
	bool Resizing = false;

	UINT WindowWidth, WindowHeight;
};

global_variable bool GlobalRunning;
global_variable d3d_app GlobalDirect3D;
global_variable LARGE_INTEGER GlobalPerfCounterFrequency;

inline LARGE_INTEGER
GetWallClock(void)
{
	LARGE_INTEGER Result;
	QueryPerformanceCounter(&Result);
	return(Result);
}

inline float 
GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
	float Result = (End.QuadPart - Start.QuadPart) / (float)GlobalPerfCounterFrequency.QuadPart;
	return(Result);
}

global_variable void
Resize()
{
	if (GlobalDirect3D.DepthStencilView && GlobalDirect3D.DepthStencilBuffer && GlobalDirect3D.RenderTargetView)
	{
		GlobalDirect3D.RenderTargetView->Release();
		GlobalDirect3D.DepthStencilView->Release();
		GlobalDirect3D.DepthStencilBuffer->Release();

		GlobalDirect3D.SwapChain->ResizeBuffers(1, GlobalDirect3D.WindowWidth, GlobalDirect3D.WindowHeight, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
		ID3D11Texture2D *BackBuffer;
		GlobalDirect3D.SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&BackBuffer);
		GlobalDirect3D.Device->CreateRenderTargetView(BackBuffer, 0, &GlobalDirect3D.RenderTargetView);
		BackBuffer->Release();

		D3D11_TEXTURE2D_DESC DepthStencilDescription;
		DepthStencilDescription.Width = GlobalDirect3D.WindowWidth;
		DepthStencilDescription.Height = GlobalDirect3D.WindowHeight;
		DepthStencilDescription.MipLevels = 1;
		DepthStencilDescription.ArraySize = 1;
		DepthStencilDescription.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		if (GlobalDirect3D.MSAA4XQuality)
		{
			DepthStencilDescription.SampleDesc.Count = 4;
			DepthStencilDescription.SampleDesc.Quality = GlobalDirect3D.MSAA4XQuality - 1;
		}
		else
		{
			DepthStencilDescription.SampleDesc.Count = 1;
			DepthStencilDescription.SampleDesc.Quality = 0;
		}
		DepthStencilDescription.Usage = D3D11_USAGE_DEFAULT;
		DepthStencilDescription.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		DepthStencilDescription.CPUAccessFlags = 0;
		DepthStencilDescription.MiscFlags = 0;

		GlobalDirect3D.Device->CreateTexture2D(&DepthStencilDescription, 0, &GlobalDirect3D.DepthStencilBuffer);
		GlobalDirect3D.Device->CreateDepthStencilView(GlobalDirect3D.DepthStencilBuffer, 0, &GlobalDirect3D.DepthStencilView);

		GlobalDirect3D.ImmediateContext->OMSetRenderTargets(1, &GlobalDirect3D.RenderTargetView, GlobalDirect3D.DepthStencilView);

		D3D11_VIEWPORT ViewPort;
		ViewPort.TopLeftX = 0.0f;
		ViewPort.TopLeftY = 0.0f;
		ViewPort.Width = (float)GlobalDirect3D.WindowWidth;
		ViewPort.Height = (float)GlobalDirect3D.WindowHeight;
		ViewPort.MinDepth = 0.0f;
		ViewPort.MaxDepth = 1.0f;

		GlobalDirect3D.ImmediateContext->RSSetViewports(1, &ViewPort);
	}
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

		case WM_SIZE:
		{
			GlobalDirect3D.WindowWidth = LOWORD(LParam);
			GlobalDirect3D.WindowHeight = HIWORD(LParam);

			if (!GlobalDirect3D.Resizing)
			{
				Resize();
			}
		} break;

		case WM_ENTERSIZEMOVE:
		{
			GlobalDirect3D.Resizing = true;
		} break;
		case WM_EXITSIZEMOVE:
		{
			GlobalDirect3D.Resizing = false;
			Resize();
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

global_variable void
ProcessPendingMessages()
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

			default:
			{
				TranslateMessage(&Message);
				DispatchMessage(&Message);
			} break;
		}
	}
}

struct vertex
{
	vec3 Pos;
	vec4 Color;
};

struct matrix_buffer
{
	mat4 Model;
	mat4 View;
	mat4 Projection;
};

struct grid
{
	std::vector<vertex> Vertices;
	std::vector<UINT> Indices;

	uint32_t Width, Depth;
	uint32_t VerticesAlongX, VerticesAlongZ;

	ID3D11Buffer *VB, *IB;
};

inline float
GetHeight(float X, float Z)
{
	return (0.3f*(Z*sinf(0.1f*X) + X*cosf(0.1f*Z)));
}

global_variable void
ConstructGrid(grid *Grid, uint32_t VerticesAlongX, uint32_t VerticesAlongZ, uint32_t Width, uint32_t Depth)
{
	Grid->Width = Width;
	Grid->Depth = Depth;
	Grid->VerticesAlongX = VerticesAlongX;
	Grid->VerticesAlongZ = VerticesAlongZ;

	Grid->Vertices.resize(VerticesAlongX*VerticesAlongZ);
	Grid->Indices.resize(3*2*(VerticesAlongX-1)*(VerticesAlongZ-1));

	float StepX = (float)Width / (VerticesAlongX - 1);
	float StepZ = (float)Depth / (VerticesAlongZ - 1);

	for (uint32_t I = 0; I < VerticesAlongZ; I++)
	{
		float Z = 0.5f*Depth - I*StepZ;
		for (uint32_t J = 0; J < VerticesAlongX; J++)
		{
			float X = -0.5f*Width + J*StepX;
			float Y = GetHeight(X, Z);
			vertex Vertex;
			Vertex.Pos = vec3(X, Y, Z);

			if (Vertex.Pos.y() < -10.0f)
			{
				Vertex.Color = vec4(1.0f, 0.96f, 0.62f, 1.0f);
			}
			else if (Vertex.Pos.y() < 5.0f)
			{
				Vertex.Color = vec4(0.48f, 0.77f, 0.46f, 1.0f);
			}
			else if (Vertex.Pos.y() < 12.0f)
			{
				Vertex.Color = vec4(0.1f, 0.48f, 0.19f, 1.0f);
			}
			else if (Vertex.Pos.y()< 20.0f)
			{
				Vertex.Color = vec4(0.45f, 0.39f, 0.34f, 1.0f);
			}
			else
			{
				Vertex.Color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
			}			Grid->Vertices[I*VerticesAlongX + J] = Vertex;		}
	}

	UINT K = 0;
	for (UINT I = 0; I < VerticesAlongZ - 1; I++)
	{
		for (UINT J = 0; J < VerticesAlongX - 1; J++)
		{
			Grid->Indices[K] = I*VerticesAlongX + J;
			Grid->Indices[K + 1] = I*VerticesAlongX + J + 1;
			Grid->Indices[K + 2] = (I + 1)*VerticesAlongX + J;
			Grid->Indices[K + 3] = I*VerticesAlongX + J + 1;
			Grid->Indices[K + 4] = (I + 1)*VerticesAlongX + J + 1;
			Grid->Indices[K + 5] = (I + 1)*VerticesAlongX + J;
			
			K += 6;
		}
	}

	D3D11_BUFFER_DESC VBDescription;
	VBDescription.ByteWidth = Grid->Vertices.size()*sizeof(vertex);
	VBDescription.Usage = D3D11_USAGE_IMMUTABLE;
	VBDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	VBDescription.CPUAccessFlags = 0;
	VBDescription.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA VBInitData;
	VBInitData.pSysMem = &Grid->Vertices[0];
	GlobalDirect3D.Device->CreateBuffer(&VBDescription, &VBInitData, &Grid->VB);

	D3D11_BUFFER_DESC IBDescription;
	IBDescription.ByteWidth = Grid->Indices.size() * sizeof(UINT);
	IBDescription.Usage = D3D11_USAGE_IMMUTABLE;
	IBDescription.BindFlags = D3D11_BIND_INDEX_BUFFER;
	IBDescription.CPUAccessFlags = 0;
	IBDescription.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA IBInitData;
	IBInitData.pSysMem = &Grid->Indices[0];
	GlobalDirect3D.Device->CreateBuffer(&IBDescription, &IBInitData, &Grid->IB);
}

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
	WindowClass.lpszClassName = "DirectX WindowClass";

	if(RegisterClass(&WindowClass))
	{
		HWND Window = CreateWindowEx(0, WindowClass.lpszClassName, "DirectX Window",
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

			Direct3D->Device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 4, &Direct3D->MSAA4XQuality);
			Assert(Direct3D->MSAA4XQuality);

			DXGI_SWAP_CHAIN_DESC SwapChainDescription;
			SwapChainDescription.BufferDesc.Width = Direct3D->WindowWidth;
			SwapChainDescription.BufferDesc.Height = Direct3D->WindowHeight;
			SwapChainDescription.BufferDesc.RefreshRate.Numerator = 60;
			SwapChainDescription.BufferDesc.RefreshRate.Denominator = 1;
			SwapChainDescription.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			SwapChainDescription.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
			SwapChainDescription.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
			if (Direct3D->MSAA4XQuality)
			{
				SwapChainDescription.SampleDesc.Count = 4;
				SwapChainDescription.SampleDesc.Quality = Direct3D->MSAA4XQuality - 1;
			}
			else
			{
				SwapChainDescription.SampleDesc.Count = 1;
				SwapChainDescription.SampleDesc.Quality = 0;
			}
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
			if (Direct3D->MSAA4XQuality)
			{
				DepthStencilDescription.SampleDesc.Count = 4;
				DepthStencilDescription.SampleDesc.Quality = Direct3D->MSAA4XQuality - 1;
			}
			else
			{
				DepthStencilDescription.SampleDesc.Count = 1;
				DepthStencilDescription.SampleDesc.Quality = 0;
			}
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
			ViewPort.Width = (float)Direct3D->WindowWidth;
			ViewPort.Height = (float)Direct3D->WindowHeight;
			ViewPort.MinDepth = 0.0f;
			ViewPort.MaxDepth = 1.0f;

			Direct3D->ImmediateContext->RSSetViewports(1, &ViewPort);

			UINT ShaderFlags = 0;
#if DEBUG | _DEBUG
			ShaderFlags |= D3D10_SHADER_DEBUG;
			ShaderFlags |= D3D10_SHADER_SKIP_OPTIMIZATION;
#endif
			ID3D10Blob *VSBuffer;
			ID3D10Blob *PSBuffer;
			ID3D10Blob *CompilationMessages = 0;
			Hr = D3DCompileFromFile(L"shaders/ColorV.hlsl", 0, 0, "VS", "vs_5_0", ShaderFlags, 0,
									&VSBuffer, &CompilationMessages);
			if (CompilationMessages)
			{
				MessageBox(0, (char *)CompilationMessages->GetBufferPointer(), 0, 0);
				CompilationMessages->Release();
			}
			if (FAILED(Hr))
			{
				MessageBox(0, "D3D11CompileFromFile failed", 0, 0);
			}

			Hr = D3DCompileFromFile(L"shaders/ColorP.hlsl", 0, 0, "PS", "ps_5_0", ShaderFlags, 0,
									&PSBuffer, &CompilationMessages);
			if (CompilationMessages)
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

			ID3D11InputLayout *InputLayout;
			D3D11_INPUT_ELEMENT_DESC InputLayoutDescription[] = 
				{
					{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, sizeof(vec3), D3D11_INPUT_PER_VERTEX_DATA, 0 },
				};
			Direct3D->Device->CreateInputLayout(InputLayoutDescription, sizeof(InputLayoutDescription)/sizeof(InputLayoutDescription[0]), 
												VSBuffer->GetBufferPointer(), VSBuffer->GetBufferSize(), &InputLayout);
			VSBuffer->Release();
			PSBuffer->Release();

			vertex Vertices[] =
				{
					vec3(-1.0f, -1.0f, -1.0f), vec4(1.0f, 1.0f, 1.0f, 1.0f),
					vec3(-1.0f, 1.0f, -1.0f), vec4(0.0f, 0.0f, 0.0f, 1.0f),
					vec3(1.0f, 1.0f, -1.0f), vec4(1.0f, 0.0f, 0.0f, 1.0f),
					vec3(1.0f, -1.0f, -1.0f), vec4(0.0f, 1.0f, 0.0f, 1.0f),
					vec3(-1.0f, -1.0f, 1.0f), vec4(0.0f, 0.0f, 1.0f, 1.0f),
					vec3(-1.0f, 1.0f, 1.0f), vec4(1.0f, 1.0f, 0.0f, 1.0f),
					vec3(1.0f, 1.0f, 1.0f), vec4(1.0f, 0.0f, 1.0f, 1.0f),
					vec3(1.0f, -1.0f, 1.0f), vec4(0.0f, 1.0f, 1.0f, 1.0f),
				};

			ID3D11Buffer *VB;
			D3D11_BUFFER_DESC VBDescription;
			VBDescription.ByteWidth = sizeof(Vertices);
			VBDescription.Usage = D3D11_USAGE_IMMUTABLE;
			VBDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			VBDescription.CPUAccessFlags = 0;
			VBDescription.MiscFlags = 0;
			D3D11_SUBRESOURCE_DATA VBInitData;
			VBInitData.pSysMem = Vertices;
			Direct3D->Device->CreateBuffer(&VBDescription, &VBInitData, &VB);

			UINT Indices[] = {
				0, 1, 2,
				0, 2, 3,
				4, 6, 5,
				4, 7, 6,
				4, 5, 1,
				4, 1, 0,
				3, 2, 6,
				3, 6, 7,
				1, 5, 6,
				1, 6, 2,
				4, 0, 3,
				4, 3, 7
			};

			ID3D11Buffer *IB;
			D3D11_BUFFER_DESC IBDescription;
			IBDescription.ByteWidth = sizeof(Indices);
			IBDescription.Usage = D3D11_USAGE_IMMUTABLE;
			IBDescription.BindFlags = D3D11_BIND_INDEX_BUFFER;
			IBDescription.CPUAccessFlags = 0;
			IBDescription.MiscFlags = 0;
			D3D11_SUBRESOURCE_DATA IBInitData;
			IBInitData.pSysMem = Indices;
			Direct3D->Device->CreateBuffer(&IBDescription, &IBInitData, &IB);

			ID3D11Buffer *MatrixBuffer;
			D3D11_BUFFER_DESC MatrixBufferDescription;
			MatrixBufferDescription.ByteWidth = sizeof(matrix_buffer);
			MatrixBufferDescription.Usage = D3D11_USAGE_DYNAMIC;
			MatrixBufferDescription.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			MatrixBufferDescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			MatrixBufferDescription.MiscFlags = 0;
			Direct3D->Device->CreateBuffer(&MatrixBufferDescription, 0, &MatrixBuffer);

			mat4 Model;
			mat4 View;
			mat4 Projection;
			Model = Identity();
			vec3 CameraPos = vec3(0.0f, 0.0f, -5.0f);
			View = LookAt(CameraPos, CameraPos + vec3(0.0f, 0.0f, 1.0f));
			Projection = Perspective(45.0f, (float)Direct3D->WindowWidth / (float)Direct3D->WindowHeight, 0.1f, 100.0f);

			D3D11_MAPPED_SUBRESOURCE MappedResource;
			matrix_buffer *DataPtr;
			Direct3D->ImmediateContext->Map(MatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
			DataPtr = (matrix_buffer *)MappedResource.pData;
			DataPtr->Model = Model;
			DataPtr->View = View;
			DataPtr->Projection = Projection;
			Direct3D->ImmediateContext->Unmap(MatrixBuffer, 0);
			Direct3D->ImmediateContext->VSSetConstantBuffers(0, 1, &MatrixBuffer);

#if 0
			grid Grid;
			ConstructGrid(&Grid, 300, 300, 30.0f, 30.0f);
#endif

			ID3D11RasterizerState *RasterizerState;
			D3D11_RASTERIZER_DESC RasterizerDescription = {};
			RasterizerDescription.FillMode = D3D11_FILL_SOLID;
			RasterizerDescription.CullMode = D3D11_CULL_NONE;
			RasterizerDescription.FrontCounterClockwise = FALSE;
			RasterizerDescription.DepthBias = 0;
			RasterizerDescription.DepthBiasClamp = 0;
			RasterizerDescription.SlopeScaledDepthBias = 0;
			RasterizerDescription.DepthClipEnable = TRUE;
			Direct3D->Device->CreateRasterizerState(&RasterizerDescription, &RasterizerState);

			GlobalRunning = true;
			LARGE_INTEGER LastCounter = GetWallClock();
			while (GlobalRunning)
			{
				ProcessPendingMessages();

				Direct3D->ImmediateContext->ClearRenderTargetView(Direct3D->RenderTargetView, Colors::Blue);
				Direct3D->ImmediateContext->ClearDepthStencilView(Direct3D->DepthStencilView, 
																  D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0.0f);

				Direct3D->ImmediateContext->RSSetState(RasterizerState);
				Direct3D->ImmediateContext->IASetInputLayout(InputLayout);
				Direct3D->ImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				UINT Stride = sizeof(vertex);
				UINT Offset = 0;
#if 0
				Direct3D->ImmediateContext->IASetVertexBuffers(0, 1, &Grid.VB, &Stride, &Offset);
				Direct3D->ImmediateContext->IASetIndexBuffer(Grid.IB, DXGI_FORMAT_R32_UINT, 0);
#else
				Direct3D->ImmediateContext->IASetVertexBuffers(0, 1, &VB, &Stride, &Offset);
				Direct3D->ImmediateContext->IASetIndexBuffer(IB, DXGI_FORMAT_R32_UINT, 0);
#endif
				Direct3D->ImmediateContext->VSSetShader(VS, 0, 0);
				Direct3D->ImmediateContext->PSSetShader(PS, 0, 0);

				Direct3D->ImmediateContext->DrawIndexed(36, 0, 0);

				float DeltaTime = GetSecondsElapsed(LastCounter, GetWallClock());
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
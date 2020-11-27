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

#define Assert(Expression) if(!(Expression)) { *(int *)0 = 0; }
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

static void
ProcessKeyboardMessage(game_button_state *Button, bool IsDown)
{
	if(Button->EndedDown != IsDown)
	{
		Button->EndedDown = IsDown;
		Button->HalfTransitionCount++;
	}
}

static void
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

struct light_matrix_buffer
{
	mat4 Projection;
	mat4 View;
};

#include <random>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include <vector>
#include <map>
#include <string>

struct vertex
{
	v3 Pos;
	v3 Normal;
};

struct indexed_primitive
{
	uint32_t PosIndex;
	uint32_t NormalIndex;

	bool operator<(const indexed_primitive& Other) const
	{
		return memcmp(this, &Other, sizeof(indexed_primitive)) > 0;
	}
};

struct mesh
{
	uint32_t IndexOffset;
	uint32_t IndexCount;

	ID3D11Buffer *IndexBuffer;
};

struct model
{
	std::vector<mesh> Meshes;

	ID3D11Buffer *VertexBuffer;
};

void InitializeSceneObjects(char *Filename, model &Model, std::vector<vertex> &VertexArray, std::vector<uint32_t> &IndexArray)
{
	tinyobj::attrib_t Attribs;
	std::vector<tinyobj::shape_t> Shapes;
	std::vector<tinyobj::material_t> Materials;
	std::string Warn = "";
	std::string Err = "";

	bool Loaded = tinyobj::LoadObj(&Attribs, &Shapes, &Materials, &Warn, &Err, Filename, "assets/", true);
	if(Loaded)
	{
		std::map<indexed_primitive, uint32_t> IndexedPrimitives;
		for(uint32_t ShapeIndex = 0; ShapeIndex < Shapes.size(); ShapeIndex++)
		{
			tinyobj::shape_t Shape = Shapes[ShapeIndex];

			uint32_t IndexOffset = IndexArray.size();
			for(uint32_t I = 0; I < Shape.mesh.indices.size(); I++)
			{
				tinyobj::index_t Index = Shape.mesh.indices[I];
				Assert(Index.vertex_index != -1);

				indexed_primitive Prim;
				Prim.PosIndex = Index.vertex_index;
				Prim.NormalIndex = (Index.normal_index != -1) ? Index.normal_index : UINT32_MAX;

				auto FoundPrim = IndexedPrimitives.find(Prim);
				if(FoundPrim != IndexedPrimitives.end())
				{
					IndexArray.push_back(FoundPrim->second);
				}
				else
				{
					uint32_t NewIndex = VertexArray.size();
					IndexedPrimitives[Prim] = NewIndex;
					
					vertex NewVertex;
					NewVertex.Pos.x = Attribs.vertices[3 * Prim.PosIndex];
					NewVertex.Pos.y = Attribs.vertices[3 * Prim.PosIndex + 1];
					NewVertex.Pos.z = -Attribs.vertices[3 * Prim.PosIndex + 2];

					NewVertex.Normal = V3(0, 0, 0);
					if(Prim.NormalIndex != -1)
					{
						NewVertex.Normal.x = Attribs.normals[3 * Prim.NormalIndex];
						NewVertex.Normal.y = Attribs.normals[3 * Prim.NormalIndex + 1];
						NewVertex.Normal.z = -Attribs.normals[3 * Prim.NormalIndex + 2];
					}

					VertexArray.push_back(NewVertex);
					IndexArray.push_back(NewIndex);
				}
			}

			mesh Mesh;
			Mesh.IndexOffset = IndexOffset;
			Mesh.IndexCount = Shape.mesh.indices.size();

			Model.Meshes.push_back(Mesh);
		}

		D3D11_BUFFER_DESC VertexBufferDescr;
		VertexBufferDescr.ByteWidth = sizeof(vertex)*VertexArray.size();
		VertexBufferDescr.Usage = D3D11_USAGE_IMMUTABLE;
		VertexBufferDescr.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		VertexBufferDescr.CPUAccessFlags = 0;
		VertexBufferDescr.MiscFlags = 0;
		VertexBufferDescr.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA VertexBufferInitData;
		VertexBufferInitData.pSysMem = &VertexArray[0];
		VertexBufferInitData.SysMemPitch = 0;
		VertexBufferInitData.SysMemSlicePitch = 0;

		GlobalDirect3D.Device->CreateBuffer(&VertexBufferDescr, &VertexBufferInitData, &Model.VertexBuffer);
		for(uint32_t MeshIndex = 0; MeshIndex < Model.Meshes.size(); MeshIndex++)
		{
			mesh *Mesh = &Model.Meshes[MeshIndex];

			D3D11_BUFFER_DESC IndexBufferDescr;
			IndexBufferDescr.ByteWidth = sizeof(uint32_t)*Mesh->IndexCount;
			IndexBufferDescr.Usage = D3D11_USAGE_IMMUTABLE;
			IndexBufferDescr.BindFlags = D3D11_BIND_INDEX_BUFFER;
			IndexBufferDescr.CPUAccessFlags = 0;
			IndexBufferDescr.MiscFlags = 0;
			IndexBufferDescr.StructureByteStride = 0;

			D3D11_SUBRESOURCE_DATA IndexBufferInitData;
			IndexBufferInitData.pSysMem = &IndexArray[0] + Mesh->IndexOffset;
			IndexBufferInitData.SysMemPitch = 0;
			IndexBufferInitData.SysMemSlicePitch = 0;

			GlobalDirect3D.Device->CreateBuffer(&IndexBufferDescr, &IndexBufferInitData, &Mesh->IndexBuffer);
		}
	}
	else
	{
		OutputDebugStringA("Can't load .OBJ file, check file path!\n");
	}
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

			// NOTE(georgy): Create RSM textures
			ID3D11Texture2D *ShadowMap;
			ID3D11DepthStencilView *ShadowMapDSV;
			ID3D11ShaderResourceView *ShadowMapSRV;

			D3D11_TEXTURE2D_DESC ShadowMapDescr;
			ShadowMapDescr.Width = Direct3D->WindowWidth;
			ShadowMapDescr.Height = Direct3D->WindowHeight;
			ShadowMapDescr.MipLevels = 1;
			ShadowMapDescr.ArraySize = 1;
			ShadowMapDescr.Format = DXGI_FORMAT_R32_TYPELESS;
			ShadowMapDescr.SampleDesc.Count = 1;
			ShadowMapDescr.SampleDesc.Quality = 0;
			ShadowMapDescr.Usage = D3D11_USAGE_DEFAULT;
			ShadowMapDescr.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
			ShadowMapDescr.CPUAccessFlags = 0;
			ShadowMapDescr.MiscFlags = 0;

			D3D11_DEPTH_STENCIL_VIEW_DESC ShadowMapDepthViewDescr;
			ShadowMapDepthViewDescr.Format = DXGI_FORMAT_D32_FLOAT;
			ShadowMapDepthViewDescr.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			ShadowMapDepthViewDescr.Flags = 0;
			ShadowMapDepthViewDescr.Texture2D.MipSlice = 0;

			D3D11_SHADER_RESOURCE_VIEW_DESC ShadowMapResourceViewDescr;
			ShadowMapResourceViewDescr.Format = DXGI_FORMAT_R32_FLOAT;
			ShadowMapResourceViewDescr.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			ShadowMapResourceViewDescr.Texture2D.MipLevels = 1;
			ShadowMapResourceViewDescr.Texture2D.MostDetailedMip = 0;

			Direct3D->Device->CreateTexture2D(&ShadowMapDescr, 0, &ShadowMap);
			Direct3D->Device->CreateDepthStencilView(ShadowMap, &ShadowMapDepthViewDescr, &ShadowMapDSV);
			Direct3D->Device->CreateShaderResourceView(ShadowMap, &ShadowMapResourceViewDescr, &ShadowMapSRV);


			ID3D11Texture2D *RSMWorldPosTexture;
			ID3D11RenderTargetView *RSMWorldPosRTV;
			ID3D11ShaderResourceView *RSMWorldPosSRV;

			D3D11_TEXTURE2D_DESC RSMWorldPosTextureDescr;
			RSMWorldPosTextureDescr.Width = Direct3D->WindowWidth;
			RSMWorldPosTextureDescr.Height = Direct3D->WindowHeight;
			RSMWorldPosTextureDescr.MipLevels = 1;
			RSMWorldPosTextureDescr.ArraySize = 1;
			RSMWorldPosTextureDescr.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			RSMWorldPosTextureDescr.SampleDesc.Count = 1;
			RSMWorldPosTextureDescr.SampleDesc.Quality = 0;
			RSMWorldPosTextureDescr.Usage = D3D11_USAGE_DEFAULT;
			RSMWorldPosTextureDescr.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			RSMWorldPosTextureDescr.CPUAccessFlags = 0;
			RSMWorldPosTextureDescr.MiscFlags = 0;

			Direct3D->Device->CreateTexture2D(&RSMWorldPosTextureDescr, 0, &RSMWorldPosTexture);
			Direct3D->Device->CreateRenderTargetView(RSMWorldPosTexture, 0, &RSMWorldPosRTV);
			Direct3D->Device->CreateShaderResourceView(RSMWorldPosTexture, 0, &RSMWorldPosSRV);


			ID3D11Texture2D *RSMNormalsTexture;
			ID3D11RenderTargetView *RSMNormalsRTV;
			ID3D11ShaderResourceView *RSMNormalsSRV;

			D3D11_TEXTURE2D_DESC RSMNormalsTextureDescr;
			RSMNormalsTextureDescr.Width = Direct3D->WindowWidth;
			RSMNormalsTextureDescr.Height = Direct3D->WindowHeight;
			RSMNormalsTextureDescr.MipLevels = 1;
			RSMNormalsTextureDescr.ArraySize = 1;
			RSMNormalsTextureDescr.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			RSMNormalsTextureDescr.SampleDesc.Count = 1;
			RSMNormalsTextureDescr.SampleDesc.Quality = 0;
			RSMNormalsTextureDescr.Usage = D3D11_USAGE_DEFAULT;
			RSMNormalsTextureDescr.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			RSMNormalsTextureDescr.CPUAccessFlags = 0;
			RSMNormalsTextureDescr.MiscFlags = 0;

			Direct3D->Device->CreateTexture2D(&RSMNormalsTextureDescr, 0, &RSMNormalsTexture);
			Direct3D->Device->CreateRenderTargetView(RSMNormalsTexture, 0, &RSMNormalsRTV);
			Direct3D->Device->CreateShaderResourceView(RSMNormalsTexture, 0, &RSMNormalsSRV);


			ID3D11Texture2D *FluxTexture;
			ID3D11RenderTargetView *FluxRTV;
			ID3D11ShaderResourceView *FluxSRV;

			D3D11_TEXTURE2D_DESC FluxTextureDescr;
			FluxTextureDescr.Width = Direct3D->WindowWidth;
			FluxTextureDescr.Height = Direct3D->WindowHeight;
			FluxTextureDescr.MipLevels = 1;
			FluxTextureDescr.ArraySize = 1;
			FluxTextureDescr.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			FluxTextureDescr.SampleDesc.Count = 1;
			FluxTextureDescr.SampleDesc.Quality = 0;
			FluxTextureDescr.Usage = D3D11_USAGE_DEFAULT;
			FluxTextureDescr.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			FluxTextureDescr.CPUAccessFlags = 0;
			FluxTextureDescr.MiscFlags = 0;

			Direct3D->Device->CreateTexture2D(&FluxTextureDescr, 0, &FluxTexture);
			Direct3D->Device->CreateRenderTargetView(FluxTexture, 0, &FluxRTV);
			Direct3D->Device->CreateShaderResourceView(FluxTexture, 0, &FluxSRV);


			// NOTE(georgy): GBuffer buffers

			ID3D11Texture2D *NormalsTexture;
			ID3D11RenderTargetView *NormalsRTV;
			ID3D11ShaderResourceView *NormalsSRV;

			D3D11_TEXTURE2D_DESC NormalsTextureDescr;
			NormalsTextureDescr.Width = Direct3D->WindowWidth;
			NormalsTextureDescr.Height = Direct3D->WindowHeight;
			NormalsTextureDescr.MipLevels = 1;
			NormalsTextureDescr.ArraySize = 1;
			NormalsTextureDescr.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			NormalsTextureDescr.SampleDesc.Count = 1;
			NormalsTextureDescr.SampleDesc.Quality = 0;
			NormalsTextureDescr.Usage = D3D11_USAGE_DEFAULT;
			NormalsTextureDescr.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			NormalsTextureDescr.CPUAccessFlags = 0;
			NormalsTextureDescr.MiscFlags = 0;

			Direct3D->Device->CreateTexture2D(&NormalsTextureDescr, 0, &NormalsTexture);
			Direct3D->Device->CreateRenderTargetView(NormalsTexture, 0, &NormalsRTV);
			Direct3D->Device->CreateShaderResourceView(NormalsTexture, 0, &NormalsSRV);

			// NOTE(georgy): RSMIndirectIllumTexture stores shadow factor in alpha channel
			ID3D11Texture2D *RSMIndirectIllumTexture;
			ID3D11RenderTargetView *RSMIndirectIllumRTV;
			ID3D11ShaderResourceView *RSMIndirectIllumSRV;
			
			D3D11_TEXTURE2D_DESC RSMIndirectIllumTextureDescr;
			RSMIndirectIllumTextureDescr.Width = Direct3D->WindowWidth;
			RSMIndirectIllumTextureDescr.Height = Direct3D->WindowHeight;
			RSMIndirectIllumTextureDescr.MipLevels = 1;
			RSMIndirectIllumTextureDescr.ArraySize = 1;
			RSMIndirectIllumTextureDescr.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			RSMIndirectIllumTextureDescr.SampleDesc.Count = 1;
			RSMIndirectIllumTextureDescr.SampleDesc.Quality = 0;
			RSMIndirectIllumTextureDescr.Usage = D3D11_USAGE_DEFAULT;
			RSMIndirectIllumTextureDescr.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			RSMIndirectIllumTextureDescr.CPUAccessFlags = 0;
			RSMIndirectIllumTextureDescr.MiscFlags = 0;

			Direct3D->Device->CreateTexture2D(&RSMIndirectIllumTextureDescr, 0, &RSMIndirectIllumTexture);
			Direct3D->Device->CreateRenderTargetView(RSMIndirectIllumTexture, 0, &RSMIndirectIllumRTV);
			Direct3D->Device->CreateShaderResourceView(RSMIndirectIllumTexture, 0, &RSMIndirectIllumSRV);


			ID3D11Texture2D *ColorTexture;
			ID3D11RenderTargetView *ColorRTV;
			ID3D11ShaderResourceView *ColorSRV;
			
			D3D11_TEXTURE2D_DESC ColorTextureDescr;
			ColorTextureDescr.Width = Direct3D->WindowWidth;
			ColorTextureDescr.Height = Direct3D->WindowHeight;
			ColorTextureDescr.MipLevels = 1;
			ColorTextureDescr.ArraySize = 1;
			ColorTextureDescr.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			ColorTextureDescr.SampleDesc.Count = 1;
			ColorTextureDescr.SampleDesc.Quality = 0;
			ColorTextureDescr.Usage = D3D11_USAGE_DEFAULT;
			ColorTextureDescr.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			ColorTextureDescr.CPUAccessFlags = 0;
			ColorTextureDescr.MiscFlags = 0;

			Direct3D->Device->CreateTexture2D(&ColorTextureDescr, 0, &ColorTexture);
			Direct3D->Device->CreateRenderTargetView(ColorTexture, 0, &ColorRTV);
			Direct3D->Device->CreateShaderResourceView(ColorTexture, 0, &ColorSRV);


			ID3D11Texture2D *RSMIndirectIllumAfterBlurTexture;
			ID3D11RenderTargetView *RSMIndirectIllumAfterBlurRTV;
			ID3D11ShaderResourceView *RSMIndirectIllumAfterBlurSRV;
			
			D3D11_TEXTURE2D_DESC RSMIndirectIllumAfterBlurTextureDescr;
			RSMIndirectIllumAfterBlurTextureDescr.Width = Direct3D->WindowWidth;
			RSMIndirectIllumAfterBlurTextureDescr.Height = Direct3D->WindowHeight;
			RSMIndirectIllumAfterBlurTextureDescr.MipLevels = 1;
			RSMIndirectIllumAfterBlurTextureDescr.ArraySize = 1;
			RSMIndirectIllumAfterBlurTextureDescr.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			RSMIndirectIllumAfterBlurTextureDescr.SampleDesc.Count = 1;
			RSMIndirectIllumAfterBlurTextureDescr.SampleDesc.Quality = 0;
			RSMIndirectIllumAfterBlurTextureDescr.Usage = D3D11_USAGE_DEFAULT;
			RSMIndirectIllumAfterBlurTextureDescr.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			RSMIndirectIllumAfterBlurTextureDescr.CPUAccessFlags = 0;
			RSMIndirectIllumAfterBlurTextureDescr.MiscFlags = 0;

			Direct3D->Device->CreateTexture2D(&RSMIndirectIllumAfterBlurTextureDescr, 0, &RSMIndirectIllumAfterBlurTexture);
			Direct3D->Device->CreateRenderTargetView(RSMIndirectIllumAfterBlurTexture, 0, &RSMIndirectIllumAfterBlurRTV);
			Direct3D->Device->CreateShaderResourceView(RSMIndirectIllumAfterBlurTexture, 0, &RSMIndirectIllumAfterBlurSRV);

			
			// NOTE(georgy): Generate samples for RSM
			std::uniform_real_distribution<float> RandomFloats(0.0f, 1.0f);
			std::default_random_engine Generator;
			v4 RSMSamples[256];
			for(uint32_t I = 0; I < ArrayCount(RSMSamples); I++)
			{
				v4 Sample;
				Sample.x = 2.0f*RandomFloats(Generator) - 1.0f;
				Sample.y = 2.0f*RandomFloats(Generator) - 1.0f;
				Sample.z = Sample.w = 0.0f;
				Sample.Normalize();

				float Scale = (float)I / ArrayCount(RSMSamples);
				Scale = Lerp(0.1f, 1.0f, Scale*Scale);
				Sample *= Scale;

				RSMSamples[I] = Sample;
			}

			v4 RSMNoise[16];
			for(uint32_t I = 0; I < ArrayCount(RSMNoise); I++)
			{
				v4 RandomVector = V4(0.0f, 0.0f, 0.0f, 0.0f);
				while(LengthSq(RandomVector) == 0.0f)
				{
					RandomVector.x = 2.0f*RandomFloats(Generator) - 1.0f;
					RandomVector.y = 2.0f*RandomFloats(Generator) - 1.0f;
					RandomVector.z = RandomVector.w = 0.0f;
				}
				RandomVector.Normalize();

				RSMNoise[I] = RandomVector;
			}

			// NOTE(georgy): Compile and create deferre shaders
			ID3D10Blob *VSBuffer;
			ID3D10Blob *PSBuffer;
			ID3D10Blob *CompilationMessages = 0;

			HRESULT ShaderCompileResult = D3DCompileFromFile(L"shaders/FullScreenQuadVS.hlsl", 0, 0, "VS", "vs_5_0", D3D10_SHADER_DEBUG | D3D10_SHADER_SKIP_OPTIMIZATION,
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

			ShaderCompileResult = D3DCompileFromFile(L"shaders/DeferredPS.hlsl", 0, 0, "PS", "ps_5_0", D3D10_SHADER_DEBUG | D3D10_SHADER_SKIP_OPTIMIZATION,
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


			// NOTE(georgy): Compile and create shadow map shaders
			ID3D10Blob *VSShadowMapBuffer;
			ID3D10Blob *PSShadowMapBuffer;

			ShaderCompileResult = D3DCompileFromFile(L"shaders/ShadowMapVS.hlsl", 0, 0, "VS", "vs_5_0", D3D10_SHADER_DEBUG | D3D10_SHADER_SKIP_OPTIMIZATION,
													 0, &VSShadowMapBuffer, &CompilationMessages);
			if(CompilationMessages)
			{
				MessageBox(0, (char *)CompilationMessages->GetBufferPointer(), 0, 0);
				CompilationMessages->Release();
			}
			if (FAILED(Hr))
			{
				MessageBox(0, "D3D11CompileFromFile failed", 0, 0);
			}

			ShaderCompileResult = D3DCompileFromFile(L"shaders/ShadowMapPS.hlsl", 0, 0, "PS", "ps_5_0", D3D10_SHADER_DEBUG | D3D10_SHADER_SKIP_OPTIMIZATION,
												     0, &PSShadowMapBuffer, &CompilationMessages);
			if(CompilationMessages)
			{
				MessageBox(0, (char *)CompilationMessages->GetBufferPointer(), 0, 0);
				CompilationMessages->Release();
			}
			if (FAILED(Hr))
			{
				MessageBox(0, "D3D11CompileFromFile failed", 0, 0);
			}

			ID3D11VertexShader *ShadowMapVS;
			ID3D11PixelShader *ShadowMapPS;
			Direct3D->Device->CreateVertexShader(VSShadowMapBuffer->GetBufferPointer(), VSShadowMapBuffer->GetBufferSize(), 0, &ShadowMapVS);
			Direct3D->Device->CreatePixelShader(PSShadowMapBuffer->GetBufferPointer(), PSShadowMapBuffer->GetBufferSize(), 0, &ShadowMapPS);

			// NOTE(georgy): Compile and create gbuffer shader
			ID3D10Blob *VSGBufferBuffer;
			ID3D10Blob *PSGBufferBuffer;

			ShaderCompileResult = D3DCompileFromFile(L"shaders/GBufferVS.hlsl", 0, 0, "VS", "vs_5_0", D3D10_SHADER_DEBUG | D3D10_SHADER_SKIP_OPTIMIZATION,
													 0, &VSGBufferBuffer, &CompilationMessages);
			if(CompilationMessages)
			{
				MessageBox(0, (char *)CompilationMessages->GetBufferPointer(), 0, 0);
				CompilationMessages->Release();
			}
			if (FAILED(Hr))
			{
				MessageBox(0, "D3D11CompileFromFile failed", 0, 0);
			}

			ShaderCompileResult = D3DCompileFromFile(L"shaders/GBufferPS.hlsl", 0, 0, "PS", "ps_5_0", D3D10_SHADER_DEBUG | D3D10_SHADER_SKIP_OPTIMIZATION,
												     0, &PSGBufferBuffer, &CompilationMessages);
			if(CompilationMessages)
			{
				MessageBox(0, (char *)CompilationMessages->GetBufferPointer(), 0, 0);
				CompilationMessages->Release();
			}
			if (FAILED(Hr))
			{
				MessageBox(0, "D3D11CompileFromFile failed", 0, 0);
			}

			ID3D11VertexShader *GBufferVS;
			ID3D11PixelShader *GBufferPS;
			Direct3D->Device->CreateVertexShader(VSGBufferBuffer->GetBufferPointer(), VSGBufferBuffer->GetBufferSize(), 0, &GBufferVS);
			Direct3D->Device->CreatePixelShader(PSGBufferBuffer->GetBufferPointer(), PSGBufferBuffer->GetBufferSize(), 0, &GBufferPS);


			// NOTE(georgy): Compile and create blur shader
			ID3D10Blob *PSBlurBuffer;

			ShaderCompileResult = D3DCompileFromFile(L"shaders/BlurPS.hlsl", 0, 0, "PS", "ps_5_0", D3D10_SHADER_DEBUG | D3D10_SHADER_SKIP_OPTIMIZATION,
												     0, &PSBlurBuffer, &CompilationMessages);
			if(CompilationMessages)
			{
				MessageBox(0, (char *)CompilationMessages->GetBufferPointer(), 0, 0);
				CompilationMessages->Release();
			}
			if (FAILED(Hr))
			{
				MessageBox(0, "D3D11CompileFromFile failed", 0, 0);
			}

			ID3D11PixelShader *BlurPS;
			Direct3D->Device->CreatePixelShader(PSBlurBuffer->GetBufferPointer(), PSBlurBuffer->GetBufferSize(), 0, &BlurPS);


			// NOTE(georgy): Create rasterizer state
			ID3D11RasterizerState *RasterizerState;
			
			D3D11_RASTERIZER_DESC RasterizerStateDescr;
  			RasterizerStateDescr.FillMode = D3D11_FILL_SOLID;
  			RasterizerStateDescr.CullMode = D3D11_CULL_NONE;
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


			ID3D11DepthStencilState *DepthAlwaysState;

			D3D11_DEPTH_STENCIL_DESC DepthAlwaysStateDescr;
			DepthAlwaysStateDescr.DepthEnable = false;
			DepthAlwaysStateDescr.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			DepthAlwaysStateDescr.DepthFunc = D3D11_COMPARISON_LESS;
			DepthAlwaysStateDescr.StencilEnable = false;
			DepthAlwaysStateDescr.StencilReadMask = 0;
			DepthAlwaysStateDescr.StencilWriteMask = 0;
			DepthAlwaysStateDescr.FrontFace = {};
			DepthAlwaysStateDescr.BackFace = {};

			Direct3D->Device->CreateDepthStencilState(&DepthAlwaysStateDescr, &DepthAlwaysState);

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

			// NOTE(georgy): Create vertex buffer for a quad

			v3 QuadVertices[] =
			{
				V3(-1.0f, -1.0f, 0.0f), V3(0.0f, 0.0f, -1.0f),
				V3(1.0f, -1.0f, 0.0f), V3(0.0f, 0.0f, -1.0f),
				V3(-1.0f, 1.0f, 0.0f), V3(0.0f, 0.0f, -1.0f),
				V3(1.0f, 1.0f, 0.0f), V3(0.0f, 0.0f, -1.0f)
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


			// NOTE(georgy): Create vertex buffer for a full screen quad

			v3 FullScreenQuadVertices[] =
			{
				V3(-1.0f, 1.0f, 0.0f),
				V3(-1.0f, -1.0f, 0.0f), 
				V3(1.0f, 1.0f, 0.0f), 
				V3(1.0f, -1.0f, 0.0f)
			};

			ID3D11Buffer *FullScreenQuadVertexBuffer;

			D3D11_BUFFER_DESC FullScreenQuadVertexBufferDescr;
			FullScreenQuadVertexBufferDescr.ByteWidth = sizeof(FullScreenQuadVertices);
			FullScreenQuadVertexBufferDescr.Usage = D3D11_USAGE_IMMUTABLE;
			FullScreenQuadVertexBufferDescr.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			FullScreenQuadVertexBufferDescr.CPUAccessFlags = 0;
			FullScreenQuadVertexBufferDescr.MiscFlags = 0;
			FullScreenQuadVertexBufferDescr.StructureByteStride = 0;

			D3D11_SUBRESOURCE_DATA FullScreenQuadVertexBufferInitData;
			FullScreenQuadVertexBufferInitData.pSysMem = FullScreenQuadVertices;
  			FullScreenQuadVertexBufferInitData.SysMemPitch = 0;
			FullScreenQuadVertexBufferInitData.SysMemSlicePitch = 0;

			Direct3D->Device->CreateBuffer(&FullScreenQuadVertexBufferDescr, &FullScreenQuadVertexBufferInitData, &FullScreenQuadVertexBuffer);


			// NOTE(georgy): Create input layout
			ID3D11InputLayout *InputLayout;
			D3D11_INPUT_ELEMENT_DESC InputLayoutDescription[] = 
			{
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 3*sizeof(float), D3D11_INPUT_PER_VERTEX_DATA, 0},
			};
			Direct3D->Device->CreateInputLayout(InputLayoutDescription, ArrayCount(InputLayoutDescription), 
												VSGBufferBuffer->GetBufferPointer(), VSGBufferBuffer->GetBufferSize(), &InputLayout);

			// NOTE(georgy): Create input layout
			ID3D11InputLayout *FullScreenQuadInputLayout;
			D3D11_INPUT_ELEMENT_DESC FullScreenQuadInputLayoutDescription[] = 
			{
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			};
			Direct3D->Device->CreateInputLayout(FullScreenQuadInputLayoutDescription, ArrayCount(FullScreenQuadInputLayoutDescription), 
												VSBuffer->GetBufferPointer(), VSBuffer->GetBufferSize(), &FullScreenQuadInputLayout);				

			VSBuffer->Release();
			PSBuffer->Release();
			VSShadowMapBuffer->Release();
			PSShadowMapBuffer->Release();
			VSGBufferBuffer->Release();
			PSGBufferBuffer->Release();

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

			// NOTE(georgy): Create constant buffer for light matrices
			ID3D11Buffer *LightMatrixBuffer;

			D3D11_BUFFER_DESC LightMatrixBufferDescr;
			LightMatrixBufferDescr.ByteWidth = sizeof(light_matrix_buffer);
			LightMatrixBufferDescr.Usage = D3D11_USAGE_DYNAMIC;
			LightMatrixBufferDescr.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			LightMatrixBufferDescr.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			LightMatrixBufferDescr.MiscFlags = 0;
			LightMatrixBufferDescr.StructureByteStride = 0;

			Direct3D->Device->CreateBuffer(&LightMatrixBufferDescr, 0, &LightMatrixBuffer);

			// NOTE(georgy): Create constant buffers for RSM samples and RSM noise
			ID3D11Buffer *RSMSamplesBuffer;
			
			D3D11_BUFFER_DESC RSMSamplesBufferDescr;
			RSMSamplesBufferDescr.ByteWidth = sizeof(RSMSamples);
			RSMSamplesBufferDescr.Usage = D3D11_USAGE_IMMUTABLE;
			RSMSamplesBufferDescr.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			RSMSamplesBufferDescr.CPUAccessFlags = 0;
			RSMSamplesBufferDescr.MiscFlags = 0;
			RSMSamplesBufferDescr.StructureByteStride = 0;

			D3D11_SUBRESOURCE_DATA RSMSamplesData;
			RSMSamplesData.pSysMem = RSMSamples;
			RSMSamplesData.SysMemPitch = 0;
			RSMSamplesData.SysMemSlicePitch = 0;

			Direct3D->Device->CreateBuffer(&RSMSamplesBufferDescr, &RSMSamplesData, &RSMSamplesBuffer);


			ID3D11Buffer *RSMNoiseBuffer;
			
			D3D11_BUFFER_DESC RSMNoiseBufferDescr;
			RSMNoiseBufferDescr.ByteWidth = sizeof(RSMNoise);
			RSMNoiseBufferDescr.Usage = D3D11_USAGE_IMMUTABLE;
			RSMNoiseBufferDescr.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			RSMNoiseBufferDescr.CPUAccessFlags = 0;
			RSMNoiseBufferDescr.MiscFlags = 0;
			RSMNoiseBufferDescr.StructureByteStride = 0;

			D3D11_SUBRESOURCE_DATA RSMSNoiseData;
			RSMSNoiseData.pSysMem = RSMNoise;
			RSMSNoiseData.SysMemPitch = 0;
			RSMSNoiseData.SysMemSlicePitch = 0;

			Direct3D->Device->CreateBuffer(&RSMNoiseBufferDescr, &RSMSNoiseData, &RSMNoiseBuffer);

			// NOTE(georgy): Create constant buffer for camera info
			ID3D11Buffer *ColorInfoBuffer;

			D3D11_BUFFER_DESC ColorInfoBufferDescr;
			ColorInfoBufferDescr.ByteWidth = sizeof(v4);
			ColorInfoBufferDescr.Usage = D3D11_USAGE_DYNAMIC;
			ColorInfoBufferDescr.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			ColorInfoBufferDescr.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			ColorInfoBufferDescr.MiscFlags = 0;
			ColorInfoBufferDescr.StructureByteStride = 0;

			Direct3D->Device->CreateBuffer(&ColorInfoBufferDescr, 0, &ColorInfoBuffer);

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
			SamplerDescr.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
			SamplerDescr.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
			SamplerDescr.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			SamplerDescr.MipLODBias = 0;
			SamplerDescr.MaxAnisotropy = 1;
			SamplerDescr.ComparisonFunc = D3D11_COMPARISON_NEVER;
			SamplerDescr.BorderColor[0] = SamplerDescr.BorderColor[1] = SamplerDescr.BorderColor[2] = SamplerDescr.BorderColor[3] = 0.0f;
			SamplerDescr.MinLOD = -D3D11_FLOAT32_MAX;
			SamplerDescr.MaxLOD = D3D11_FLOAT32_MAX;

			Direct3D->Device->CreateSamplerState(&SamplerDescr, &SamplerState);


			ID3D11SamplerState *ShadowMapSamplerState;
			D3D11_SAMPLER_DESC ShadowMapSamplerDescr;
			ShadowMapSamplerDescr.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			ShadowMapSamplerDescr.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
			ShadowMapSamplerDescr.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
			ShadowMapSamplerDescr.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			ShadowMapSamplerDescr.MipLODBias = 0;
			ShadowMapSamplerDescr.MaxAnisotropy = 1;
			ShadowMapSamplerDescr.ComparisonFunc = D3D11_COMPARISON_NEVER;
			ShadowMapSamplerDescr.MinLOD = -D3D11_FLOAT32_MAX;
			ShadowMapSamplerDescr.MaxLOD = D3D11_FLOAT32_MAX;

			Direct3D->Device->CreateSamplerState(&ShadowMapSamplerDescr, &ShadowMapSamplerState);



			// NOTE(georgy): Load bunny model
			model BunnyModel;
			std::vector<vertex> BunnyVertexArray;
			std::vector<uint32_t> BunnyIndexArray;
			InitializeSceneObjects("bunny.obj", BunnyModel, BunnyVertexArray, BunnyIndexArray);


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


			v3 CameraPos = V3(0.581630588f, 1.0f, -2.52652550f);
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

				// NOTE(georgy): Render to shadow map
				ID3D11RenderTargetView *RSMRenderTargets[] = {RSMWorldPosRTV, RSMNormalsRTV, FluxRTV};
				Direct3D->ImmediateContext->OMSetRenderTargets(3, RSMRenderTargets, ShadowMapDSV);
				Direct3D->ImmediateContext->ClearRenderTargetView(RSMRenderTargets[0], Colors::Black);
				Direct3D->ImmediateContext->ClearRenderTargetView(RSMRenderTargets[1], Colors::Black);
				Direct3D->ImmediateContext->ClearRenderTargetView(RSMRenderTargets[2], Colors::Black);
				Direct3D->ImmediateContext->ClearDepthStencilView(ShadowMapDSV, D3D11_CLEAR_DEPTH, 1.0f, 0.0f);

				Direct3D->ImmediateContext->OMSetDepthStencilState(DepthStencilState, 0);
				Direct3D->ImmediateContext->RSSetState(RasterizerState);
				Direct3D->ImmediateContext->OMSetBlendState(BlendState, 0, 0xFFFFFFFF);

				Direct3D->ImmediateContext->IASetInputLayout(InputLayout);
				Direct3D->ImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				Direct3D->ImmediateContext->VSSetShader(ShadowMapVS, 0, 0);
				Direct3D->ImmediateContext->PSSetShader(ShadowMapPS, 0, 0);

				D3D11_MAPPED_SUBRESOURCE MappedResource;
				Direct3D->ImmediateContext->Map(MatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
				matrix_buffer *MatrixBufferPtr = (matrix_buffer *)MappedResource.pData;
				MatrixBufferPtr->Model = Identity();
				MatrixBufferPtr->View = LookAt(V3(3.0f, 3.0f, -3.0f), V3(0.0f, 0.0f, 0.0f));
				MatrixBufferPtr->Projection = Orthographic(-2.5f, 2.5f, -2.5f, 2.5f, 3.5f, 10.0f);
				Direct3D->ImmediateContext->Unmap(MatrixBuffer, 0);
				Direct3D->ImmediateContext->VSSetConstantBuffers(0, 1, &MatrixBuffer);

				Direct3D->ImmediateContext->Map(ColorInfoBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
				v3 *ColorInfoPtr = (v3 *)MappedResource.pData;
				*ColorInfoPtr = 5.0f*V3(0.35, 0.35, 0.35);
				Direct3D->ImmediateContext->Unmap(ColorInfoBuffer, 0);
				Direct3D->ImmediateContext->PSSetConstantBuffers(1, 1, &ColorInfoBuffer);

				UINT Stride = sizeof(vertex);
				UINT Offset = 0;
				Direct3D->ImmediateContext->IASetVertexBuffers(0, 1, &BunnyModel.VertexBuffer, &Stride, &Offset);
				for(uint32_t MeshIndex = 0; MeshIndex < BunnyModel.Meshes.size(); MeshIndex++)
				{
					mesh *Mesh = &BunnyModel.Meshes[MeshIndex];

					Direct3D->ImmediateContext->IASetIndexBuffer(Mesh->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
					Direct3D->ImmediateContext->DrawIndexed(Mesh->IndexCount, 0, 0);
				}

				Direct3D->ImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				
				Stride = 2*sizeof(v3);
				Offset = 0;
				Direct3D->ImmediateContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);

				Direct3D->ImmediateContext->Map(MatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
				MatrixBufferPtr = (matrix_buffer *)MappedResource.pData;
				MatrixBufferPtr->Model = Translate(V3(0.0f, 1.0f, 1.0f));
				MatrixBufferPtr->View = LookAt(V3(3.0f, 3.0f, -3.0f), V3(0.0f, 0.0f, 0.0f));
				MatrixBufferPtr->Projection = Orthographic(-2.5f, 2.5f, -2.5f, 2.5f, 3.5f, 10.0f);
				Direct3D->ImmediateContext->Unmap(MatrixBuffer, 0);
				Direct3D->ImmediateContext->VSSetConstantBuffers(0, 1, &MatrixBuffer);

				Direct3D->ImmediateContext->Map(ColorInfoBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
				ColorInfoPtr = (v3 *)MappedResource.pData;
				*ColorInfoPtr = 5.0f*V3(0.0, 0.0, 0.75);
				Direct3D->ImmediateContext->Unmap(ColorInfoBuffer, 0);
				Direct3D->ImmediateContext->PSSetConstantBuffers(1, 1, &ColorInfoBuffer);
				
				Direct3D->ImmediateContext->Draw(4, 0);

				
				Direct3D->ImmediateContext->Map(MatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
				MatrixBufferPtr = (matrix_buffer *)MappedResource.pData;
				MatrixBufferPtr->Model = Rotate(90.0f, V3(0.0f, 1.0f, 0.0f)) * Translate(V3(-1.0f, 1.0f, 0.0f));
				MatrixBufferPtr->View = LookAt(V3(3.0f, 3.0f, -3.0f), V3(0.0f, 0.0f, 0.0f));
				MatrixBufferPtr->Projection = Orthographic(-2.5f, 2.5f, -2.5f, 2.5f, 3.5f, 10.0f);
				Direct3D->ImmediateContext->Unmap(MatrixBuffer, 0);
				Direct3D->ImmediateContext->VSSetConstantBuffers(0, 1, &MatrixBuffer);

				Direct3D->ImmediateContext->Map(ColorInfoBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
				ColorInfoPtr = (v3 *)MappedResource.pData;
				*ColorInfoPtr = 5.0f*V3(0.75, 0.0, 0.0);
				Direct3D->ImmediateContext->Unmap(ColorInfoBuffer, 0);
				Direct3D->ImmediateContext->PSSetConstantBuffers(1, 1, &ColorInfoBuffer);
				
				Direct3D->ImmediateContext->Draw(4, 0);

				
				Direct3D->ImmediateContext->Map(MatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
				MatrixBufferPtr = (matrix_buffer *)MappedResource.pData;
				MatrixBufferPtr->Model = Rotate(-90.0f, V3(1.0f, 0.0, 0.0f));
				MatrixBufferPtr->View = LookAt(V3(3.0f, 3.0f, -3.0f), V3(0.0f, 0.0f, 0.0f));
				MatrixBufferPtr->Projection = Orthographic(-2.5f, 2.5f, -2.5f, 2.5f, 3.5f, 10.0f);
				Direct3D->ImmediateContext->Unmap(MatrixBuffer, 0);
				Direct3D->ImmediateContext->VSSetConstantBuffers(0, 1, &MatrixBuffer);

				Direct3D->ImmediateContext->Map(ColorInfoBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
				ColorInfoPtr = (v3 *)MappedResource.pData;
				*ColorInfoPtr = 5.0f*V3(0.0, 0.75, 0.0);
				Direct3D->ImmediateContext->Unmap(ColorInfoBuffer, 0);
				Direct3D->ImmediateContext->PSSetConstantBuffers(1, 1, &ColorInfoBuffer);
				
				Direct3D->ImmediateContext->Draw(4, 0);


				// NOTE(georgy): Render to GBuffer
				ID3D11RenderTargetView *GBuffer[] = {NormalsRTV, RSMIndirectIllumRTV, ColorRTV};
				Direct3D->ImmediateContext->OMSetRenderTargets(3, GBuffer, Direct3D->DepthStencilView);
				Direct3D->ImmediateContext->ClearRenderTargetView(NormalsRTV, Colors::Black);
				Direct3D->ImmediateContext->ClearRenderTargetView(RSMIndirectIllumRTV, Colors::Black);
				Direct3D->ImmediateContext->ClearRenderTargetView(ColorRTV, Colors::Black);
				Direct3D->ImmediateContext->ClearDepthStencilView(Direct3D->DepthStencilView, 
																  D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0.0f);

				Direct3D->ImmediateContext->VSSetShader(GBufferVS, 0, 0);
				Direct3D->ImmediateContext->PSSetShader(GBufferPS, 0, 0);

				Direct3D->ImmediateContext->PSSetShaderResources(0, 1, &ShadowMapSRV);
				Direct3D->ImmediateContext->PSSetShaderResources(1, 1, &RSMWorldPosSRV);
				Direct3D->ImmediateContext->PSSetShaderResources(2, 1, &RSMNormalsSRV);
				Direct3D->ImmediateContext->PSSetShaderResources(3, 1, &FluxSRV);
				Direct3D->ImmediateContext->PSSetSamplers(0, 1, &SamplerState);
				Direct3D->ImmediateContext->PSSetSamplers(1, 1, &ShadowMapSamplerState);

				Direct3D->ImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				Direct3D->ImmediateContext->Map(LightMatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
				light_matrix_buffer *LightMatrixBufferPtr = (light_matrix_buffer *)MappedResource.pData;
				LightMatrixBufferPtr->View = LookAt(V3(3.0f, 3.0f, -3.0f), V3(0.0f, 0.0f, 0.0f));
				LightMatrixBufferPtr->Projection = Orthographic(-2.5f, 2.5f, -2.5f, 2.5f, 3.5f, 10.0f);
				Direct3D->ImmediateContext->Unmap(LightMatrixBuffer, 0);
				Direct3D->ImmediateContext->PSSetConstantBuffers(2, 1, &LightMatrixBuffer);

				Direct3D->ImmediateContext->PSSetConstantBuffers(3, 1, &RSMSamplesBuffer);
				Direct3D->ImmediateContext->PSSetConstantBuffers(4, 1, &RSMNoiseBuffer);
				
				Direct3D->ImmediateContext->Map(MatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
				MatrixBufferPtr = (matrix_buffer *)MappedResource.pData;
				MatrixBufferPtr->Model = Identity();
				MatrixBufferPtr->View = LookAt(CameraPos, CameraPos + CameraFront);
				MatrixBufferPtr->Projection = Perspective(45.0f, (real32)Direct3D->WindowWidth / (real32)Direct3D->WindowHeight, 0.1f, 100.0f);
				Direct3D->ImmediateContext->Unmap(MatrixBuffer, 0);
				Direct3D->ImmediateContext->VSSetConstantBuffers(0, 1, &MatrixBuffer);

				Direct3D->ImmediateContext->Map(ColorInfoBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
				ColorInfoPtr = (v3 *)MappedResource.pData;
				*ColorInfoPtr = 5.0f*V3(0.35, 0.35, 0.35);
				Direct3D->ImmediateContext->Unmap(ColorInfoBuffer, 0);
				Direct3D->ImmediateContext->PSSetConstantBuffers(1, 1, &ColorInfoBuffer);

				Stride = sizeof(vertex);
				Offset = 0;
				Direct3D->ImmediateContext->IASetVertexBuffers(0, 1, &BunnyModel.VertexBuffer, &Stride, &Offset);
				for(uint32_t MeshIndex = 0; MeshIndex < BunnyModel.Meshes.size(); MeshIndex++)
				{
					mesh *Mesh = &BunnyModel.Meshes[MeshIndex];

					Direct3D->ImmediateContext->IASetIndexBuffer(Mesh->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
					Direct3D->ImmediateContext->DrawIndexed(Mesh->IndexCount, 0, 0);
				}


				Direct3D->ImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				
				Stride = 2*sizeof(v3);
				Offset = 0;
				Direct3D->ImmediateContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);

				Direct3D->ImmediateContext->Map(MatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
				MatrixBufferPtr = (matrix_buffer *)MappedResource.pData;
				MatrixBufferPtr->Model = Translate(V3(0.0f, 1.0f, 1.0f));
				MatrixBufferPtr->View = LookAt(CameraPos, CameraPos + CameraFront);
				MatrixBufferPtr->Projection = Perspective(45.0f, (real32)Direct3D->WindowWidth / (real32)Direct3D->WindowHeight, 0.1f, 100.0f);
				Direct3D->ImmediateContext->Unmap(MatrixBuffer, 0);
				Direct3D->ImmediateContext->VSSetConstantBuffers(0, 1, &MatrixBuffer);

				Direct3D->ImmediateContext->Map(ColorInfoBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
				ColorInfoPtr = (v3 *)MappedResource.pData;
				*ColorInfoPtr = 5.0f*V3(0.0, 0.0, 0.75);
				Direct3D->ImmediateContext->Unmap(ColorInfoBuffer, 0);
				Direct3D->ImmediateContext->PSSetConstantBuffers(1, 1, &ColorInfoBuffer);
				
				Direct3D->ImmediateContext->Draw(4, 0);

				
				Direct3D->ImmediateContext->Map(MatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
				MatrixBufferPtr = (matrix_buffer *)MappedResource.pData;
				MatrixBufferPtr->Model = Rotate(90.0f, V3(0.0f, 1.0f, 0.0f)) * Translate(V3(-1.0f, 1.0f, 0.0f));
				MatrixBufferPtr->View = LookAt(CameraPos, CameraPos + CameraFront);
				MatrixBufferPtr->Projection = Perspective(45.0f, (real32)Direct3D->WindowWidth / (real32)Direct3D->WindowHeight, 0.1f, 100.0f);
				Direct3D->ImmediateContext->Unmap(MatrixBuffer, 0);
				Direct3D->ImmediateContext->VSSetConstantBuffers(0, 1, &MatrixBuffer);

				Direct3D->ImmediateContext->Map(ColorInfoBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
				ColorInfoPtr = (v3 *)MappedResource.pData;
				*ColorInfoPtr = 5.0f*V3(0.75, 0.0, 0.0);
				Direct3D->ImmediateContext->Unmap(ColorInfoBuffer, 0);
				Direct3D->ImmediateContext->PSSetConstantBuffers(1, 1, &ColorInfoBuffer);
				
				Direct3D->ImmediateContext->Draw(4, 0);


				Direct3D->ImmediateContext->Map(MatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
				MatrixBufferPtr = (matrix_buffer *)MappedResource.pData;
				MatrixBufferPtr->Model = Rotate(-90.0f, V3(1.0f, 0.0, 0.0f));
				MatrixBufferPtr->View = LookAt(CameraPos, CameraPos + CameraFront);
				MatrixBufferPtr->Projection = Perspective(45.0f, (real32)Direct3D->WindowWidth / (real32)Direct3D->WindowHeight, 0.1f, 100.0f);
				Direct3D->ImmediateContext->Unmap(MatrixBuffer, 0);
				Direct3D->ImmediateContext->VSSetConstantBuffers(0, 1, &MatrixBuffer);

				Direct3D->ImmediateContext->Map(ColorInfoBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
				ColorInfoPtr = (v3 *)MappedResource.pData;
				*ColorInfoPtr = 5.0f*V3(0.0, 0.75, 0.0);
				Direct3D->ImmediateContext->Unmap(ColorInfoBuffer, 0);
				Direct3D->ImmediateContext->PSSetConstantBuffers(1, 1, &ColorInfoBuffer);
				
				Direct3D->ImmediateContext->Draw(4, 0);

				ID3D11ShaderResourceView* NullSRVs[] = { nullptr, nullptr, nullptr, nullptr };
				Direct3D->ImmediateContext->PSSetShaderResources(0, 4, NullSRVs);

				
				// NOTE(georgy): Blur RSM indirect texture
				Direct3D->ImmediateContext->OMSetRenderTargets(1, &RSMIndirectIllumAfterBlurRTV, 0);
				Direct3D->ImmediateContext->ClearRenderTargetView(RSMIndirectIllumAfterBlurRTV, Colors::Black);

				Direct3D->ImmediateContext->IASetInputLayout(FullScreenQuadInputLayout);
				Direct3D->ImmediateContext->VSSetShader(VS, 0, 0);
				Direct3D->ImmediateContext->PSSetShader(BlurPS, 0, 0);

				Direct3D->ImmediateContext->OMSetDepthStencilState(DepthAlwaysState, 0);

				Stride = sizeof(v3); Offset = 0;
				Direct3D->ImmediateContext->IASetVertexBuffers(0, 1, &FullScreenQuadVertexBuffer, &Stride, &Offset);
				Direct3D->ImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

				Direct3D->ImmediateContext->PSSetShaderResources(0, 1, &RSMIndirectIllumSRV);

				Direct3D->ImmediateContext->Draw(4, 0);

				Direct3D->ImmediateContext->PSSetShaderResources(0, 1, NullSRVs);


				// NOTE(georgy): Render to backbuffer
				Direct3D->ImmediateContext->OMSetRenderTargets(1, &Direct3D->RenderTargetView, 0);
				Direct3D->ImmediateContext->ClearRenderTargetView(Direct3D->RenderTargetView, Colors::Black);

				Direct3D->ImmediateContext->IASetInputLayout(FullScreenQuadInputLayout);
				Direct3D->ImmediateContext->VSSetShader(VS, 0, 0);
				Direct3D->ImmediateContext->PSSetShader(PS, 0, 0);

				Direct3D->ImmediateContext->OMSetDepthStencilState(DepthAlwaysState, 0);

				Stride = sizeof(v3); Offset = 0;
				Direct3D->ImmediateContext->IASetVertexBuffers(0, 1, &FullScreenQuadVertexBuffer, &Stride, &Offset);
				Direct3D->ImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

				Direct3D->ImmediateContext->PSSetShaderResources(0, 1, &NormalsSRV);
				Direct3D->ImmediateContext->PSSetShaderResources(1, 1, &RSMIndirectIllumAfterBlurSRV);
				Direct3D->ImmediateContext->PSSetShaderResources(2, 1, &ColorSRV);
				Direct3D->ImmediateContext->PSSetSamplers(0, 1, &SamplerState);

				Direct3D->ImmediateContext->Draw(4, 0);

				Direct3D->ImmediateContext->PSSetShaderResources(0, 3, NullSRVs);


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
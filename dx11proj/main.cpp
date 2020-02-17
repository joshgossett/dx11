#include <Windows.h>
#include <wrl.h>
#include <assert.h>
#include <string>

#include <d3d11.h>
#include <DirectXMath.h>

using namespace DirectX;

#pragma comment(lib, "d3d11.lib")

using namespace Microsoft::WRL;

ComPtr<IDXGISwapChain> swapchain;
ComPtr<ID3D11Device> device;
ComPtr<ID3D11DeviceContext> devcon;
ComPtr<ID3D11RenderTargetView> rendertarget;
ComPtr<ID3D11DepthStencilView> depthview;
ComPtr<ID3D11Texture2D> depthbuffer;
ComPtr<ID3D11Buffer> v_buffer;
ComPtr<ID3D11Buffer> i_buffer;
ComPtr<ID3D11Buffer> c_buffer;
ComPtr<ID3D11VertexShader> vshader;
ComPtr<ID3D11PixelShader> pshader;
ComPtr<ID3D11InputLayout> input_layout;
ComPtr<ID3D11RasterizerState> raster_state;
ComPtr<ID3D11ShaderResourceView> texture_view;
ComPtr<ID3D11SamplerState> sampler_state;
ComPtr<ID3D11Texture2D> image_texture;

//timer variables
int64_t count_freq;
int64_t last_count;

//returns the fraction of a second since the last update(the last time the function was called)
double GetFractionalTimeStamp() {
	int64_t count;
	QueryPerformanceCounter((LARGE_INTEGER*)&count);
	auto dif = count - last_count;
	last_count = count;
	return dif / (double)count_freq;
}

void CreateShaderResourceViewFromTexture() {
	DXGI_SAMPLE_DESC sampledesc{};
	sampledesc.Count = 1;
	sampledesc.Quality = 0;

	D3D11_TEXTURE2D_DESC texdesc{};
	texdesc.ArraySize = 1;
	texdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	texdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texdesc.Height = 2;
	texdesc.Width = 2;
	texdesc.MipLevels = 1;
	texdesc.SampleDesc = sampledesc;
	texdesc.Usage = D3D11_USAGE_DYNAMIC;

	uint32_t texdata[4] = { 0xFFFF0000, 0xFFFFFFFF, 0xFF00FF00, 0xFF0000FF };
	D3D11_SUBRESOURCE_DATA texsubdata{};
	texsubdata.pSysMem = &texdata[0];
	texsubdata.SysMemPitch = sizeof(uint32_t) * 2;
	texsubdata.SysMemSlicePitch = 0;

	HRESULT hr = device->CreateTexture2D(&texdesc, &texsubdata, &image_texture);
	assert(SUCCEEDED(hr));

	hr = device->CreateShaderResourceView(image_texture.Get(), nullptr, &texture_view);
	assert(SUCCEEDED(hr));
}

#include "vs.h"
#include "ps.h"

//struct Vertex {
//	XMFLOAT4 pos;
//	XMFLOAT4 color;
//};

struct TexVec {
	XMFLOAT3 pos;
	XMFLOAT2 texCoords;
};

struct CBuffer {
	XMMATRIX WVP;
};

CBuffer constbuff{ XMMatrixIdentity() };
XMMATRIX World{};
XMMATRIX View{};
XMMATRIX Projection{};

void InitD3D(HINSTANCE inst, HWND wnd, int width, int height) {
	HRESULT hr;

	DXGI_MODE_DESC bufferdesc{ sizeof(DXGI_MODE_DESC) };

	bufferdesc.Width = width;
	bufferdesc.Height = height;
	bufferdesc.RefreshRate.Numerator = 60;
	bufferdesc.RefreshRate.Denominator = 1;
	bufferdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	bufferdesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	bufferdesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	DXGI_SWAP_CHAIN_DESC swapdesc{ sizeof(DXGI_SWAP_CHAIN_DESC) };

	swapdesc.BufferDesc = bufferdesc;
	swapdesc.SampleDesc.Count = 1;
	swapdesc.SampleDesc.Quality = 0;
	swapdesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapdesc.BufferCount = 1;
	swapdesc.OutputWindow = wnd;
	swapdesc.Windowed = TRUE;
	swapdesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	D3D_FEATURE_LEVEL flevels[] = { D3D_FEATURE_LEVEL_11_1 };
	hr = D3D11CreateDeviceAndSwapChain(0, D3D_DRIVER_TYPE_HARDWARE, 0, 0, flevels, 1, D3D11_SDK_VERSION, &swapdesc, &swapchain, &device, 0, &devcon);
	assert(hr == S_OK);

	ComPtr<ID3D11Texture2D> BackBuffer;
	hr = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), &BackBuffer);
	assert(hr == S_OK);

	hr = device->CreateRenderTargetView((ID3D11Resource*)BackBuffer.Get(), 0, &rendertarget);
	assert(hr == S_OK);

	D3D11_TEXTURE2D_DESC depth_stencil_desc;
	depth_stencil_desc.Width = width;
	depth_stencil_desc.Height = height;
	depth_stencil_desc.MipLevels = 1;
	depth_stencil_desc.ArraySize = 1;
	depth_stencil_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depth_stencil_desc.SampleDesc.Count = 1;
	depth_stencil_desc.SampleDesc.Quality = 0;
	depth_stencil_desc.Usage = D3D11_USAGE_DEFAULT;
	depth_stencil_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depth_stencil_desc.CPUAccessFlags = 0;
	depth_stencil_desc.MiscFlags = 0;

	hr = device->CreateTexture2D(&depth_stencil_desc, nullptr, &depthbuffer);
	assert(hr == S_OK);
	hr = device->CreateDepthStencilView(depthbuffer.Get(), nullptr, &depthview);
	assert(hr == S_OK);

	devcon->OMSetRenderTargets(1, rendertarget.GetAddressOf(), depthview.Get());

	World = XMMatrixIdentity();
	XMVECTOR eye = XMVectorSet(0, 0, -1, 0);
	XMVECTOR lookat = XMVectorSet(0, 0, 0, 0);
	XMVECTOR up = XMVectorSet(0, 1, 0, 0);
	View = XMMatrixLookAtLH(eye, lookat, up);
	Projection = XMMatrixOrthographicLH(width, height, 0, 1);
}

void InitScene(int width, int height) {
	D3D11_INPUT_ELEMENT_DESC input_desc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	HRESULT hr = 1;
	hr = device->CreateVertexShader(vert_shader, ARRAYSIZE(vert_shader), nullptr, &vshader);
	assert(hr == S_OK);
	hr = device->CreatePixelShader(pix_shader, ARRAYSIZE(pix_shader), nullptr, &pshader);
	assert(hr == S_OK);

	devcon->VSSetShader(vshader.Get(), nullptr, 0);
	devcon->PSSetShader(pshader.Get(), nullptr, 0);

	TexVec v[] =
	{
		{ XMFLOAT3(-300.0f, 200.0f, 0.5f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(300.0f, 200.0f, 0.5f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(300.0f, -200.0f, 0.5f), XMFLOAT2(1.0f, 1.0f) },
		{ XMFLOAT3(-300.0f, -200.0f, 0.5f), XMFLOAT2(0.0f, 1.0f) }
	};

	int i[] =
	{
		0,1,2,
		0,2,3
	};

	D3D11_BUFFER_DESC vbuff_desc{};
	vbuff_desc.Usage = D3D11_USAGE_DEFAULT;
	vbuff_desc.ByteWidth = sizeof(TexVec) * ARRAYSIZE(v);
	vbuff_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbuff_desc.CPUAccessFlags = 0;
	vbuff_desc.MiscFlags = 0;

	D3D11_BUFFER_DESC ibuff_desc{};
	ibuff_desc.Usage = D3D11_USAGE_DEFAULT;
	ibuff_desc.ByteWidth = sizeof(int) * ARRAYSIZE(i);
	ibuff_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibuff_desc.CPUAccessFlags = 0;
	ibuff_desc.MiscFlags = 0;

	D3D11_BUFFER_DESC cbuff_desc{};
	cbuff_desc.Usage = D3D11_USAGE_DEFAULT;
	cbuff_desc.ByteWidth = sizeof(CBuffer);
	cbuff_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbuff_desc.CPUAccessFlags = 0;
	cbuff_desc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA vbuff_data{};
	vbuff_data.pSysMem = v;
	hr = device->CreateBuffer(&vbuff_desc, &vbuff_data, &v_buffer);
	assert(hr == S_OK);

	D3D11_SUBRESOURCE_DATA ibuff_data{};
	ibuff_data.pSysMem = i;
	hr = device->CreateBuffer(&ibuff_desc, &ibuff_data, &i_buffer);
	assert(hr == S_OK);

	hr = device->CreateBuffer(&cbuff_desc, nullptr, &c_buffer);
	assert(hr == S_OK);

	UINT stride = sizeof(TexVec);
	UINT offset = 0;
	devcon->IASetVertexBuffers(0, 1, v_buffer.GetAddressOf(), &stride, &offset);

	devcon->IASetIndexBuffer(i_buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

	devcon->VSSetConstantBuffers(0, 1, c_buffer.GetAddressOf());

	hr = device->CreateInputLayout(input_desc, ARRAYSIZE(input_desc), vert_shader, ARRAYSIZE(vert_shader), &input_layout);
	assert(hr == S_OK);

	devcon->IASetInputLayout(input_layout.Get());

	CreateShaderResourceViewFromTexture();

	D3D11_SAMPLER_DESC sampDesc{};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	hr = device->CreateSamplerState(&sampDesc, sampler_state.GetAddressOf());
	assert(SUCCEEDED(hr));

	devcon->PSSetShaderResources(0, 1, texture_view.GetAddressOf());
	devcon->PSSetSamplers(0, 1, sampler_state.GetAddressOf());

	devcon->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D11_RASTERIZER_DESC raster_desc{};
	raster_desc.FillMode = D3D11_FILL_SOLID;
	raster_desc.CullMode = D3D11_CULL_BACK;
	raster_desc.FrontCounterClockwise = false;
	raster_desc.DepthBias = 0;
	raster_desc.SlopeScaledDepthBias = 0;
	raster_desc.DepthClipEnable = true;
	raster_desc.ScissorEnable = false;
	raster_desc.MultisampleEnable = true;
	raster_desc.AntialiasedLineEnable = true;
	device->CreateRasterizerState(&raster_desc, &raster_state);
	devcon->RSSetState(raster_state.Get());

	D3D11_VIEWPORT viewport{};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = (float)width;
	viewport.Height = (float)height;
	viewport.MinDepth = 0.0;
	viewport.MaxDepth = 1.0;
	devcon->RSSetViewports(1, &viewport);
}

float angle = 0;

void UpdateScene() {
	double deltaT = GetFractionalTimeStamp();
	World = XMMatrixRotationZ(angle);
	angle += XMConvertToRadians(90.0f * deltaT);// one rotation every 4 seconds
	constbuff.WVP = Projection * View * World;
	devcon->UpdateSubresource(c_buffer.Get(), 0, nullptr, &constbuff, 0, 0);
}

void DrawScene() {
	XMFLOAT4 bgColor{ 0.1f, 0.1f, 0.1f, 1.0f };

	devcon->ClearRenderTargetView(rendertarget.Get(), &bgColor.x);
	devcon->ClearDepthStencilView(depthview.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	devcon->DrawIndexed(6, 0, 0);

	swapchain->Present(1, 0);
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	RECT size{};
	char msg_buff[128];
	sprintf_s(msg_buff, "MESSAGE: %d \n", msg);
	OutputDebugStringA(msg_buff);
	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_SIZE:
		if (!(wp == SIZE_MAXIMIZED))
			break;
	case WM_EXITSIZEMOVE:
		GetClientRect(hwnd, &size);
		InitD3D(GetModuleHandle(0), hwnd, size.right - size.left, size.bottom - size.top);
		InitScene(size.right - size.left, size.bottom - size.top);
		OutputDebugStringA("Reinitialized\n");
		return DefWindowProc(hwnd, msg, wp, lp);
	case WM_PAINT:
		GetClientRect(hwnd, &size);
		InitD3D(GetModuleHandle(0), hwnd, size.right - size.left, size.bottom - size.top);
		InitScene(size.right - size.left, size.bottom - size.top);
		OutputDebugStringA("Reinitialized\n");
		UpdateScene();
		DrawScene();
		break;
	default:
		break;
	}
	return DefWindowProc(hwnd, msg, wp, lp);
}

HWND InitWindow(HINSTANCE inst, int width, int height, LPCSTR title)
{
	WNDCLASSEX wndex{ sizeof(WNDCLASSEX) };
	wndex.lpfnWndProc = wndProc;
	wndex.lpszClassName = TEXT("DX11CLASS");
	wndex.style = CS_VREDRAW | CS_HREDRAW;
	wndex.hInstance = inst;
	wndex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
	wndex.hCursor = LoadCursor(0, IDC_ARROW);
	wndex.hIcon = LoadIcon(0, IDI_APPLICATION);
	wndex.hIconSm = LoadIcon(0, IDI_APPLICATION);
	ATOM cs = RegisterClassEx(&wndex);
	assert(cs);

	RECT winsize{ 0,0, width, height };
	AdjustWindowRect(&winsize, WS_OVERLAPPEDWINDOW, false);

	HWND hwnd = CreateWindowEx(WS_EX_APPWINDOW, TEXT("DX11CLASS"), title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, winsize.right - winsize.left, winsize.bottom - winsize.top, nullptr, nullptr, nullptr, 0);
	assert(hwnd);
	return hwnd;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmd, int show) {
	auto s1 = sizeof(LARGE_INTEGER);
	auto s2 = sizeof(int64_t);
	assert(s1 == s2);
	QueryPerformanceFrequency((LARGE_INTEGER*)&count_freq);
	QueryPerformanceCounter((LARGE_INTEGER*)&last_count);
	HWND wnd = InitWindow(inst, 600, 400, TEXT("DX 11 Window"));
	RECT size;
	GetClientRect(wnd, &size);
	int w = size.right - size.left;
	int h = size.bottom - size.top;
	InitD3D(inst, wnd, w, h);
	InitScene(w, h);
	ShowWindow(wnd, SW_SHOW);
	UpdateWindow(wnd);

	while (true) {
		MSG msg;
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				break;
			}
			DispatchMessage(&msg);
		}
		UpdateScene();
		DrawScene();
	}
}
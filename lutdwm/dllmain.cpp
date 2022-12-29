// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

#include <fstream>
#pragma comment (lib, "d3d11.lib") // Maybe un-useful
#pragma comment (lib, "d3dcompiler.lib")
#pragma comment (lib, "dxgi.lib") // Maybe un-useful
#pragma comment (lib, "uuid.lib") // Maybe un-useful
#pragma comment (lib, "dxguid.lib")

#define DITHER_GAMMA 2.2
#define LUT_FOLDER "%SYSTEMROOT%\\Temp\\luts"

#define RELEASE_IF_NOT_NULL(x) { if (x != NULL) { x->Release(); } }
#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)
#define RESIZE(x, y) realloc(x, (y) * sizeof(*x));
#define LOG_FILE_PATH R"(C:\Users\Authority\source\repos\lutdwm\x64\Debug\dwm.log)"
#define MAX_LOG_FILE_SIZE 20000000


unsigned int lut_index(const unsigned int b, const unsigned int g, const unsigned int r, const unsigned int c, const unsigned int lut_size)
{
	return lut_size * lut_size * 4 * b + lut_size * 4 * g + 4 * r + c;
}

#define LUT_ACCESS_INDEX(lut, b, g, r, c, lut_size) (*((float*)(lut) + lut_index(b, g, r, c, lut_size)))

#pragma intrinsic(_ReturnAddress)

const unsigned char COverlayContext_Present_bytes[] = {
	0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48, 0x83, 0xec, 0x40, 0x48, 0x8b, 0xb1, 0x20,
	0x2c, 0x00, 0x00, 0x45, 0x8b, 0xd0, 0x48, 0x8b, 0xfa, 0x48, 0x8b, 0xd9, 0x48, 0x85, 0xf6, 0x0f, 0x85
};
const int IOverlaySwapChain_IDXGISwapChain_offset = -0x118;

const unsigned char COverlayContext_IsCandidateDirectFlipCompatbile_bytes[] = {
	0x48, 0x89, 0x7c, 0x24, 0x20, 0x55, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8b, 0xec, 0x48, 0x83,
	0xec, 0x40
};
const unsigned char COverlayContext_OverlaysEnabled_bytes[] = {
	0x75, 0x04, 0x32, 0xc0, 0xc3, 0xcc, 0x83, 0x79, 0x30, 0x01, 0x0f, 0x97, 0xc0, 0xc3
};

const int COverlayContext_DeviceClipBox_offset = -0x120;

const int IOverlaySwapChain_HardwareProtected_offset = -0xbc;

/*
 * AOB for function: COverlayContext_Present_bytes_w11
 *
 * 40 53 55 56 57 41 56 41 57 48 81 EC 88 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 44 24 78 48
 *
 */
const unsigned char COverlayContext_Present_bytes_w11[] = {
	0x40, 0x53, 0x55, 0x56, 0x57, 0x41, 0x56, 0x41, 0x57, 0x48, 0x81, 0xEC, 0x88, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x05,
	'?', '?', '?', '?', 0x48, 0x33, 0xC4, 0x48, 0x89, 0x44, 0x24, 0x78, 0x48
};
const int IOverlaySwapChain_IDXGISwapChain_offset_w11 = 0xE0;

/*
 * AOB for function: COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11
 *
 * 40 55 53 56 57 41 54 41 55 41 56 41 57 48 8B EC 48 83 EC 68 48
 */
const unsigned char COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11[] = {
	0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8B, 0xEC, 0x48, 0x83, 0xEC,
	0x68, 0x48,
};

/*
 * AOB for function: COverlayContext_OverlaysEnabled_bytes_w11
 *
 * 83 3D ?? ?? ?? ?? ?? 75 04
 */
const unsigned char COverlayContext_OverlaysEnabled_bytes_w11[] = {
	0x83, 0x3D, '?', '?', '?', '?', '?', 0x75, 0x04
};

int COverlayContext_DeviceClipBox_offset_w11 = 0x466C;

const int IOverlaySwapChain_HardwareProtected_offset_w11 = -0x12C;

bool isWindows11;

bool aob_match_inverse(const void* buf1, const void* mask, const int buf_len)
{
	for (int i = 0; i < buf_len; ++i)
	{
		if (((unsigned char*)buf1)[i] != ((unsigned char*)mask)[i] && ((unsigned char*)mask)[i] != '?')
		{
			return true;
		}
	}
	return false;
}

char shaders[] = STRINGIFY((
    struct VS_INPUT {
    float2 pos : POSITION;
    float2 tex : TEXCOORD;
};

struct VS_OUTPUT {
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD;
};

Texture2D backBufferTex : register(t0);
Texture3D lutTex : register(t1);
SamplerState smp : register(s0);

Texture2D noiseTex : register(t2);
SamplerState noiseSmp : register(s1);

int lutSize : register(b0);
bool hdr : register(b0);

static float3x3 scrgb_to_bt2100 = {
2939026994.L / 585553224375.L, 9255011753.L / 3513319346250.L,   173911579.L / 501902763750.L,
  76515593.L / 138420033750.L, 6109575001.L / 830520202500.L,    75493061.L / 830520202500.L,
  12225392.L / 93230009375.L, 1772384008.L / 2517210253125.L, 18035212433.L / 2517210253125.L,
};

static float3x3 bt2100_to_scrgb = {
 348196442125.L / 1677558947.L, -123225331250.L / 1677558947.L,  -15276242500.L / 1677558947.L,
-579752563250.L / 37238079773.L, 5273377093000.L / 37238079773.L,  -38864558125.L / 37238079773.L,
 -12183628000.L / 5369968309.L, -472592308000.L / 37589778163.L, 5256599974375.L / 37589778163.L,
};

static float m1 = 1305 / 8192.;
static float m2 = 2523 / 32.;
static float c1 = 107 / 128.;
static float c2 = 2413 / 128.;
static float c3 = 2392 / 128.;

float3 SampleLut(float3 index) {
    float3 tex = (index + 0.5) / lutSize;
    return lutTex.Sample(smp, tex).rgb;
}

// adapted from https://doi.org/10.2312/egp.20211031
void barycentricWeight(float3 r, out float4 bary, out int3 vert2, out int3 vert3) {
    vert2 = int3(0, 0, 0); vert3 = int3(1, 1, 1);
    int3 c = r.xyz >= r.yzx;
    bool c_xy = c.x; bool c_yz = c.y; bool c_zx = c.z;
    bool c_yx = !c.x; bool c_zy = !c.y; bool c_xz = !c.z;
    bool cond;  float3 s = float3(0, 0, 0);
#define ORDER(X, Y, Z)                   \
            cond = c_ ## X ## Y && c_ ## Y ## Z; \
            s = cond ? r.X ## Y ## Z : s;        \
            vert2.X = cond ? 1 : vert2.X;        \
            vert3.Z = cond ? 0 : vert3.Z;
    ORDER(x, y, z)   ORDER(x, z, y)   ORDER(z, x, y)
        ORDER(z, y, x)   ORDER(y, z, x)   ORDER(y, x, z)
        bary = float4(1 - s.x, s.z, s.x - s.y, s.y - s.z);
}

float3 LutTransformTetrahedral(float3 rgb) {
    float3 lutIndex = rgb * (lutSize - 1);
    float4 bary; int3 vert2; int3 vert3;
    barycentricWeight(frac(lutIndex), bary, vert2, vert3);

    float3 base = floor(lutIndex);
    return bary.x * SampleLut(base) +
        bary.y * SampleLut(base + 1) +
        bary.z * SampleLut(base + vert2) +
        bary.w * SampleLut(base + vert3);
}

float3 pq_eotf(float3 e) {
    return pow(max((pow(e, 1 / m2) - c1), 0) / (c2 - c3 * pow(e, 1 / m2)), 1 / m1);
}

float3 pq_inv_eotf(float3 y) {
    return pow((c1 + c2 * pow(y, m1)) / (1 + c3 * pow(y, m1)), m2);
}

float3 OrderedDither(float3 rgb, float2 pos) {
    float3 low = floor(rgb * 255) / 255;
    float3 high = low + 1.0 / 255;

    float3 rgb_linear = pow(rgb, DITHER_GAMMA);
    float3 low_linear = pow(low, DITHER_GAMMA);
    float3 high_linear = pow(high, DITHER_GAMMA);

    float noise = noiseTex.Sample(noiseSmp, pos / NOISE_SIZE).x;
    float3 threshold = lerp(low_linear, high_linear, noise);

    return lerp(low, high, rgb_linear > threshold);
}

VS_OUTPUT VS(VS_INPUT input) {
    VS_OUTPUT output;
    output.pos = float4(input.pos, 0, 1);
    output.tex = input.tex;
    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET{
    float3 sample = backBufferTex.Sample(smp, input.tex).rgb;

    if (hdr) {
        float3 hdr10_sample = pq_inv_eotf(saturate(mul(scrgb_to_bt2100, sample)));

        float3 hdr10_res = LutTransformTetrahedral(hdr10_sample);

        float3 scrgb_res = mul(bt2100_to_scrgb, pq_eotf(hdr10_res));

        return float4(scrgb_res, 1);
    }
    else {
        float3 res = LutTransformTetrahedral(sample);

        res = OrderedDither(res, input.pos.xy);

        return float4(res, 1);
    }
}
));

ID3D11Device* device;
ID3D11DeviceContext* deviceContext;
ID3D11VertexShader* vertexShader;
ID3D11PixelShader* pixelShader;
ID3D11InputLayout* inputLayout;

ID3D11Buffer* vertexBuffer;
UINT numVerts;
UINT stride;
UINT offset;

D3D11_TEXTURE2D_DESC backBufferDesc;
D3D11_TEXTURE2D_DESC textureDesc[2];

ID3D11SamplerState* samplerState;
ID3D11Texture2D* texture[2];
ID3D11ShaderResourceView* textureView[2];

ID3D11SamplerState* noiseSamplerState;
ID3D11ShaderResourceView* noiseTextureView;

ID3D11Buffer* constantBuffer;

struct lutData
{
	int left;
	int top;
	int size;
	bool isHdr;
	ID3D11ShaderResourceView* textureView;
	float* rawLut;
};

void DrawRectangle(struct tagRECT* rect, int index)
{
	float width = backBufferDesc.Width;
	float height = backBufferDesc.Height;

	float screenLeft = rect->left / width;
	float screenTop = rect->top / height;
	float screenRight = rect->right / width;
	float screenBottom = rect->bottom / height;

	float left = screenLeft * 2 - 1;
	float top = screenTop * -2 + 1;
	float right = screenRight * 2 - 1;
	float bottom = screenBottom * -2 + 1;

	width = textureDesc[index].Width;
	height = textureDesc[index].Height;
	float texLeft = rect->left / width;
	float texTop = rect->top / height;
	float texRight = rect->right / width;
	float texBottom = rect->bottom / height;

	float vertexData[] = {
		left, bottom, texLeft, texBottom,
		left, top, texLeft, texTop,
		right, bottom, texRight, texBottom,
		right, top, texRight, texTop
	};

	D3D11_MAPPED_SUBRESOURCE resource;
	deviceContext->Map( vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
	memcpy(resource.pData, vertexData, stride * numVerts);
	deviceContext->Unmap( vertexBuffer, 0);

	deviceContext->IASetVertexBuffers( 0, 1, &vertexBuffer, &stride, &offset);

	deviceContext->Draw( numVerts, 0);
}

int numLuts;

lutData* luts;

bool ParseLUT(lutData* lut, char* filename)
{
	FILE* file = fopen(filename, "r");
	if (file == NULL) return false;

	char line[256];
	unsigned int lutSize;

	while (1)
	{
		if (!fgets(line, sizeof(line), file))
		{
			fclose(file);
			return false;
		}
		if (sscanf(line, "LUT_3D_SIZE%d", &lutSize) == 1)
		{
			break;
		}
	}
	// borgaccio
	float* rawLut = (float*)malloc(lutSize * lutSize * lutSize * 4 * sizeof(float));
	// lut_3d_vec rawLut(lutSize, { lutSize, {lutSize, RGBA_VEC} });

	for (int b = 0; b < lutSize; b++)
	{
		for (int g = 0; g < lutSize; g++)
		{
			for (int r = 0; r < lutSize; r++)
			{
				while (1)
				{
					if (!fgets(line, sizeof(line), file))
					{
						fclose(file);
						// free(rawLut);
						return false;
					}
					if (line[0] <= '9' && line[0] != '#' && line[0] != '\n')
					{
						float red, green, blue;

						if (sscanf(line, "%f%f%f", &red, &green, &blue) != 3)
						{
							fclose(file);
							// free(rawLut);
							return false;
						}
						LUT_ACCESS_INDEX(rawLut, b, g, r, 0, lutSize) = red;
						LUT_ACCESS_INDEX(rawLut, b, g, r, 1, lutSize) = green;
						LUT_ACCESS_INDEX(rawLut, b, g, r, 2, lutSize) = blue;
						LUT_ACCESS_INDEX(rawLut, b, g, r, 3, lutSize) = 1;

						break;
					}
				}
			}
		}
	}
	fclose(file);
	lut->size = lutSize;
	lut->rawLut = rawLut;
	return true;
}

bool AddLUTs(char* folder)
{
	WIN32_FIND_DATAA findData;

	char path[MAX_PATH];
	strcpy(path, folder);
	strcat(path, "\\*");
	HANDLE hFind = FindFirstFileA(path, &findData);
	if (hFind == INVALID_HANDLE_VALUE) return false;
	do
	{
		if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			char filePath[MAX_PATH];
			char* fileName = findData.cFileName;

			strcpy(filePath, folder);
			strcat(filePath, "\\");
			strcat(filePath, fileName);

			luts = (lutData*)RESIZE(luts, numLuts + 1)
			lutData* lut = &luts[numLuts];
			if (sscanf(findData.cFileName, "%d_%d", &lut->left, &lut->top) == 2)
			{
				lut->isHdr = strstr(fileName, "hdr") != NULL;
				lut->textureView = NULL;
				if (!ParseLUT(lut, filePath))
				{
					// TODO: Remove this debug instruction
					MessageBoxA(NULL, "LUT could not be parsed", "DEBUG HOOK DWM", MB_OK | MB_ICONWARNING);
					FindClose(hFind);
					return false;
				}
				numLuts++;
			}
		}
	}
	while (FindNextFileA(hFind, &findData) != 0);
	FindClose(hFind);
	return true;
}

int numLutTargets;
void** lutTargets;

bool IsLUTActive(void* target)
{
	for (int i = 0; i < numLutTargets; i++)
	{
		if (lutTargets[i] == target)
		{
			return true;
		}
	}
	return false;
}

void SetLUTActive(void* target)
{
	if (!IsLUTActive(target))
	{
		lutTargets = (void**)RESIZE(lutTargets, numLutTargets + 1)
		lutTargets[numLutTargets++] = target;
	}
}

void UnsetLUTActive(void* target)
{
	for (int i = 0; i < numLutTargets; i++)
	{
		if (lutTargets[i] == target)
		{
			lutTargets[i] = lutTargets[--numLutTargets];
			lutTargets = (void**)RESIZE(lutTargets, numLutTargets)
			return;
		}
	}
}

lutData* GetLUTDataFromCOverlayContext(void* context, bool hdr)
{
	int left, top;
	if (isWindows11)
	{
		float* rect = (float*)((unsigned char*)*(void**)context + COverlayContext_DeviceClipBox_offset_w11);
		left = (int)rect[0];
		top = (int)rect[1];
	}
	else
	{
		int* rect = (int*)((unsigned char*)context + COverlayContext_DeviceClipBox_offset);
		left = rect[0];
		top = rect[1];
	}

	for (int i = 0; i < numLuts; i++)
	{
		if (luts[i].left == left && luts[i].top == top && luts[i].isHdr == hdr)
		{
			return &luts[i];
		}
	}
	return NULL;
}

void InitializeStuff(IDXGISwapChain* swapChain)
{
	swapChain->GetDevice(IID_ID3D11Device, (void**)&device);
	device->GetImmediateContext(&deviceContext);
	{
		ID3DBlob* vsBlob;
		D3DCompile(shaders + 1, sizeof(shaders) - 3, NULL, NULL, NULL, "VS", "vs_5_0", 0, 0, &vsBlob, NULL);
		device->CreateVertexShader( vsBlob->GetBufferPointer(),
		                                   vsBlob->GetBufferSize(), NULL, &vertexShader);
		D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
		{
			{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
		};

		device->CreateInputLayout(inputElementDesc, ARRAYSIZE(inputElementDesc),
		                                  vsBlob->GetBufferPointer(),
		                                  vsBlob->GetBufferSize(), &inputLayout);
		vsBlob->Release();
	}
	{
		ID3DBlob* psBlob;
		D3DCompile(shaders + 1, sizeof(shaders) - 3, NULL, NULL, NULL, "PS", "ps_5_0", 0, 0, &psBlob, NULL);
		device->CreatePixelShader( psBlob->GetBufferPointer(),
		                                  psBlob->GetBufferSize(), NULL, &pixelShader);
		psBlob->Release();
	}
	{
		stride = 4 * sizeof(float);
		numVerts = 4;
		offset = 0;

		D3D11_BUFFER_DESC vertexBufferDesc = {};
		vertexBufferDesc.ByteWidth = stride * numVerts;
		vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		device->CreateBuffer(&vertexBufferDesc, NULL, &vertexBuffer);
	}
	{
		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

		device->CreateSamplerState(& samplerDesc, & samplerState);
	}
	for (int i = 0; i < numLuts; i++)
	{
		lutData* lut = &luts[i];

		D3D11_TEXTURE3D_DESC desc = {};
		desc.Width = lut->size;
		desc.Height = lut->size;
		desc.Depth = lut->size;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA initData;
		initData.pSysMem = lut->rawLut;
		initData.SysMemPitch = lut->size * 4 * sizeof(float);
		initData.SysMemSlicePitch = lut->size * lut->size * 4 * sizeof(float);

		ID3D11Texture3D* tex;
		device->CreateTexture3D(&desc, & initData, & tex);
		device->CreateShaderResourceView( (ID3D11Resource*)tex, NULL, &luts[i].textureView);
		tex->Release();
		free(lut->rawLut);
		lut->rawLut = NULL;
	}
	{
		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

		device->CreateSamplerState(&samplerDesc, &noiseSamplerState);
	}
	{
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = NOISE_SIZE;
		desc.Height = NOISE_SIZE;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R32_FLOAT;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		float noise[NOISE_SIZE][NOISE_SIZE];

		for (int i = 0; i < NOISE_SIZE; i++)
		{
			for (int j = 0; j < NOISE_SIZE; j++)
			{
				noise[i][j] = (noiseBytes[i][j] + 0.5) / 256;
			}
		}

		D3D11_SUBRESOURCE_DATA initData;
		initData.pSysMem = noise;
		initData.SysMemPitch = sizeof(noise[0]);

		ID3D11Texture2D* tex;
		device->CreateTexture2D(&desc, &initData, &tex);
		device->CreateShaderResourceView((ID3D11Resource*)tex, NULL, &noiseTextureView);
		tex->Release();
	}
	{
		D3D11_BUFFER_DESC constantBufferDesc = {};
		constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constantBufferDesc.ByteWidth = 16;
		constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		device->CreateBuffer(&constantBufferDesc, NULL, &constantBuffer);
	}
}

void UninitializeStuff()
{
	RELEASE_IF_NOT_NULL(device)
	RELEASE_IF_NOT_NULL(deviceContext)
	RELEASE_IF_NOT_NULL(vertexShader)
	RELEASE_IF_NOT_NULL(pixelShader)
	RELEASE_IF_NOT_NULL(inputLayout)
	RELEASE_IF_NOT_NULL(vertexBuffer)
	RELEASE_IF_NOT_NULL(samplerState)
	for (int i = 0; i < 2; i++)
	{
		RELEASE_IF_NOT_NULL(texture[i])
		RELEASE_IF_NOT_NULL(textureView[i])
	}
	RELEASE_IF_NOT_NULL(noiseSamplerState)
	RELEASE_IF_NOT_NULL(noiseTextureView)
	RELEASE_IF_NOT_NULL(constantBuffer)
	for (int i = 0; i < numLuts; i++)
	{
		free(luts[i].rawLut);
		RELEASE_IF_NOT_NULL(luts[i].textureView)
	}
	free(luts);
	free(lutTargets);
}

bool ApplyLUT(void* cOverlayContext, IDXGISwapChain* swapChain, struct tagRECT* rects, int numRects)
{
	if (!device)
	{
		InitializeStuff(swapChain);
	}

	ID3D11Texture2D* backBuffer;
	ID3D11RenderTargetView* renderTargetView;

	swapChain->GetBuffer(0, IID_ID3D11Texture2D, (void**)&backBuffer);

	D3D11_TEXTURE2D_DESC newBackBufferDesc;
	backBuffer->GetDesc(&newBackBufferDesc);

	int index = -1;
	if (newBackBufferDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM)
	{
		index = 0;
	}
	else if (newBackBufferDesc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT)
	{
		index = 1;
	}

	lutData* lut;
	if (index == -1 || !(lut = GetLUTDataFromCOverlayContext(cOverlayContext, index == 1)))
	{
		backBuffer->Release();
		return false;
	}

	D3D11_TEXTURE2D_DESC oldTextureDesc = textureDesc[index];
	if (newBackBufferDesc.Width > oldTextureDesc.Width || newBackBufferDesc.Height > oldTextureDesc.Height)
	{
		if (texture[index] != NULL)
		{
			texture[index]->Release();
			textureView[index]->Release();
		}

		UINT newWidth = max(newBackBufferDesc.Width, oldTextureDesc.Width);
		UINT newHeight = max(newBackBufferDesc.Height, oldTextureDesc.Height);

		D3D11_TEXTURE2D_DESC newTextureDesc;

		newTextureDesc = newBackBufferDesc;
		newTextureDesc.Width = newWidth;
		newTextureDesc.Height = newHeight;
		newTextureDesc.Usage = D3D11_USAGE_DEFAULT;
		newTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		newTextureDesc.CPUAccessFlags = 0;
		newTextureDesc.MiscFlags = 0;

		textureDesc[index] = newTextureDesc;

		device->CreateTexture2D(&textureDesc[index], NULL, &texture[index]);
		device->CreateShaderResourceView((ID3D11Resource*)texture[index], NULL, &textureView[index]);
	}

	backBufferDesc = newBackBufferDesc;

	device->CreateRenderTargetView((ID3D11Resource*)backBuffer, NULL, &renderTargetView);
	const D3D11_VIEWPORT d3d11_viewport(0, 0, backBufferDesc.Width, backBufferDesc.Height, 0.0f, 1.0f);
	deviceContext->RSSetViewports(1,&d3d11_viewport);

	deviceContext->OMSetRenderTargets(1, &renderTargetView, NULL);
	renderTargetView->Release();

	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	deviceContext->IASetInputLayout(inputLayout);

	deviceContext->VSSetShader(vertexShader, NULL, 0);
	deviceContext->PSSetShader(pixelShader, NULL, 0);

	deviceContext->PSSetShaderResources(0, 1, &textureView[index]);
	deviceContext->PSSetShaderResources(1, 1, &lut->textureView);
	deviceContext->PSSetSamplers(0, 1, &samplerState);

	deviceContext->PSSetShaderResources(2, 1, &noiseTextureView);
	deviceContext->PSSetSamplers(1, 1, &noiseSamplerState);

	int constantData[4] = {lut->size, index == 1};

	D3D11_MAPPED_SUBRESOURCE resource;
	deviceContext->Map((ID3D11Resource*)constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0,
	                           &resource);
	memcpy(resource.pData, constantData, sizeof(constantData));
	deviceContext->Unmap((ID3D11Resource*)constantBuffer, 0);

	deviceContext->PSSetConstantBuffers(0, 1, &constantBuffer);

	for (int i = 0; i < numRects; i++)
	{
		D3D11_BOX sourceRegion;
		sourceRegion.left = rects[i].left;
		sourceRegion.right = rects[i].right;
		sourceRegion.top = rects[i].top;
		sourceRegion.bottom = rects[i].bottom;
		sourceRegion.front = 0;
		sourceRegion.back = 1;

		deviceContext->CopySubresourceRegion((ID3D11Resource*)texture[index], 0, rects[i].left,
		                                             rects[i].top, 0, (ID3D11Resource*)backBuffer, 0, &sourceRegion);
		DrawRectangle(&rects[i], index);
	}

	backBuffer->Release();
	return true;
}

typedef struct rectVec
{
	struct tagRECT* start;
	struct tagRECT* end;
	struct tagRECT* cap;
} rectVec;

typedef long (COverlayContext_Present_t)(void*, void*, unsigned int, rectVec*, unsigned int, bool);

COverlayContext_Present_t* COverlayContext_Present_orig;
COverlayContext_Present_t* COverlayContext_Present_real_orig;

void log_to_file(const char* log_buf)
{
	try
	{
		std::fstream file(LOG_FILE_PATH,
		                  std::ios::out | std::ios::in | std::ios::ate);

		std::streampos file_size = file.tellp();

		if (file_size > MAX_LOG_FILE_SIZE)
		{
			file.close();
			std::remove(LOG_FILE_PATH);
			file.open(LOG_FILE_PATH, std::ios::out);
		}
		else
		{
			file.seekp(0, std::ios::end);
		}
		if (static bool first_log = true)
		{
			MessageBoxA(NULL, "Writing to file", "DEBUG HOOK DWM", MB_OK);
			first_log = false;
		}
		file << log_buf;
		file.close();
		
	}
	catch (...)
	{
		MessageBoxA(NULL, "Something really fucked up in log_to_file function", "DEBUG HOOK DWM", MB_OK);
	}
}

long COverlayContext_Present_hook(void* self, void* overlaySwapChain, unsigned int a3, rectVec* rectVec,
                                  unsigned int a5, bool a6)
{
	if (_ReturnAddress() < (void*)COverlayContext_Present_real_orig)
	{
		if (static bool first_log = true)
		{
			log_to_file("I am inside COverlayContext::Present hook inside the main if condition");
			first_log = false;
		}
		
		if (isWindows11 && *((bool*)overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11) ||
			!isWindows11 && *((bool*)overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset))
		{
			UnsetLUTActive(self);
		}
		else
		{
			IDXGISwapChain* swapChain;
			if (isWindows11)
			{
				swapChain = *(IDXGISwapChain**)((unsigned char*)overlaySwapChain +
					IOverlaySwapChain_IDXGISwapChain_offset_w11);
			}
			else
			{
				swapChain = *(IDXGISwapChain**)((unsigned char*)overlaySwapChain +
					IOverlaySwapChain_IDXGISwapChain_offset);
			}

			if (ApplyLUT(self, swapChain, rectVec->start, rectVec->end - rectVec->start))
			{
				SetLUTActive(self);
			}
			else
			{
				UnsetLUTActive(self);
			}
		}
	}
	
	return COverlayContext_Present_orig(self, overlaySwapChain, a3, rectVec, a5, a6);
}

typedef bool (COverlayContext_IsCandidateDirectFlipCompatbile_t)(void*, void*, void*, void*, int, unsigned int, bool,
                                                                 bool);

COverlayContext_IsCandidateDirectFlipCompatbile_t* COverlayContext_IsCandidateDirectFlipCompatbile_orig;

bool COverlayContext_IsCandidateDirectFlipCompatbile_hook(void* self, void* a2, void* a3, void* a4, int a5,
                                                          unsigned int a6, bool a7, bool a8)
{
	if (IsLUTActive(self))
	{
		return false;
	}
	return COverlayContext_IsCandidateDirectFlipCompatbile_orig(self, a2, a3, a4, a5, a6, a7, a8);
}

typedef bool (COverlayContext_OverlaysEnabled_t)(void*);

COverlayContext_OverlaysEnabled_t* COverlayContext_OverlaysEnabled_orig;

bool COverlayContext_OverlaysEnabled_hook(void* self)
{
	if (IsLUTActive(self))
	{
		return false;
	}
	return COverlayContext_OverlaysEnabled_orig(self);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		{
			HMODULE dwmcore = GetModuleHandle(L"dwmcore.dll");
			MODULEINFO moduleInfo;
			GetModuleInformation(GetCurrentProcess(), dwmcore, &moduleInfo, sizeof moduleInfo);

			unsigned char* KUSER_SHARED_DATA = (unsigned char*)0x7FFE0000;
			ULONG NtBuildNumber = *(ULONG*)(KUSER_SHARED_DATA + 0x260);
			isWindows11 = NtBuildNumber >= 22000;
			// TODO: Remove this debug instruction
			MessageBoxA(NULL, "DWM LUT ATTACH", "DEBUG HOOK DWM", MB_OK);

			if (isWindows11)
			{
				// TODO: Remove this debug instruction
				MessageBoxA(NULL, "DETECTED WINDOWS 11 OS", "DEBUG HOOK DWM", MB_OK);
				for (size_t i = 0; i <= moduleInfo.SizeOfImage - sizeof COverlayContext_OverlaysEnabled_bytes_w11; i++)
				{
					unsigned char* address = (unsigned char*)dwmcore + i;
					if (!COverlayContext_Present_orig && sizeof COverlayContext_Present_bytes_w11 <= moduleInfo.
						SizeOfImage - i && !aob_match_inverse(address, COverlayContext_Present_bytes_w11,
						                                      sizeof COverlayContext_Present_bytes_w11))
					{
						// TODO: Remove this debug instruction
						MessageBoxA(NULL, "DETECTED COverlayContextPresent address", "DEBUG HOOK DWM", MB_OK);
						COverlayContext_Present_orig = (COverlayContext_Present_t*)address;
						COverlayContext_Present_real_orig = COverlayContext_Present_orig;
					}
					else if (!COverlayContext_IsCandidateDirectFlipCompatbile_orig && sizeof
						COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11 <= moduleInfo.SizeOfImage - i && !
						aob_match_inverse(
							address, COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11,
							sizeof COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11))
					{
						COverlayContext_IsCandidateDirectFlipCompatbile_orig = (
							COverlayContext_IsCandidateDirectFlipCompatbile_t*)address;
					}
					else if (!COverlayContext_OverlaysEnabled_orig && sizeof COverlayContext_OverlaysEnabled_bytes_w11
						<= moduleInfo.SizeOfImage - i && !aob_match_inverse(
							address, COverlayContext_OverlaysEnabled_bytes_w11,
							sizeof COverlayContext_OverlaysEnabled_bytes_w11))
					{
						COverlayContext_OverlaysEnabled_orig = (COverlayContext_OverlaysEnabled_t*)address;
					}
					if (COverlayContext_Present_orig && COverlayContext_IsCandidateDirectFlipCompatbile_orig &&
						COverlayContext_OverlaysEnabled_orig)
					{
						MessageBoxA(NULL, "All addresses successfully retrieved", "DEBUG HOOK DWM", MB_OK);
						break;
					}
				}

				DWORD rev;
				DWORD revSize = sizeof(rev);
				RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "UBR", RRF_RT_DWORD,
				             NULL, &rev, &revSize);

				if (rev >= 706)
				{
					MessageBoxA(NULL, "Dectected recent Windows OS", "DEBUG HOOK DWM", MB_OK);
					// COverlayContext_DeviceClipBox_offset_w11 += 8;
				}
			}
			else
			{
				for (size_t i = 0; i <= moduleInfo.SizeOfImage - sizeof(COverlayContext_Present_bytes); i++)
				{
					unsigned char* address = (unsigned char*)dwmcore + i;
					if (!COverlayContext_Present_orig && !memcmp(address, COverlayContext_Present_bytes,
					                                             sizeof(COverlayContext_Present_bytes)))
					{
						COverlayContext_Present_orig = (COverlayContext_Present_t*)address;
						COverlayContext_Present_real_orig = COverlayContext_Present_orig;
					}
					else if (!COverlayContext_IsCandidateDirectFlipCompatbile_orig && !memcmp(
						address, COverlayContext_IsCandidateDirectFlipCompatbile_bytes,
						sizeof(COverlayContext_IsCandidateDirectFlipCompatbile_bytes)))
					{
						static int found = 0;
						found++;
						if (found == 2)
						{
							COverlayContext_IsCandidateDirectFlipCompatbile_orig = (
								COverlayContext_IsCandidateDirectFlipCompatbile_t*)(address - 0xa);
						}
					}
					else if (!COverlayContext_OverlaysEnabled_orig && !memcmp(
						address, COverlayContext_OverlaysEnabled_bytes, sizeof(COverlayContext_OverlaysEnabled_bytes)))
					{
						COverlayContext_OverlaysEnabled_orig = (COverlayContext_OverlaysEnabled_t*)(address - 0x7);
					}
					if (COverlayContext_Present_orig && COverlayContext_IsCandidateDirectFlipCompatbile_orig &&
						COverlayContext_OverlaysEnabled_orig)
					{
						break;
					}
				}
			}

			char lutFolderPath[MAX_PATH];
			ExpandEnvironmentStringsA(LUT_FOLDER, lutFolderPath, sizeof(lutFolderPath));
			if (!AddLUTs(lutFolderPath))
			{
				return FALSE;
			}
			char variable_message_states[300];
			sprintf(variable_message_states, "Current variable states: COverlayContext::Present - %p\t"
			        "COverlayContext::IsCandidateDirectFlipCompatible - %p\tCOverlayContext::OverlaysEnabled - %p",
			        COverlayContext_Present_orig,
			        COverlayContext_IsCandidateDirectFlipCompatbile_orig, COverlayContext_OverlaysEnabled_orig);
			MessageBoxA(NULL, variable_message_states, "DEBUG HOOM DWM", MB_OK);

			if (COverlayContext_Present_orig && COverlayContext_IsCandidateDirectFlipCompatbile_orig &&
				COverlayContext_OverlaysEnabled_orig && numLuts != 0)

			{
				MH_Initialize();
				MH_CreateHook((PVOID)COverlayContext_Present_orig, (PVOID)COverlayContext_Present_hook,
				              (PVOID*)&COverlayContext_Present_orig);
				MH_CreateHook((PVOID)COverlayContext_IsCandidateDirectFlipCompatbile_orig,
				              (PVOID)COverlayContext_IsCandidateDirectFlipCompatbile_hook,
				              (PVOID*)&COverlayContext_IsCandidateDirectFlipCompatbile_orig);
				MH_CreateHook((PVOID)COverlayContext_OverlaysEnabled_orig, (PVOID)COverlayContext_OverlaysEnabled_hook,
				              (PVOID*)&COverlayContext_OverlaysEnabled_orig);
				MH_EnableHook(MH_ALL_HOOKS);

				//log_to_file("DWM HOOK DLL INITIALIZATION");
				MessageBoxA(NULL, "DWM HOOK INITIALIZATION", "DEBUG HOOK DWM", MB_OK);
				break;
			}
			return FALSE;
		}
	case DLL_PROCESS_DETACH:
		MH_Uninitialize();
		Sleep(100);
		UninitializeStuff();
		break;
	default:
		break;
	}
	return TRUE;
}


#pragma once
#include "datatypes.hpp"
#include "com_ptr.hpp"
#include <windows.h>
#include <d3d11.h>
#include <vector>
#include <array>

class DirectXDevice
{
public:

	enum class BufferType
	{
		Default,
		Structured,
		ByteAddress
	};

	DirectXDevice(HWND window, int2 resolution);

	// Create resources
	ID3D11UnorderedAccessView* createBackBufferUAV();
	ID3D11DepthStencilView* createDepthStencilView(int2 size);
	ID3D11RenderTargetView* DirectXDevice::createBackBufferRTV();
	ID3D11ComputeShader* createComputeShader(const std::vector<unsigned char> &shaderBytes);
	ID3D11Buffer* createConstantBuffer(unsigned bytes);
	ID3D11Buffer* createBuffer(unsigned numElements, unsigned strideBytes, BufferType type = BufferType::Default);
	// TODO: Add support for 1d textures
	ID3D11Texture2D* createTexture2d(int2 dimensions, DXGI_FORMAT format, int mips);
	ID3D11Texture3D* createTexture3d(int3 dimensions, DXGI_FORMAT format, int mips);
	ID3D11UnorderedAccessView* createUAV(ID3D11Resource *buffer);
	ID3D11UnorderedAccessView* createByteAddressUAV(ID3D11Resource *buffer, unsigned numElements);
	ID3D11UnorderedAccessView* createTypedUAV(ID3D11Resource *buffer, unsigned numElements, DXGI_FORMAT format);
	ID3D11ShaderResourceView* createSRV(ID3D11Resource *buffer);
	ID3D11ShaderResourceView* DirectXDevice::createTypedSRV(ID3D11Resource *buffer, unsigned numElements, DXGI_FORMAT format);
	ID3D11SamplerState* createSampler();

	// Data update
	template <typename T>
	void updateConstantBuffer(ID3D11Buffer* cbuffer, const T &cb)
	{
		D3D11_MAPPED_SUBRESOURCE map;
		deviceContext->Map(cbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		memcpy(map.pData, &cb, sizeof(cb));
		deviceContext->Unmap(cbuffer, 0);
	}

	// Commands
	void clear(ID3D11RenderTargetView *rtv, const float4 &color);
	void clearDepth(ID3D11DepthStencilView *depthStencilView);
	void setRenderTargets(std::initializer_list<ID3D11RenderTargetView*> rtvs, ID3D11DepthStencilView *depthStencilView);
	void dispatch(ID3D11ComputeShader *shader, int3 resolution, int3 groupSize,
					std::initializer_list<ID3D11Buffer*> cbs,
					std::initializer_list<ID3D11ShaderResourceView*> srvs,
					std::initializer_list<ID3D11UnorderedAccessView*> uavs = {},
					std::initializer_list<ID3D11SamplerState*> samplers = {});
	void presentFrame();
	void clearUAV(ID3D11UnorderedAccessView* uav, std::array<float, 4> color);

	HWND getWindowHandle() { return windowHandle; }

	ID3D11Device *getDevice() { return device.ptr; }

	int2 getResolution() { return resolution; }

	ID3D11DeviceContext *getDeviceContext() { return deviceContext.ptr; }

private:

	HWND windowHandle;
	int2 resolution;
	com_ptr<IDXGISwapChain> swapChain;
	com_ptr<ID3D11Device> device;
	com_ptr<ID3D11DeviceContext> deviceContext;
};
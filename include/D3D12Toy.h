#pragma once
#include "DXSample.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class D3DToy : public DXSample
{
public:
	D3DToy(UINT width, UINT height, std::wstring name);
	~D3DToy();
	virtual void OnInit();
	virtual void OnUpdate();
	virtual void OnResize();
	virtual void OnRender();
	virtual void OnDestroy();

	void OnMouseMove();

protected:
	const D3D_FEATURE_LEVEL mAPPFeatureLevel = D3D_FEATURE_LEVEL_11_0;
	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM; //UNORM and <= 10bit, otherwise may cause unhandle exception when creating swapchain
	DXGI_FORMAT mDepthStencilBufferFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; // TODO ?
	static const UINT mBufferCount = 2;
	//UINT mCurrentBackBuffer = 0;

	bool mMsaa = false;
	UINT mMsaaSampleCount = 4;
	UINT mMsaaQuality = 2;

	bool mWindowd = true;

	float mFOV = 0.25 * XM_PI;
	float nearZ = 1.0f;
	float farZ = 100.0f;

private:
	ComPtr<IDXGIFactory4> mFactory; //Using DXGIFactory4 for WARP
	ComPtr<IDXGIAdapter> mAdapter; //GPU adapter
	ComPtr<ID3D12Device> mDevice; //GPU
	ComPtr<ID3D12Fence> mFence;	//Fence for synchronization
	UINT64 mCurrentFenceValue = 0;
	ComPtr<IDXGISwapChain3> mSwapChain;

	ComPtr<ID3D12CommandAllocator> mCommandAllocator;
	ComPtr<ID3D12GraphicsCommandList> mCommandList;
	ComPtr<ID3D12CommandQueue> mCommandQueue;

	ComPtr<ID3D12DescriptorHeap> mRTVHeap;
	ComPtr<ID3D12DescriptorHeap> mDSVHeap;
	ComPtr<ID3D12Resource> mSwapChainBuffer[mBufferCount];
	ComPtr<ID3D12Resource> mDepthStencilBuffer;
	
	//Upload (vertices/indices) to default heap using UploadBuffer
	std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB;
	std::unique_ptr<MeshGeometry> mGeometry;

	ComPtr<ID3D12DescriptorHeap> mCBVHeap; //CBV for CPU and GPU commmu
	ComPtr<ID3D12RootSignature> mRootSignature;

	ComPtr<ID3DBlob> mVertexShader; //bytecode
	ComPtr<ID3DBlob> mPixelShader;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	ComPtr<ID3D12PipelineState> mPSO;

	UINT mRTVDescSize; //Render Target View Descriptor Size
	UINT mDSVDescSize; //Depth / Stencil
	UINT mCBVDescSize; //Constant Buffer

	D3D12_VIEWPORT mViewport; //Reset whenever commandlist is reset
	D3D12_RECT mScissorRect;	//Reset whenever commandlist is reset
	XMFLOAT4X4 mWorld;
	XMFLOAT4X4 mView;
	XMFLOAT4X4 mProj;

	void CheckFeatureSupport();

	void CreateSwapChain();
	void CreateCommandObjects();

	virtual void CreateRTVAndDSVDescriptorHeap();
	//Below TODO
	void CreateCBVDescriptorHeap();

	void CreateRootSignature();

	void CreateShadersAndInputLayout();

	//Organize geometry, upload to default heap
	void BuildGeometry(); //VBV and IBV creating on render
	
	void CreatePipelineStateObject(); 

	void FlushCommandQueue();

};
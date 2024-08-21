#pragma once
#include "DXSample.h"
#include "Tools/GeometryGenerator.h"

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

	static const int numFrameResources = 3;

private:
	// Constant data per-object.
	struct ObjectConstants
	{
		XMFLOAT4X4 world;
	};
	const UINT objCBByteSize = CalcConstBufferByteSizes(sizeof(ObjectConstants));

	// Constant data per pass
	struct PassConstants
	{
		XMFLOAT4X4 view;
		XMFLOAT4X4 inverseView;
		XMFLOAT4X4 proj;
		XMFLOAT4X4 inverseProj;
		XMFLOAT4X4 viewProj;
		XMFLOAT4X4 inverseViewProj;
		
		XMFLOAT2 RTVSize;
		XMFLOAT2 invRTVSize;
		float nearZ;
		float farZ;
		float totalTime;
	};
	const UINT passCBByteSize = CalcConstBufferByteSizes(sizeof(PassConstants));

	//Constant buffer info used for different levels of CBV update frequency.
	struct FrameResource {
	public:
		FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount)
		{
			ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAllocator)));
			objCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
			passCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
		}
		FrameResource(const FrameResource& rhs) = delete;
		FrameResource& operator=(const FrameResource& rhs) = delete;
		~FrameResource() {}

		// We cannot reset the allocator until the GPU is done processing commands. 
		// Each frame needs their own allocator.
		ComPtr<ID3D12CommandAllocator> cmdAllocator;
		// We cannot update a cbuffer until the GPU is done processing commands that reference it.
		// Each frame needs their own cbuffers.
		std::unique_ptr<UploadBuffer<ObjectConstants>> objCB = nullptr;
		std::unique_ptr<UploadBuffer<PassConstants>> passCB = nullptr;

		// Fence value to mark commands up to this fence point. This lets us
		// check if these frame resources are still in use by the GPU.
		UINT64 fence = 0;
	};
	//Aggregation of rendering information used for different PSO(Opaque, transparent...)
	struct RenderItem
	{
		RenderItem()
		{
			XMStoreFloat4x4(&world, XMMatrixIdentity());
		}
		// World matrix of the shape that describes the object¡¯s local space
		// relative to the world space, which defines the position, 
		// orientation, and scale of the object in the world.
		XMFLOAT4X4 world;
		// Dirty flag indicating the object data has changed and we need 
		// to update the constant buffer. Because we have an object 
		// cbuffer for each FrameResource, we have to apply the
		// update to each FrameResource. Thus, when we modify obj constant data we
		// should set NumFramesDirty = gNumFrameResources so that each frame resource
		// gets the update.
		int numFramesDirty = numFrameResources;
		// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
		UINT objCBIndex = -1;
		// Geometry associated with this render-item. Multiple render-items can share the same geometry.
		MeshGeometry* geo = nullptr;
		// Primitive topology.
		D3D12_PRIMITIVE_TOPOLOGY primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		// DrawIndexedInstanced parameters.
		UINT indexCount = 0;
		UINT startIndexLocation = 0;
		int baseVertexLocation = 0;
	};

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
	
	std::unique_ptr<MeshGeometry> mGeometry;

	std::vector<std::unique_ptr<RenderItem>> mRenderItems = {}; //All render items
	std::vector<RenderItem*> mOpaqueRenderItems; //Divided by different PSO
	std::vector<RenderItem*> mTransparentRenderItems;

	//Upload (vertices/indices) to default heap using UploadBuffer
	//std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB;

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrentFrameRes = nullptr;
	int mCurrentFrameResIndex = 0;

	int mPassCbvOffset = 0;
	PassConstants mMainPassConst;

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

	XMFLOAT4X4 mView;
	XMFLOAT4X4 mProj;

	void CheckFeatureSupport();

	void CreateCommandObjects();
	void CreateSwapChain();

	void CreateRTVAndDSVDescriptorHeap();

	//Organize geometry, upload to default heap
	void BuildGeometry(); //VBV and IBV creating on render

	void BuildRenderItems();

	void CreateCBVDescriptorHeap();

	void CreateRootSignature();

	void CreateShadersAndInputLayout();

	void CreatePipelineStateObject(); 

	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	void FlushCommandQueue();

};
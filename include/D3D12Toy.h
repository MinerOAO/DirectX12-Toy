#pragma once
#include "DXSample.h"
#include "Tools/GeometryGenerator.h"

constexpr auto MAX_DIRECT_LIGHT_SOURCE_NUM = 8;
constexpr auto MAX_POINT_LIGHT_SOURCE_NUM = 8;
constexpr auto MAX_SPOT_LIGHT_SOURCE_NUM = 4;

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class D3DToy : public DXSample
{
public:
	D3DToy(UINT width, UINT height, std::wstring name);
	~D3DToy() override;
	void OnInit() override;
	void OnUpdate() override;
	void OnResize() override;
	void OnRender() override;
	void OnDestroy() override;

	void OnMouseMove(int xPos, int yPos, bool updatePos) override;
	void OnZoom(short delta) override;

protected:
	const D3D_FEATURE_LEVEL mAPPFeatureLevel = D3D_FEATURE_LEVEL_11_0;
	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM; //UNORM and <= 10bit, otherwise may cause unhandle exception when creating swapchain
	DXGI_FORMAT mDepthStencilBufferFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; // Depth 24 / Stencil 8
	static const UINT mBufferCount = 2;
	//UINT mCurrentBackBuffer = 0;

	bool mMsaa = false; //problematic, MSAA is supported for DXGI_SWAP_EFFECT_DISCARD, not FLIP_DISCARD
	UINT mMsaaSampleCount = 4;
	UINT mMsaaQuality = 2;

	bool mWindowd = true;

	float mFOV = 0.25 * XM_PI;
	float nearZ = 10.0f;
	float farZ = 10000.0f;
	XMFLOAT4X4 mView, mProj;

	static const int numFrameResources = 3;

	XMFLOAT2 mLastMousePos;
	float mPhi = 0.0f; //y
	float mTheta = 0.0f; //xz plane
	float mRadius = 500.0f; //r
	const float zoomSense = 0.1f; // Speed of zooming
	const float mouseSense = 0.25f; //Speed of mouse

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
		
		XMFLOAT3 eyePosWorld;
		float totalTime;

		XMFLOAT2 RTVSize;
		XMFLOAT2 invRTVSize;
		float nearZ;
		float farZ;
	};
	const UINT passCBByteSize = CalcConstBufferByteSizes(sizeof(PassConstants));
	//One constant one material
	struct MaterialConstants
	{
		DirectX::XMFLOAT4 diffuseAlbedo;
		DirectX::XMFLOAT3 fresnelR0;
		float shininess;
		// Used in the chapter on texture mapping.
		DirectX::XMFLOAT4X4 matTransform;
	};
	const UINT matCBByteSize = CalcConstBufferByteSizes(sizeof(MaterialConstants));
	struct LightConstants
	{
		XMFLOAT4 ambientLight;
		Light directionalLights[MAX_DIRECT_LIGHT_SOURCE_NUM];
		Light pointLights[MAX_POINT_LIGHT_SOURCE_NUM];
		Light spotLights[MAX_SPOT_LIGHT_SOURCE_NUM];
	};
	const UINT lightCBByteSize = CalcConstBufferByteSizes(sizeof(LightConstants));
	//Constant buffer info used for different levels of CBV update frequency.
	struct FrameResource {
	public:
		FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount)
		{
			ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAllocator)));
			objCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
			materialCB = std::make_unique<UploadBuffer<MaterialConstants>>(device, materialCount, true);
			passCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
			lightCB = std::make_unique<UploadBuffer<LightConstants>>(device, passCount, true);
		}
		FrameResource(const FrameResource& rhs) = delete;
		FrameResource& operator=(const FrameResource& rhs) = delete;
		~FrameResource() {}

		// We cannot reset the allocator until the GPU is done processing commands. 
		// Each frame needs their own allocator.
		ComPtr<ID3D12CommandAllocator> cmdAllocator;
		// We cannot update a cbuffer until the GPU is done processing commands that reference it.Each frame needs their own cbuffers.
		//Obj constant buffer (Per obj)
		std::unique_ptr<UploadBuffer<ObjectConstants>> objCB = nullptr;
		//Material constant buffer(Per obj)
		std::unique_ptr<UploadBuffer<MaterialConstants>> materialCB = nullptr;

		//Per pass buffers below
		std::unique_ptr<UploadBuffer<PassConstants>> passCB = nullptr;
		//Light constant buffer(Per pass)
		std::unique_ptr<UploadBuffer<LightConstants>> lightCB = nullptr;
		//Dynamic vertex buffer
		//std::unique_ptr<UploadBuffer<Vertex>> waveVB = nullptr;
		
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
		std::string materialName = "";
		// Geometry associated with this render-item. Multiple render-items can share the same geometry.
		MeshGeometry* geo = nullptr;
		// Primitive topology.
		D3D12_PRIMITIVE_TOPOLOGY primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		// DrawIndexedInstanced parameters.
		UINT indexCount = 0;
		UINT startIndexLocation = 0;
		int baseVertexLocation = 0;
	};
	// State records for materials in constant buffer
	struct MaterialItem
	{
		MaterialItem()
		{
			XMStoreFloat4x4(&matConsts.matTransform, XMMatrixIdentity());
		}
		//name for lookup
		std::string name;
		//Index in CB 
		int matCBIndex = -1;
		//Index in SRV heap for diffuse texture
		int diffuseSRVHeapIndex = -1;
		//Dirty flag
		int numFramesDirty = numFrameResources;

		//Material Data
		MaterialConstants matConsts;
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

	ComPtr<ID3D12DescriptorHeap> mRTVDescHeap; //Render Target V
	ComPtr<ID3D12DescriptorHeap> mDSVDescHeap; //Depth Stencil V
	ComPtr<ID3D12DescriptorHeap> mShaderResDescHeap; //Shader Resource View

	ComPtr<ID3D12DescriptorHeap> mSamplerDescHeap;// For Dynamic Sampler
	std::vector<CD3DX12_STATIC_SAMPLER_DESC> mStaticSamplers;

	ComPtr<ID3D12Resource> mSwapChainBuffer[mBufferCount];
	ComPtr<ID3D12Resource> mDepthStencilBuffer;
	
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unique_ptr<MeshGeometry> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<MaterialItem>> mMaterialItems;

	std::vector<std::unique_ptr<RenderItem>> mRenderItems = {}; //All render items
	std::vector<RenderItem*> mOpaqueRenderItems; //Divided by different PSO
	std::vector<RenderItem*> mTransparentRenderItems;
	std::vector<RenderItem*> mWireFrameRenderItems;

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;//Constant buffer
	FrameResource* mCurrentFrameRes = nullptr;
	int mCurrentFrameResIndex = 0;
	int mMaterialCbvOffset = 0;

	PassConstants mMainPassConst;//View, proj matrix, near Z, far z
	LightConstants mLights;

	ComPtr<ID3D12DescriptorHeap> mConstBufferDescHeap; //CBV for CPU and GPU commmu
	ComPtr<ID3D12RootSignature> mRootSignature;

	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOMap;

	UINT mRTVDescSize; //Render Target View Descriptor Size
	UINT mDSVDescSize; //Depth / Stencil
	UINT mCBVDescSize; //Constant Buffer

	D3D12_VIEWPORT mViewport; //Reset whenever commandlist is reset
	D3D12_RECT mScissorRect;	//Reset whenever commandlist is reset

	void CheckFeatureSupport();

	void CreateCommandObjects();
	void CreateSwapChain();

	void CreateRTVAndDSVDescHeap();
	void CreateSRVAndSamplerDescHeap();

	//Organize geometry, upload to default heap
	std::unique_ptr<RenderItem> BuildSingleGeometry(GeometryGenerator::MeshData& meshData, MeshGeometry* geometry, std::vector<Vertex>& vertices, UINT& vertexOffset, std::vector<uint32_t>& indices, UINT& indexOffset,
		int objCBIndex, D3D12_PRIMITIVE_TOPOLOGY topology);

	void BuildGeoAndMat(); //VBV and IBV creating on render

	void SetLights();

	void CreateCBVDescriptorHeap();//CB depends on Per-obj constants(mat, geometry)

	void CreateRootSignature();

	void CreatePipelineStateObject(); 

	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	void FlushCommandQueue();

};
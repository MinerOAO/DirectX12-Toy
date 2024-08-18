#include "stdafx.h"
#include "D3D12Toy.h"

D3DToy::D3DToy(UINT width, UINT height, std::wstring name) : DXSample(width, height, name) 
{
	XMStoreFloat4x4(&mWorld, XMMatrixIdentity());
	XMStoreFloat4x4(&mView, XMMatrixIdentity());
	XMMATRIX p = XMMatrixPerspectiveFovLH(mFOV, static_cast<float>(mWidth) / mHeight, nearZ, farZ);
	XMStoreFloat4x4(&mProj, p);
}
D3DToy::~D3DToy()
{
	if (mDevice != nullptr)
	{
		FlushCommandQueue();
	}
}

void D3DToy::OnInit()
{
#if defined(DEBUG) || defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();
	}
#endif
	DXSample::OnInit();
//Create Factory
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mFactory))); 
//Create Device
	HRESULT deviceCreating = D3D12CreateDevice(nullptr, // default adapter
		mAPPFeatureLevel, IID_PPV_ARGS(&mDevice));
	if (FAILED(deviceCreating))
	{
		ComPtr<IDXGIAdapter> warpAdapter;
		ThrowIfFailed(mFactory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
		ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(), mAPPFeatureLevel, IID_PPV_ARGS(&mDevice)));
	}
//Create Fence
	ThrowIfFailed(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
//Get Descriptor Size
	mRTVDescSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDSVDescSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mCBVDescSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
//Check Feature support
	CheckFeatureSupport();
//Create Command Allocator, List and Queue
	CreateCommandObjects();
//Create Swap Chain
	CreateSwapChain();
//Create descriptor heap and handle
//Create RTV using SwapChain buffer and descriptor heap handle
//Create Depth/Stencil buffer, DSV. Initialize DSV state
	CreateRTVAndDSVDescriptorHeap();
//Create Constant Buffer descriptor heap
	CreateCBVDescriptorHeap();
//Create Root signature
	CreateRootSignature();
//Create Shader and Input layout
	CreateShadersAndInputLayout();
//Organize geometry, upload to default heap
	BuildGeometry();//VBV and IBV creating on render
//Create PSO
	CreatePipelineStateObject();
//Execute the initialization commands
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
//Wait until commands are finished
	FlushCommandQueue();
}
void D3DToy::OnUpdate()
{
	DXSample::OnUpdate();
	float x = 3.0f;
	float y = 3.0f;
	float z = 3.0f;

	XMVECTOR position = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	
	XMMATRIX view = XMMatrixLookAtLH(position, target, up);
	XMStoreFloat4x4(&mView, view);

	XMMATRIX world = XMLoadFloat4x4(&mWorld);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);
	XMMATRIX transform = world * view * proj;

	// Update the constant buffer with the latest worldViewProj matrix.
	ObjectConstants objConst;
	//Transpose->Row-major->column-major
	XMStoreFloat4x4(&objConst.WorldViewProj, XMMatrixTranspose(transform));
	mObjectCB->CopyData(0, objConst);
}
void D3DToy::OnResize()
{
	DXSample::OnResize();
	//When resized, compute aspect ratio and projection matrix
	
	XMMATRIX p = XMMatrixPerspectiveFovLH(mFOV, static_cast<float>(mWidth) / mHeight, nearZ, farZ);
	XMStoreFloat4x4(&mProj, p);
}
void D3DToy::OnRender()
{
	//only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(mCommandAllocator->Reset());
	//command list can be reset after it has been added to the command queue 
	ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), mPSO.Get()));

	//Set viewport, scissorRect
	mViewport.TopLeftX = 0.0f;
	mViewport.TopLeftY = 0.0f;
	mViewport.Width = static_cast<float>(mWidth);
	mViewport.Height = static_cast<float>(mHeight);
	mViewport.MinDepth = 0.0f;
	mViewport.MaxDepth = 1.0f;

	mScissorRect = { 0, 0, static_cast<long>(mWidth), static_cast<long>(mHeight) };
	mCommandList->RSSetViewports(1, &mViewport); //cannot specify multiple viewports to the same render target
	mCommandList->RSSetScissorRects(1, &mScissorRect); //cannot specify multiple scissor rectangles on the same render target

	//Indicate a state transition on the resource usage
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mSwapChainBuffer[mSwapChain->GetCurrentBackBufferIndex()].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET));

	//Clear back buffer and depth buffer
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
		mRTVHeap->GetCPUDescriptorHandleForHeapStart(),
		mSwapChain->GetCurrentBackBufferIndex(), // index to offset
		mRTVDescSize // byte size of descriptor
	);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(mDSVHeap->GetCPUDescriptorHandleForHeapStart());
	mCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	mCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	//Specify the render buffer
	mCommandList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);

	//Set CBV
	ID3D12DescriptorHeap* descriptorHeaps[] = { mCBVHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	//Set Root signature
	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	mCommandList->IASetVertexBuffers(0, 1, &mGeometry->VertexBufferView());
	mCommandList->IASetIndexBuffer(&mGeometry->IndexBufferView());
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	mCommandList->SetGraphicsRootDescriptorTable(0, mCBVHeap->GetGPUDescriptorHandleForHeapStart());

	mCommandList->DrawIndexedInstanced(mGeometry->drawArgs["box"].indexCount, 1, 0, 0, 0);

	//State transition
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mSwapChainBuffer[mSwapChain->GetCurrentBackBufferIndex()].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT));

	//End Recording
	mCommandList->Close();
	//Add to Command queue
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() }; //Why?
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
	//Swap back and front buffer
	ThrowIfFailed(mSwapChain->Present(0, 0)); //Parameter meaning?
	//mCurrentBackBuffer = (mCurrentBackBuffer + 1) % mBufferCount;

	//Wait commands to complete
	FlushCommandQueue();
}
void D3DToy::OnDestroy()
{

}

void D3DToy::FlushCommandQueue() //in other words, wait Commands to be excuted clearly
{
	mCurrentFenceValue++;
	// Add a command. Only when other commands before this are finished, mFence is set to fence.
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFenceValue));

	// Wait until GPU completes.
	if (mFence->GetCompletedValue() < mCurrentFenceValue)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFenceValue, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}
void D3DToy::CreateSwapChain()
{
	//Create Swap Chain
	mSwapChain.Reset();
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; //back buffer or render target
	swapChainDesc.BufferCount = mBufferCount;
	swapChainDesc.SampleDesc.Count = mMsaa ? mMsaaSampleCount : 1;
	swapChainDesc.SampleDesc.Quality = mMsaa ? (mMsaaQuality - 1) : 0;
	swapChainDesc.Windowed = mWindowd;
	swapChainDesc.OutputWindow = Win32Application::GetHwnd();
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; // when full-screen, display mode will be the one best matches the current application window dimensions
	
	auto& bufDesc = swapChainDesc.BufferDesc;
	bufDesc.Width = mWidth;
	bufDesc.Height = mHeight;
	bufDesc.Format = mBackBufferFormat;
	bufDesc.RefreshRate.Numerator = 360;
	bufDesc.RefreshRate.Denominator = 1; // Refreshrate = Numerator / Denominator
	bufDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED; //Center, Stretch 
	bufDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;

	ComPtr<IDXGISwapChain> swapChain;
	ThrowIfFailed(mFactory->CreateSwapChain(mCommandQueue.Get(), &swapChainDesc, &swapChain)); //request command queue
	ThrowIfFailed(swapChain.As(&mSwapChain)); //Transform to IDXGI 3
}
void D3DToy::CreateCommandObjects()
{
	//Create command list and command queue
	ThrowIfFailed(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCommandAllocator)));
	ThrowIfFailed(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		mCommandAllocator.Get(),
		nullptr, //Initial pipeline state object
		IID_PPV_ARGS(&mCommandList) // Using GetAddressOf will Only Retrieve pointer without modifying ComPtr. & operator will release ComPtr
	));// Node mask -> GPU ID?

	D3D12_COMMAND_QUEUE_DESC commandQueueDesc;
	commandQueueDesc.NodeMask = 0;
	commandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	ThrowIfFailed(mDevice->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&mCommandQueue)));
}
void D3DToy::CreateRTVAndDSVDescriptorHeap()
{
	//Create Descriptor Heap
	D3D12_DESCRIPTOR_HEAP_DESC rtvDescHeap, dsvDescHeap;
	rtvDescHeap.NodeMask = 0; // GPU ID?
	rtvDescHeap.NumDescriptors = mBufferCount;
	rtvDescHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvDescHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(mDevice->CreateDescriptorHeap(&rtvDescHeap, IID_PPV_ARGS(&mRTVHeap)));

	dsvDescHeap.NodeMask = 0;
	dsvDescHeap.NumDescriptors = 1;
	dsvDescHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvDescHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(mDevice->CreateDescriptorHeap(&dsvDescHeap, IID_PPV_ARGS(&mDSVHeap)));
	//Create Desciptor Heap Handle
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescHandle(mRTVHeap->GetCPUDescriptorHandleForHeapStart(), mSwapChain->GetCurrentBackBufferIndex(), mRTVDescSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvDescHandle(mDSVHeap->GetCPUDescriptorHandleForHeapStart());
	//Create Render Target View¡¡(RTV to backbuffer)
	for (UINT i = 0; i < mBufferCount; ++i)
	{
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i]))); // Returns a pointer to an ID3D12Resource that represents the back buffer
		mDevice->CreateRenderTargetView(
			mSwapChainBuffer[i].Get(),
			nullptr, // nullptr indicates one mimap level with corresponding format (back buffer is 1 mipmap level)
			rtvDescHandle // Handle to the descriptor that will store the created RTV.
		); //Create RTV based on descriptor heap handle and actual back buffer resource.
		rtvDescHandle.Offset(1, mRTVDescSize);
	}
	//Creat Depth/Stencil Buffer, Commit to heap
	D3D12_RESOURCE_DESC dsBufferDesc;
	dsBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	dsBufferDesc.Alignment = 0;
	dsBufferDesc.Width = mWidth;
	dsBufferDesc.Height = mHeight;
	dsBufferDesc.DepthOrArraySize = 1;
	dsBufferDesc.Format = mDepthStencilBufferFormat;
	dsBufferDesc.MipLevels = 1;
	DXGI_SWAP_CHAIN_DESC scDesc;
	mSwapChain->GetDesc(&scDesc);
	dsBufferDesc.SampleDesc = scDesc.SampleDesc; //Should be the same as Back buffer
	dsBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	dsBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; //
	D3D12_CLEAR_VALUE optClear;
	optClear.Format = mDepthStencilBufferFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0; // ?

	ThrowIfFailed(mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), //Create Heap properties, currently Default heap
		D3D12_HEAP_FLAG_NONE, //Additional flags about the heap
		&dsBufferDesc,
		D3D12_RESOURCE_STATE_COMMON, //Set the initial usage state of the resource
		&optClear,
		IID_PPV_ARGS(&mDepthStencilBuffer)
	));
	//Create DepthStencil View
	mDevice->CreateDepthStencilView(
		mDepthStencilBuffer.Get(),
		nullptr, // indicates to create a view to the first mipmap level of this resource(the depthstencil buffer was created with only one mipmap level) with the format the resource was createdwith
		dsvDescHandle);
	//Transistion from initial state
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_DEPTH_WRITE
	));
}
void D3DToy::CreateCBVDescriptorHeap()
{
	//Constant Buffer descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NodeMask = 0; // GPU ID?
	cbvHeapDesc.NumDescriptors = 1;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	ThrowIfFailed(mDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCBVHeap)));

	//Constant buffer to store the constants of n object.
	int numOfElement = 1;
	mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(mDevice.Get(), numOfElement, true);
	//Constant buffer view (to each single object)
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc; //In RTV, we made the view desc nullptr.
	int cbvObjIdx = 0;
	UINT objByteSize = CalcConstBufferByteSizes(sizeof(ObjectConstants));
	cbvDesc.BufferLocation = mObjectCB->Resource()->GetGPUVirtualAddress() + objByteSize * cbvObjIdx;
	cbvDesc.SizeInBytes = objByteSize;

	mDevice->CreateConstantBufferView(&cbvDesc, mCBVHeap->GetCPUDescriptorHandleForHeapStart());
}
void D3DToy::CreateRootSignature()
{
	// The root signature defines the
	// resources the shader programs expect. If we think of the shader
	// programs as a function, and the input resources as function 
	// parameters, then the root signature can be thought of as defining 
	// the function signature.

	// A root signature is an array of root parameters.
	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[1];
	//Create a single descriptor table of CBVs
	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable); //Num of descriptor range, descriptor range
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT); //Num of parameter,

	// create a root signature with a single slot which points to a 
	// descriptor range consisting of a single constant buffer.
	//
	// Pattern: pointer to object Blob and error Blob.
	//	ID3DBlob is just a generic chunk of memory that has two methods: void* GetBufferPointer, GetBufferSize
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(),
		errorBlob.GetAddressOf());
	ThrowIfFailed(mDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&mRootSignature)));
}
void D3DToy::CreateShadersAndInputLayout()
{
	mVertexShader = CompileShader(L"src\\Shaders\\VertexShader.hlsl", nullptr, "VS", "vs_5_0");
	mPixelShader = CompileShader(L"src\\Shaders\\PixelShader.hlsl", nullptr, "PS", "ps_5_0");
	//INput element Desc
	mInputLayout = {
	// SemanticName, SemanticIndex, Format, InputSlot,  AlignedByteOffset, InputSlotClass(PER VERTEX / INSTANCE), InstanceDataStepRate
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}
void D3DToy::BuildGeometry()
{
	Vertex vertices[] =
	{
		Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(blue) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(blue) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(blue) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(blue) }),
		Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(blue) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(blue) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(blue) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(blue) })
	};
	const UINT64 vbStride = sizeof(Vertex);
	const UINT64 vbByteSize = 8 * sizeof(Vertex);

	uint16_t indices[] = {  // front face
	0, 1, 2,
	0, 2, 3,
	// back face
	4, 6, 5,
	4, 7, 6,
	// left face
	4, 5, 1,
	4, 1, 0,
	// right face
	3, 2, 6,
	3, 6, 7,
	// top face
	1, 5, 6,
	1, 6, 2,
	// bottom face
	4, 0, 3,
	4, 3, 7
	};
	const UINT ibByteSize = sizeof(indices);

	mGeometry = std::make_unique<MeshGeometry>();
	mGeometry->Name = "boxGeo";

	//Create resource on CPU
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mGeometry->vertexBufferCPU));
	CopyMemory(mGeometry->vertexBufferCPU->GetBufferPointer(), vertices, vbByteSize);
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mGeometry->indexBufferCPU));
	CopyMemory(mGeometry->indexBufferCPU->GetBufferPointer(), indices, ibByteSize);

	//Committed to default heap intermediately.
	CreateDefaultBuffer(mDevice.Get(), mCommandList.Get(), vertices, vbByteSize, mGeometry->vertexBufferGPU, mGeometry->vertexBufferUploader);
	CreateDefaultBuffer(mDevice.Get(), mCommandList.Get(), indices, ibByteSize, mGeometry->indexBufferGPU, mGeometry->indexBufferUploader);
	//Set necessary info
	mGeometry->vertexBufferByteSize = vbByteSize;
	mGeometry->vertexByteStride = sizeof(Vertex);
	mGeometry->indexFormat = DXGI_FORMAT_R16_UINT;
	mGeometry->indexBufferByteSize = ibByteSize;
	//Set submesh
	mGeometry->drawArgs["box"] = SubmeshGeometry{ sizeof(indices) / sizeof(uint16_t), 0, 0};
}
void D3DToy::CreatePipelineStateObject()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
	//Create shader desc
	psoDesc.VS = { reinterpret_cast<BYTE*>(mVertexShader->GetBufferPointer()), mVertexShader->GetBufferSize() };
	psoDesc.PS = { reinterpret_cast<BYTE*>(mPixelShader->GetBufferPointer()), mPixelShader->GetBufferSize() };
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = mBackBufferFormat;
	psoDesc.SampleDesc.Count = mMsaa ? mMsaaSampleCount : 1;
	psoDesc.SampleDesc.Quality = mMsaa ? (mMsaaQuality - 1) : 0;
	psoDesc.DSVFormat = mDepthStencilBufferFormat;

	ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}
void D3DToy::CheckFeatureSupport()
{
	//Check Feature support (4x MSAA here)
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS multiSampleLevel;
	multiSampleLevel.SampleCount = 4;
	multiSampleLevel.NumQualityLevels = 0; //Query num of quality levels
	multiSampleLevel.Format = mBackBufferFormat;
	multiSampleLevel.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	ThrowIfFailed(mDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &multiSampleLevel, sizeof(multiSampleLevel)));
	assert(multiSampleLevel.NumQualityLevels > 0);

	const DXGI_FORMAT formatArray[] = { 
	DXGI_FORMAT_R10G10B10A2_UNORM,
	DXGI_FORMAT_R10G10B10A2_UINT,
	DXGI_FORMAT_R11G11B10_FLOAT,
	DXGI_FORMAT_R8G8B8A8_UNORM,
	DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
	DXGI_FORMAT_R8G8B8A8_UINT,
	DXGI_FORMAT_R8G8B8A8_SNORM ,
	DXGI_FORMAT_R8G8B8A8_SINT };
	D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSup;
	for (auto format : formatArray)
	{
		formatSup.Format = format;
		if (SUCCEEDED(mDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSup, sizeof(formatSup))))
		{
			if (formatSup.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET)
			{
				//OutputDebugString(std::to_wstring((int)format).c_str());
			}
		}
	}
}
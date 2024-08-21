#include "stdafx.h"
#include "D3D12Toy.h"

D3DToy::D3DToy(UINT width, UINT height, std::wstring name) : DXSample(width, height, name) 
{
	XMStoreFloat4x4(&mView, XMMatrixIdentity());
	XMMATRIX p = XMMatrixPerspectiveFovLH(mFOV, m_aspectRatio, nearZ, farZ);
	XMStoreFloat4x4(&mProj, p);
}
D3DToy::~D3DToy()
{
	if (mDevice != nullptr)
	{
		FlushCommandQueue();
	}
}

void D3DToy::OnMouseMove()
{

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
//Organize geometry upload to default heap
	BuildGeometry();//VBV and IBV creating in DrawRenderItems()
//Essential info for render geometries
	BuildRenderItems();
//Create Frame Reousrces and their Constant Buffer descriptor heap
	CreateCBVDescriptorHeap();
//Create Root signature
	CreateRootSignature();
//Create Shader and Input layout
	CreateShadersAndInputLayout();
//Create PSO
	CreatePipelineStateObject();
//Execute the initialization commands
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
//Wait until commands are finished
	FlushCommandQueue();
}
//Update constant buffer
void D3DToy::OnUpdate()
{
	DXSample::OnUpdate();
	mCurrentFrameResIndex = (mCurrentFrameResIndex + 1) % numFrameResources;
	mCurrentFrameRes = mFrameResources[mCurrentFrameResIndex].get();
	//is set and has not reached this fence value
	if (mCurrentFrameRes->fence != 0 && mFence->GetCompletedValue() < mCurrentFrameRes->fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFrameRes->fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
	//Update obj constants 
	for (auto& e : mRenderItems)
	{
		// Only update the cbuffer data if the constants have changed. 
		// This needs to be tracked per frame resource.
		if (e->numFramesDirty > 0)
		{
			// Update the constant buffer with the latest world matrix.
			//Transpose->Row-major->column-major
			ObjectConstants objConst;
			XMMATRIX world = XMLoadFloat4x4(&e->world);
			XMStoreFloat4x4(&objConst.world, XMMatrixTranspose(world));
			mCurrentFrameRes->objCB->CopyData(e->objCBIndex, objConst);
			--e->numFramesDirty;
		}
	}

	//Update pass constants
	float x = 4.0f * cos(mTimer.CurrentTime());
	float y = 3.0f;
	float z = 4.0f * sin(mTimer.CurrentTime());
	
	XMVECTOR position = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	
	XMMATRIX view = XMMatrixLookAtLH(position, target, up);
	XMStoreFloat4x4(&mView, view);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);

	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassConst.view, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassConst.inverseView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassConst.proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassConst.inverseProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassConst.viewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassConst.inverseViewProj, XMMatrixTranspose(invViewProj));

	mMainPassConst.RTVSize = XMFLOAT2(mWidth, mHeight);
	mMainPassConst.invRTVSize = XMFLOAT2(1.0f / mWidth, 1.0f / mHeight);

	mMainPassConst.nearZ = nearZ;
	mMainPassConst.farZ = farZ;
	mMainPassConst.totalTime = mTimer.CurrentTime();

	mCurrentFrameRes->passCB->CopyData(0, mMainPassConst);
}
void D3DToy::OnResize()
{
	DXSample::OnResize();
	//When resized, compute aspect ratio and projection matrix
	
	XMMATRIX p = XMMatrixPerspectiveFovLH(mFOV, m_aspectRatio, nearZ, farZ);
	XMStoreFloat4x4(&mProj, p);
}
void D3DToy::OnRender()
{
	//only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(mCurrentFrameRes->cmdAllocator->Reset());
	//command list can be reset after it has been added to the command queue 
	ThrowIfFailed(mCommandList->Reset(mCurrentFrameRes->cmdAllocator.Get(), mPSO.Get()));

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

	int passCbvIndex = mPassCbvOffset + mCurrentFrameResIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
		mCBVHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, mCBVDescSize);
	mCommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);
	DrawRenderItems(mCommandList.Get(), mOpaqueRenderItems);

	//mCommandList->IASetVertexBuffers(0, 1, &mGeometry->VertexBufferView());
	//mCommandList->IASetIndexBuffer(&mGeometry->IndexBufferView());
	//mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//mCommandList->SetGraphicsRootDescriptorTable(0, mCBVHeap->GetGPUDescriptorHandleForHeapStart());

	//mCommandList->DrawIndexedInstanced(mGeometry->drawArgs["box"].indexCount, 1, 0, 0, 0);

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

	//Waiting process transferred to frame resources OnUpdate()
	++mCurrentFenceValue;
	mCurrentFrameRes->fence = mCurrentFenceValue;
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFenceValue));
	//Wait commands to complete
	//FlushCommandQueue();
}
void D3DToy::OnDestroy()
{

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
	for (int i = 0; i < numFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(mDevice.Get(), 1, mRenderItems.size()));
	}
	UINT objCount = (UINT)mOpaqueRenderItems.size();
	mPassCbvOffset = objCount * numFrameResources;
	//Constant Buffer descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NodeMask = 0; // GPU ID?
	cbvHeapDesc.NumDescriptors = numFrameResources * (objCount + 1);
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	ThrowIfFailed(mDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCBVHeap)));

	//Constant buffer view (to each single object)
	for (int frameIndex = 0; frameIndex < numFrameResources; ++frameIndex)
	{
		for (UINT objIndex = 0; objIndex < objCount; ++objIndex)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mFrameResources[frameIndex]->objCB->Resource()->GetGPUVirtualAddress();
			// Offset to the ith object constant buffer in the current buffer.
			cbAddress += objIndex * objCBByteSize;
			// Offset to the object CBV in the descriptor heap.
			int heapIndex = frameIndex * objCount + objIndex;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
				mCBVHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCBVDescSize);
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = objCBByteSize;
			mDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}
	// Last three descriptors are the pass CBVs for each frame resource.
	for (int frameIndex = 0; frameIndex < numFrameResources; ++frameIndex)
	{
		// Pass buffer only stores one cbuffer per frame resource.
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mFrameResources[frameIndex]->passCB->Resource()->GetGPUVirtualAddress();
		// Offset to the pass cbv in the descriptor heap.
		int heapIndex = mPassCbvOffset + frameIndex; //Jump over all objCB
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
			mCBVHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, mCBVDescSize);
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes = passCBByteSize;
		mDevice->CreateConstantBufferView(&cbvDesc, handle);
	}
	////Constant buffer to store the constants of n object.
	//int numOfElement = 1;
	//mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(mDevice.Get(), numOfElement, true);
	////Constant buffer view (to each single object)
	//D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc; //In RTV, we made the view desc nullptr.
	//int cbvObjIdx = 0;
	//UINT objByteSize = CalcConstBufferByteSizes(sizeof(ObjectConstants));
	//cbvDesc.BufferLocation = mObjectCB->Resource()->GetGPUVirtualAddress() + objByteSize * cbvObjIdx;
	//cbvDesc.SizeInBytes = objByteSize;

	//mDevice->CreateConstantBufferView(&cbvDesc, mCBVHeap->GetCPUDescriptorHandleForHeapStart());
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
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];
	//Create a single descriptor table of CBVs
	CD3DX12_DESCRIPTOR_RANGE cbvTable0;
	cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); //Set baseShaderRegister 0 -> buffer0(b0)
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0); //Num of descriptor range, descriptor range

	CD3DX12_DESCRIPTOR_RANGE cbvTable1;
	cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1); //Set baseShaderRegister 1 -> buffer1(b1)
	slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT); //Num of parameter,

	// create a root signature with a single slot which points to a 
	// descriptor range consisting of a single constant buffer.
	//
	// Pattern: pointer to object Blob and error Blob.
	//	ID3DBlob is just a generic chunk of memory that has two methods: void* GetBufferPointer, GetBufferSize
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&serializedRootSig,
		&errorBlob);
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
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.BuildBox(1.0f, 1.0f, 1.0f);
	GeometryGenerator::MeshData cylinder = geoGen.BuildCylinder(2.0f, 1.0f, 2.0f, 36, 36);

	//vertex offsets
	UINT boxVertexOffset = 0;
	UINT cylinderVertexOffset = box.vertices.size();
	//index offsets
	UINT boxIndexOffset = 0;
	UINT cylinderIndexOffset = box.indices.size();
	//Define submesh
	SubmeshGeometry boxSubmesh;
	boxSubmesh.indexCount = box.indices.size(); //Pay attention to index count
	boxSubmesh.startIndexLocation = boxIndexOffset;
	boxSubmesh.baseVertexLocation = boxVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.indexCount = cylinder.indices.size();
	cylinderSubmesh.startIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.baseVertexLocation = cylinderVertexOffset;

	std::vector<Vertex> vertices(box.vertices.size() + cylinder.vertices.size());
	UINT k = 0;
	for (size_t i = 0; i < box.vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.vertices[i].position;
		if (i % 2 == 0)
		{
			vertices[k].Color = XMFLOAT4(lightBlue);
		}
		else
		{
			vertices[k].Color = XMFLOAT4(lightGreen);
		}
	}

	for (size_t i = 0; i < cylinder.vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.vertices[i].position;
		if (i % 2 == 0)
		{
			vertices[k].Color = XMFLOAT4(lightBlue);
		}
		else
		{
			vertices[k].Color = XMFLOAT4(lightGreen);
		}
	}

	const UINT64 vbStride = sizeof(Vertex);
	const UINT64 vbByteSize = vertices.size() * sizeof(Vertex);

	std::vector<uint32_t> indices;
	indices.insert(indices.end(), box.indices.begin(), box.indices.end());
	indices.insert(indices.end(), cylinder.indices.begin(), cylinder.indices.end());

	const UINT ibByteSize = indices.size() * sizeof(uint32_t);

	mGeometry = std::make_unique<MeshGeometry>();
	mGeometry->Name = "Geometires";

	//Create resource on CPU
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mGeometry->vertexBufferCPU));
	CopyMemory(mGeometry->vertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mGeometry->indexBufferCPU));
	CopyMemory(mGeometry->indexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	//Committed to default heap intermediately. IASetVertex/IndexBuffer indicates the interpting ways
	CreateDefaultBuffer(mDevice.Get(), mCommandList.Get(), mGeometry->vertexBufferCPU->GetBufferPointer(), vbByteSize, mGeometry->vertexBufferGPU, mGeometry->vertexBufferUploader);
	CreateDefaultBuffer(mDevice.Get(), mCommandList.Get(), mGeometry->indexBufferCPU->GetBufferPointer(), ibByteSize, mGeometry->indexBufferGPU, mGeometry->indexBufferUploader);
	//Set necessary info
	mGeometry->vertexBufferByteSize = vbByteSize;
	mGeometry->vertexByteStride = sizeof(Vertex);
	mGeometry->indexFormat = DXGI_FORMAT_R32_UINT;
	mGeometry->indexBufferByteSize = ibByteSize;
	//Set submesh
	mGeometry->drawArgs["box"] = boxSubmesh;
	mGeometry->drawArgs["cylinder"] = cylinderSubmesh;
}
void D3DToy::BuildRenderItems()
{
	auto boxRenderItem = std::make_unique<RenderItem>();
	boxRenderItem->objCBIndex = 0;
	boxRenderItem->geo = mGeometry.get();
	boxRenderItem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRenderItem->indexCount = boxRenderItem->geo->drawArgs["box"].indexCount;
	boxRenderItem->startIndexLocation = boxRenderItem->geo->drawArgs["box"].startIndexLocation;
	boxRenderItem->baseVertexLocation = boxRenderItem->geo->drawArgs["box"].baseVertexLocation;
	mRenderItems.push_back(std::move(boxRenderItem));

	auto cylinderRenderItem = std::make_unique<RenderItem>();
	cylinderRenderItem->objCBIndex = 1;
	cylinderRenderItem->geo = mGeometry.get();
	cylinderRenderItem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	cylinderRenderItem->indexCount = cylinderRenderItem->geo->drawArgs["cylinder"].indexCount;
	cylinderRenderItem->startIndexLocation = cylinderRenderItem->geo->drawArgs["cylinder"].startIndexLocation;
	cylinderRenderItem->baseVertexLocation = cylinderRenderItem->geo->drawArgs["cylinder"].baseVertexLocation;
	mRenderItems.push_back(std::move(cylinderRenderItem));

	for (auto& e : mRenderItems)
	{
		mOpaqueRenderItems.push_back(e.get());
	}
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
void D3DToy::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = CalcConstBufferByteSizes(sizeof(ObjectConstants));
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];
		cmdList->IASetVertexBuffers(0, 1, &ri->geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->primitiveType);
		// Offset to the CBV in the descriptor heap for this object and
		// for this frame resource.
		UINT cbvIndex = mCurrentFrameResIndex * (UINT)mOpaqueRenderItems.size() + ri->objCBIndex;
		auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
			mCBVHeap->GetGPUDescriptorHandleForHeapStart());
		cbvHandle.Offset(cbvIndex, mCBVDescSize);
		cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle);
		cmdList->DrawIndexedInstanced(ri->indexCount, 1,
			ri->startIndexLocation, ri->baseVertexLocation, 0);
	}
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
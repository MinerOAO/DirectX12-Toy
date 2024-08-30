#include "stdafx.h"
#include "D3D12Toy.h"

D3DToy::D3DToy(UINT width, UINT height, std::wstring name) : DXSample(width, height, name) 
{
	//Init view matrix
	XMVECTOR position = XMVectorSet(mRadius * cosf(mPhi) * cosf(mTheta), mRadius * sinf(mPhi), mRadius * cosf(mPhi) * sinf(mTheta), 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(position, target, up);
	XMStoreFloat4x4(&mView, view);
	//INit proj

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

void D3DToy::OnMouseMove(int xPos, int yPos, bool updatePos)
{
	if (updatePos)
	{
		float yaw = XMConvertToRadians(mouseSense * static_cast<float>(xPos - mLastMousePos.x));
		float pitch = XMConvertToRadians(mouseSense * static_cast<float>(yPos - mLastMousePos.y));

		mPhi += pitch;
		mTheta += yaw;

		//limits pitch angle
		mPhi = mPhi > XM_PI / 2 ? (XM_PI / 2 - FLT_EPSILON) : mPhi;
		mPhi = mPhi < -XM_PI / 2 ? (-XM_PI / 2 + FLT_EPSILON) : mPhi; //cosf flips around pi/2, due to float precision?
		//view matrix update in OnUpdate() pass constants.
	}
	//Keep tracking position, avoiding sudden movement.
	mLastMousePos.x = xPos;
	mLastMousePos.y = yPos;
}

void D3DToy::OnZoom(short delta)
{
	mRadius -= delta * zoomSense;
	mRadius = mRadius < 0.1f ? 0.1f : mRadius;
	//view matrix update in OnUpdate() pass constants.
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
//Materials used for geometries
	BuildMaterial();
//Organize geometry upload to default heap
//Essential info for render geometries
	BuildGeometries();//VBV and IBV creating in DrawRenderItems()
	//BuildSingleGroupGeometries();
//Create Frame Reousrces and their Constant Buffer descriptor heap
	CreateCBVDescriptorHeap();
//Create Root signature
	CreateRootSignature();
//Create Shaders, Input layout and PSO
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
		if (e->objCBIndex == 0) //update box
		{
			ObjectConstants objConst;
			XMMATRIX world = XMMatrixIdentity();
			XMMATRIX rotation = XMMatrixRotationY(mTimer.CurrentTime());
			XMMATRIX trans = XMMatrixTranslation(mRadius * cos(2 * mTimer.CurrentTime()), 0.0f, mRadius * sin(2 * mTimer.CurrentTime()));
			world = XMMatrixMultiply(trans, world);
			world = XMMatrixMultiply(rotation, world);
			XMStoreFloat4x4(&objConst.world, XMMatrixTranspose(world));
			mCurrentFrameRes->objCB->CopyData(e->objCBIndex, objConst);
			e->numFramesDirty = numFrameResources - 1;
		}
	}
//Update material constants
	for (auto& e : mMaterialItems)
	{
		MaterialItem* matItem = e.second.get();
		if (matItem->numFramesDirty > 0)
		{
			mCurrentFrameRes->materialCB->CopyData(matItem->matCBIndex, matItem->matConsts);
			--matItem->numFramesDirty;
		}
	}
	//Update pass constants
	// sin* cos, cos, sin * sin?
	// cos * cos, sin, cos * sin?
	XMVECTOR position = XMVectorSet(mRadius * cosf(mPhi) * cosf(mTheta), mRadius * sinf(mPhi), -mRadius * cosf(mPhi) * sinf(mTheta), 1.0f);
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

	XMStoreFloat3(&mMainPassConst.eyePosWorld, position);

	mMainPassConst.RTVSize = XMFLOAT2(mWidth, mHeight);
	mMainPassConst.invRTVSize = XMFLOAT2(1.0f / mWidth, 1.0f / mHeight);

	mMainPassConst.nearZ = nearZ;
	mMainPassConst.farZ = farZ;
	mMainPassConst.totalTime = mTimer.CurrentTime();

	mCurrentFrameRes->passCB->CopyData(0, mMainPassConst);
//Update light constants
	mLights.ambientLight = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mLights.directionalLights[0].direction = XMFLOAT3(1.0f, 1.0f, 1.0f);
	mLights.directionalLights[0].strength = XMFLOAT3(1.0f, 1.0f, 1.0f);

	mCurrentFrameRes->lightCB->CopyData(0, mLights);
}
void D3DToy::OnResize()
{
	DXSample::OnResize();
	//When resized, compute projection matrix
	XMMATRIX p = XMMatrixPerspectiveFovLH(mFOV, m_aspectRatio, nearZ, farZ);
	XMStoreFloat4x4(&mProj, p);
}
void D3DToy::OnRender()
{
	//only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(mCurrentFrameRes->cmdAllocator->Reset());
	//command list can be reset after it has been added to the command queue 
	//set initial state(triangle) for next pass
	ThrowIfFailed(mCommandList->Reset(mCurrentFrameRes->cmdAllocator.Get(), mPSOMap["triangle"].Get()));

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
	//using root descriptor instead of descriptor heap for single object per pass
	mCommandList->SetGraphicsRootConstantBufferView(2, mCurrentFrameRes->passCB->Resource()->GetGPUVirtualAddress());
	mCommandList->SetGraphicsRootConstantBufferView(3, mCurrentFrameRes->lightCB->Resource()->GetGPUVirtualAddress());
	DrawRenderItems(mCommandList.Get(), mOpaqueRenderItems);
	//Change pipelinestate
	mCommandList->SetPipelineState(mPSOMap["line"].Get());
	DrawRenderItems(mCommandList.Get(), mLineRenderItems);

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
	UINT objCount = (UINT)mRenderItems.size();//Changes from mOpaque to mRenderItems
	UINT materialCount = (UINT)mMaterialItems.size();
	mMaterialCbvOffset = objCount * numFrameResources;
	//Constant Buffer descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NodeMask = 0; // GPU ID?
	cbvHeapDesc.NumDescriptors = numFrameResources * (objCount + materialCount);
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	ThrowIfFailed(mDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCBVHeap)));

	//Below for Creating different CBVs according to offset

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
	//CBV for materials.
	for (int frameIndex = 0; frameIndex < numFrameResources; ++frameIndex)
	{
		for (UINT matIndex = 0; matIndex < materialCount; ++matIndex)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mFrameResources[frameIndex]->materialCB->Resource()->GetGPUVirtualAddress();
			// Offset to the ith object constant buffer in the current buffer.
			cbAddress += matIndex * matCBByteSize;

			// Offset to the CBV in the descriptor heap.
			int heapIndex = mMaterialCbvOffset + frameIndex * materialCount + matIndex;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
				mCBVHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCBVDescSize);
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = matCBByteSize;
			mDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}
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
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];
	//Create a single descriptor table of CBVs
	CD3DX12_DESCRIPTOR_RANGE cbvTable0, cbvTableMaterial;
	cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); //Set baseShaderRegister 0 -> buffer0(b0)
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0); //Num of descriptor range, descriptor range

	cbvTableMaterial.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);//buffer 1
	slotRootParameter[1].InitAsDescriptorTable(1, &cbvTableMaterial);

	slotRootParameter[2].InitAsConstantBufferView(2); //buffer 2

	slotRootParameter[3].InitAsConstantBufferView(3); //buffer 3

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT); //Num of parameter,

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
void D3DToy::BuildMaterial()
{
	std::string matName = "default";
	auto material = std::make_unique<MaterialItem>();
	material->matCBIndex = 0;
	material->name = matName;
	material->matConsts.diffuseAlbedo = XMFLOAT4(lightBlue);
	material->matConsts.fresnelR0 = XMFLOAT3(0.95f, 0.95f, 0.95f);
	material->matConsts.shininess = 64;
	mMaterialItems.emplace(matName, std::move(material));
}
void D3DToy::BuildGeometries()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.BuildBox(10.0f, 10.0f, 10.0f);
	GeometryGenerator::MeshData grid = geoGen.BuildGrid(1000.0f, 1000.0f, 10, 10);

	GeometryGenerator::MeshData objData;
	geoGen.ReadObjFileInOne("assets\\models\\Homework\\Ai.obj", objData);

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	mGeometry = std::make_unique<MeshGeometry>();
	mGeometry->Name = "Geometires";

	int renderItemOffset = 0;
	UINT indexOffset = 0, vertexOffset = 0; //adjust in BuildSingleGeometry()
	mRenderItems.push_back(BuildSingleGeometry(box, mGeometry.get(), vertices, vertexOffset, indices, indexOffset, 0, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
	mRenderItems.push_back(BuildSingleGeometry(objData, mGeometry.get(), vertices, vertexOffset, indices, indexOffset, 1, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
	for (auto& e : mRenderItems)
	{
		mOpaqueRenderItems.push_back(e.get());
	}
	//Draw linelist objects
	renderItemOffset += mRenderItems.size();
	mRenderItems.push_back(BuildSingleGeometry(grid, mGeometry.get(), vertices, vertexOffset, indices, indexOffset, 2, D3D_PRIMITIVE_TOPOLOGY_LINELIST));
	for (int i = renderItemOffset; i < mRenderItems.size(); ++i)
	{
		mLineRenderItems.push_back(mRenderItems[i].get());
	}

	const UINT64 vbStride = sizeof(Vertex);
	const UINT64 vbByteSize = vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = indices.size() * sizeof(uint32_t);

	//Set necessary info
	mGeometry->vertexBufferByteSize = vbByteSize;
	mGeometry->vertexByteStride = sizeof(Vertex);
	mGeometry->indexFormat = DXGI_FORMAT_R32_UINT;
	mGeometry->indexBufferByteSize = ibByteSize;

	//Create resource on CPU
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mGeometry->vertexBufferCPU));
	CopyMemory(mGeometry->vertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mGeometry->indexBufferCPU));
	CopyMemory(mGeometry->indexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	//Committed to default heap intermediately. IASetVertex/IndexBuffer indicates the interpting ways
	CreateDefaultBuffer(mDevice.Get(), mCommandList.Get(), mGeometry->vertexBufferCPU->GetBufferPointer(), vbByteSize, mGeometry->vertexBufferGPU, mGeometry->vertexBufferUploader);
	CreateDefaultBuffer(mDevice.Get(), mCommandList.Get(), mGeometry->indexBufferCPU->GetBufferPointer(), ibByteSize, mGeometry->indexBufferGPU, mGeometry->indexBufferUploader);
}
void D3DToy::BuildSingleGroupGeometries()
{
	GeometryGenerator geoGen;
	std::vector<GeometryGenerator::MeshData> meshDataGroup;
	GeometryGenerator::MeshData allInOne;
	geoGen.ReadObjFile("assets\\models\\Homework\\Ai.obj", meshDataGroup);
	//vertex offsets
	UINT groupVertexOffset = 0;
	//index offsets
	UINT groupIndexOffset = 0;

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	UINT k = 0;
	mGeometry = std::make_unique<MeshGeometry>();
	mGeometry->Name = "Geometires";
	for (int i = 0; i < meshDataGroup.size(); ++i)
	{
		mRenderItems.push_back(BuildSingleGeometry(meshDataGroup[i], mGeometry.get(), vertices, groupVertexOffset, indices, groupIndexOffset, i, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
	}
	for (auto& e : mRenderItems)
	{
		mOpaqueRenderItems.push_back(e.get());
	}
	//Set necessary info
	const UINT64 vbStride = sizeof(Vertex);
	const UINT64 vbByteSize = vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = indices.size() * sizeof(uint32_t);

	mGeometry->vertexBufferByteSize = vbByteSize;
	mGeometry->vertexByteStride = sizeof(Vertex);
	mGeometry->indexFormat = DXGI_FORMAT_R32_UINT;
	mGeometry->indexBufferByteSize = ibByteSize;

	//Create resource on CPU
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mGeometry->vertexBufferCPU));
	CopyMemory(mGeometry->vertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mGeometry->indexBufferCPU));
	CopyMemory(mGeometry->indexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	//Committed to default heap intermediately. IASetVertex/IndexBuffer indicates the interpting ways
	CreateDefaultBuffer(mDevice.Get(), mCommandList.Get(), mGeometry->vertexBufferCPU->GetBufferPointer(), vbByteSize, mGeometry->vertexBufferGPU, mGeometry->vertexBufferUploader);
	CreateDefaultBuffer(mDevice.Get(), mCommandList.Get(), mGeometry->indexBufferCPU->GetBufferPointer(), ibByteSize, mGeometry->indexBufferGPU, mGeometry->indexBufferUploader);
}
std::unique_ptr<D3DToy::RenderItem> D3DToy::BuildSingleGeometry(GeometryGenerator::MeshData& meshData, MeshGeometry* geometry, std::vector<Vertex>& vertices, UINT& vertexOffset, std::vector<uint32_t>& indices, UINT& indexOffset,
	int objCBIndex, D3D12_PRIMITIVE_TOPOLOGY topology)
{
	SubmeshGeometry submesh;
	submesh.indexCount = meshData.indices.size();
	//indexOffset in buffer
	submesh.startIndexLocation = indexOffset;
	//vertexOffset in buffer
	submesh.baseVertexLocation = vertexOffset;

	for (size_t i = 0; i < meshData.vertices.size(); ++i) //
	{
		Vertex v;
		v.Pos = meshData.vertices[i].position;
		v.Normal = meshData.vertices[i].normal;
		vertices.push_back(v);
	}
	indices.insert(indices.end(), meshData.indices.begin(), meshData.indices.end());

	//Set submesh
	geometry->drawArgs[meshData.name] = submesh;

	auto renderItem = std::make_unique<RenderItem>();
	renderItem->objCBIndex = objCBIndex;
	renderItem->geo = geometry;
	renderItem->primitiveType = topology;
	renderItem->indexCount = renderItem->geo->drawArgs[meshData.name].indexCount;
	renderItem->startIndexLocation = renderItem->geo->drawArgs[meshData.name].startIndexLocation;
	renderItem->baseVertexLocation = renderItem->geo->drawArgs[meshData.name].baseVertexLocation;

	indexOffset += (UINT)meshData.indices.size();
	vertexOffset += (UINT)meshData.vertices.size();
	return renderItem;
}
void D3DToy::CreatePipelineStateObject()
{
	auto vertexShader = CompileShader(L"src\\Shaders\\DefaultShader.hlsl", nullptr, "VS", "vs_5_0");
	auto pixelShader = CompileShader(L"src\\Shaders\\DefaultShader.hlsl", nullptr, "PS", "ps_5_0");
	//INput element Desc
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout = {
		// SemanticName, SemanticIndex, Format, InputSlot,  AlignedByteOffset, InputSlotClass(PER VERTEX / INSTANCE), InstanceDataStepRate
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
	//Create shader desc
	psoDesc.VS = { reinterpret_cast<BYTE*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize() };
	psoDesc.PS = { reinterpret_cast<BYTE*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize() };
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
	ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOMap["triangle"])));

	//Grid PSO
	pixelShader = CompileShader(L"src\\Shaders\\GridPixelShader.hlsl", nullptr, "PS", "ps_5_0");
	psoDesc.PS = { reinterpret_cast<BYTE*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize() };
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOMap["line"])));
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
		UINT cbvIndex = mCurrentFrameResIndex * (UINT)mRenderItems.size() + ri->objCBIndex; //Changes from mOpaque to mRender
		auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
			mCBVHeap->GetGPUDescriptorHandleForHeapStart());
		cbvHandle.Offset(cbvIndex, mCBVDescSize);
		cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle);//CB

		//Should follow the sequency in CreateCBV?
		cbvIndex = mMaterialCbvOffset + mCurrentFrameResIndex;
		cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
			mCBVHeap->GetGPUDescriptorHandleForHeapStart());
		cbvHandle.Offset(cbvIndex, mCBVDescSize);
		cmdList->SetGraphicsRootDescriptorTable(1, cbvHandle);

		cmdList->DrawIndexedInstanced(ri->indexCount, 1,
			ri->startIndexLocation, ri->baseVertexLocation, 0);//VB IB
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
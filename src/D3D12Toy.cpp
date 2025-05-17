#include "stdafx.h"
#include "D3D12Toy.h"

D3DToy::D3DToy(UINT width, UINT height, std::wstring name) : DXSample(width, height, name) 
{
	mCam = std::make_unique<Camera>(
		width,
		height,
		500.0f,
		0.25f * XM_PI,
		10.0f,
		10000.0f,
		XMFLOAT3(0.0f, 1.0f, 0.0f),
		XMFLOAT4(0.0f, 100.0f, 0.0f, 1.0f)
	);
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
	mCam->OnMouseMove(xPos, yPos, updatePos);
}

void D3DToy::OnZoom(short delta)
{
	mCam->OnZoom(delta);
}
void D3DToy::OnKeyDown(UINT8 key)
{
	switch (key)
	{
	case 'S':
		if (mCurrentInitialPSO == mPSOMap["triangle"])
			mCurrentInitialPSO = mPSOMap["line"];
		else
			mCurrentInitialPSO = mPSOMap["triangle"];
		break;
	case VK_UP:
	{
		ObjEvent event;
		event.renderItem = mSpecialRenderItem;
		event.trans = XMMatrixIdentity();
		event.rotation = XMMatrixIdentity();
		event.scaling = XMMatrixScaling(2.0f, 2.0f, 2.0f);	
		mObjEventQueue.push(event);
		break;
	}
	case VK_DOWN:
	{
		ObjEvent event;
		event.renderItem = mSpecialRenderItem;
		event.trans = XMMatrixIdentity();
		event.rotation = XMMatrixIdentity();
		event.scaling = XMMatrixScaling(0.5f, 0.5f, 0.5f);
		mObjEventQueue.push(event);
		break;
	}
	default:
		break;
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
//Create Adapter
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
	CreateRTVAndDSVDescHeap();
//Organize geometry upload to default heap
//Essential info for render geometries
	BuildGeoAndMat();//VBV and IBV creating in DrawRenderItems()
	//BuildSingleGroupGeometries();
//Materials used for geometries
	SetLights();
//Create Frame Reousrces and their Constant Buffer descriptor heap. CB num depends on Per-obj constants(mat, geometry)
	CreateCBVAndSRVDescHeap();
//Shader resource and samplers(static/dynamic)
	CreateSamplerDescHeap();
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
		if (e->id == "box")
		{
			ObjEvent event;
			event.renderItem = e.get();
			event.trans = XMMatrixTranslation(210 * cos(2 * mTimer.CurrentTime()), 100.0f, 210 * sin(2 * mTimer.CurrentTime()));
			event.rotation = XMMatrixRotationY(mTimer.CurrentTime());
			event.scaling = XMMatrixIdentity();
			mObjEventQueue.push(event);
		}
	}
	ProcessObjEvent();
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
	//Update pass constants (pass, lights)

	XMMATRIX view, proj;
	mCam->OnUpdate(view, proj);
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

	mMainPassConst.eyePosWorld = mCam->mPosition;

	mMainPassConst.RTVSize = XMFLOAT2(mWidth, mHeight);
	mMainPassConst.invRTVSize = XMFLOAT2(1.0f / mWidth, 1.0f / mHeight);

	mMainPassConst.nearZ = mCam->nearZ;
	mMainPassConst.farZ = mCam->farZ;
	mMainPassConst.totalTime = mTimer.CurrentTime();

	mCurrentFrameRes->passCB->CopyData(0, mMainPassConst);
//Update light constants
	mLights.pointLights[0].position = XMFLOAT3(200 * cos(2 * mTimer.CurrentTime()), 100.0f, 200 * sin(2 * mTimer.CurrentTime()));//Between the cube and model
	mCurrentFrameRes->lightCB->CopyData(0, mLights);
}
void D3DToy::OnResize(UINT nWidth, UINT nHeight)
{
	DXSample::OnResize(nWidth, nHeight);
	mCam->OnResize(mWidth, mHeight);
}
void D3DToy::OnRender()
{
	//only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(mCurrentFrameRes->cmdAllocator->Reset());
	//command list can be reset after it has been added to the command queue 
	//set initial state(triangle) for next pass
	ThrowIfFailed(mCommandList->Reset(mCurrentFrameRes->cmdAllocator.Get(), mCurrentInitialPSO.Get()));

	mCommandList->RSSetViewports(1, &mCam->mViewport); //cannot specify multiple viewports to the same render target
	mCommandList->RSSetScissorRects(1, &mCam->mScissorRect); //cannot specify multiple scissor rectangles on the same render target

	//Indicate a state transition on the resource usage
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mSwapChainBuffer[mSwapChain->GetCurrentBackBufferIndex()].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET));

	//Clear back buffer and depth buffer
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
		mRTVDescHeap->GetCPUDescriptorHandleForHeapStart(),
		mSwapChain->GetCurrentBackBufferIndex(), // index to offset
		mRTVDescSize // byte size of descriptor
	);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(mDSVDescHeap->GetCPUDescriptorHandleForHeapStart());
	mCommandList->ClearRenderTargetView(rtvHandle, grey, 0, nullptr);
	mCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	//Specify the render buffer
	mCommandList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);

	//Set Root signature
	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
	//Set CBV
	ID3D12DescriptorHeap* descriptorHeaps[] = { mConstBufferDescHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	//using root descriptor instead of descriptor heap for single object per pass
	mCommandList->SetGraphicsRootConstantBufferView(2, mCurrentFrameRes->passCB->Resource()->GetGPUVirtualAddress());
	mCommandList->SetGraphicsRootConstantBufferView(3, mCurrentFrameRes->lightCB->Resource()->GetGPUVirtualAddress());

	DrawRenderItems(mCommandList.Get(), mOpaqueRenderItems);
	//Change pipelinestate
	mCommandList->SetPipelineState(mPSOMap["line"].Get());
	DrawRenderItems(mCommandList.Get(), mWireFrameRenderItems);

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
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;//DXGI_SWAP_EFFECT_DISCARD?
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; // when full-screen, display mode will be the one best matches the current application window dimensions
	
	auto& bufDesc = swapChainDesc.BufferDesc;
	bufDesc.Width = mWidth;
	bufDesc.Height = mHeight;
	bufDesc.Format = mBackBufferFormat;
	//bufDesc.RefreshRate.Numerator = 360;
	//bufDesc.RefreshRate.Denominator = 1; // Refreshrate = Numerator / Denominator
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
void D3DToy::CreateRTVAndDSVDescHeap()
{
	//Create Descriptor Heap
	D3D12_DESCRIPTOR_HEAP_DESC rtvDescHeap, dsvDescHeap;
	rtvDescHeap.NodeMask = 0; // GPU ID?
	rtvDescHeap.NumDescriptors = mBufferCount;
	rtvDescHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvDescHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(mDevice->CreateDescriptorHeap(&rtvDescHeap, IID_PPV_ARGS(&mRTVDescHeap)));

	dsvDescHeap.NodeMask = 0;
	dsvDescHeap.NumDescriptors = 1;
	dsvDescHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvDescHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(mDevice->CreateDescriptorHeap(&dsvDescHeap, IID_PPV_ARGS(&mDSVDescHeap)));
	//Create Desciptor Heap Handle
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescHandle(mRTVDescHeap->GetCPUDescriptorHandleForHeapStart(), mSwapChain->GetCurrentBackBufferIndex(), mRTVDescSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvDescHandle(mDSVDescHeap->GetCPUDescriptorHandleForHeapStart());
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
void D3DToy::CreateCBVAndSRVDescHeap()
{
	for (int i = 0; i < numFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(mDevice.Get(), 1, mRenderItems.size(), mMaterialItems.size()));
	}
	//Constant Buffer Figure
	//ObjDataFrame11 ¡­ ObjDataFrame3n | MaterialDataFrame 11 ¡­ MaterialDataFrame 3n | ShaderResourceFrame11 ¡­ ShaderResourceFrame3n
	UINT objCount = (UINT)mRenderItems.size();//Changes from mOpaque to mRenderItems
	UINT materialCount = (UINT)mMaterialItems.size();
	mMaterialCbvOffset = objCount * numFrameResources;
	mSRVOffset = mMaterialCbvOffset + materialCount * numFrameResources;
	//Constant Buffer descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NodeMask = 0; // GPU ID?
	cbvHeapDesc.NumDescriptors = numFrameResources * (objCount + materialCount) + mTextures.size();
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	ThrowIfFailed(mDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mConstBufferDescHeap)));

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
				mConstBufferDescHeap->GetCPUDescriptorHandleForHeapStart());
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
				mConstBufferDescHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCBVDescSize);
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = matCBByteSize;
			mDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}
	//SRV Descriptor
	int i = 0;
	for (auto& tex : mTextures)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		auto& texDesc = tex.second->resource->GetDesc();
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; //default component order
		srvDesc.Format = texDesc.Format;// Same as tex resource, compressed:DXGI_FORMAT_BC3_UNORM, etc
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = texDesc.MipLevels; //texture related
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		tex.second->diffuseSRVHeapIndex = i++;

		CD3DX12_CPU_DESCRIPTOR_HANDLE srDescriptorHeap(
			mConstBufferDescHeap->GetCPUDescriptorHandleForHeapStart());
		int heapIndex = mSRVOffset + tex.second->diffuseSRVHeapIndex;
		srDescriptorHeap.Offset(heapIndex, mCBVDescSize);
		mDevice->CreateShaderResourceView(tex.second->resource.Get(), &srvDesc, srDescriptorHeap);
	}
}
void D3DToy::CreateSamplerDescHeap()
{
	//Sampler Descriptor heap
	//D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
	//samplerHeapDesc.NumDescriptors = 1;
	//samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	//samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	//ThrowIfFailed(mDevice->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&mSamplerDescHeap)));
	////Sampler Descriptor
	//D3D12_SAMPLER_DESC samplerDesc = {};
	//samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
	//samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	//samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	//samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	//samplerDesc.MinLOD = 0;
	//samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	//samplerDesc.MipLODBias = 0.0f;
	//samplerDesc.MaxAnisotropy = 16;
	//samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;//Be used in shadow mapping

	//mDevice->CreateSampler(&samplerDesc, mSamplerDescHeap->GetCPUDescriptorHandleForHeapStart());

	//Static sampler
	const CD3DX12_STATIC_SAMPLER_DESC defaultSampler(
		0, //shader register/indx
		D3D12_FILTER_ANISOTROPIC, //Filter type
		D3D12_TEXTURE_ADDRESS_MODE_WRAP, //AddressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP, //AddressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP //AddressW
	);
	mStaticSamplers.push_back(defaultSampler);
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
	CD3DX12_ROOT_PARAMETER slotRootParameter[5] = {};
	//Create a single descriptor table of CBVs
	CD3DX12_DESCRIPTOR_RANGE cbvTable0 = {}, cbvTableMaterial = {}, srvTable = {};
	cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); //Set baseShaderRegister 0 -> buffer0(b0)
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0); //Num of descriptor range, descriptor range

	cbvTableMaterial.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);//buffer 1
	slotRootParameter[1].InitAsDescriptorTable(1, &cbvTableMaterial);//only one descriptor in this descriptor range

	slotRootParameter[2].InitAsConstantBufferView(2); //buffer 2

	slotRootParameter[3].InitAsConstantBufferView(3); //buffer 3

	srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); //srv buffer 0
	slotRootParameter[4].InitAsDescriptorTable(1, &srvTable, D3D12_SHADER_VISIBILITY_PIXEL);// important

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		5, 
		slotRootParameter, 
		mStaticSamplers.size(),
		mStaticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	); //Num of parameter,

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
void D3DToy::BuildGeoAndMat()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.BuildBox(10.0f, 10.0f, 10.0f);
	GeometryGenerator::MeshData grid = geoGen.BuildGrid(1000.0f, 1000.0f, 10, 10);

	std::vector<GeometryGenerator::MeshData> objMeshes;
	std::vector<MaterialLoader::Material> mtlList;
	geoGen.ReadObjFile("assets\\models\\Homework\\Test", "Amber.obj", objMeshes, mtlList);

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	mGeometries = std::make_unique<MeshGeometry>();
	mGeometries->Name = "Geometires";

	int renderItemOffset = 0;
	UINT indexOffset = 0, vertexOffset = 0; //adjust in BuildSingleGeometry()
	BuildSingleGeometry(mRenderItems, box, mGeometries.get(), vertices, vertexOffset, indices, indexOffset, 0, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	for (auto& mesh : objMeshes)
	{
		//Same objConstant buffer for now
		BuildSingleGeometry(mRenderItems, mesh, mGeometries.get(), vertices, vertexOffset, indices, indexOffset, 1, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}
	//Dirty ways
	mSpecialRenderItem = mRenderItems.back().get();

	for (auto& e : mRenderItems)
	{
		mOpaqueRenderItems.push_back(e.get());
	}

	//Draw linelist objects
	renderItemOffset += mRenderItems.size();
	BuildSingleGeometry(mRenderItems, grid, mGeometries.get(), vertices, vertexOffset, indices, indexOffset, 2, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	for (int i = renderItemOffset; i < mRenderItems.size(); ++i)
	{
		mWireFrameRenderItems.push_back(mRenderItems[i].get());
	}

	const UINT64 vbStride = sizeof(Vertex);
	const UINT64 vbByteSize = vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = indices.size() * sizeof(uint32_t);

	//Set necessary info
	mGeometries->vertexBufferByteSize = vbByteSize;
	mGeometries->vertexByteStride = sizeof(Vertex);
	mGeometries->indexFormat = DXGI_FORMAT_R32_UINT;
	mGeometries->indexBufferByteSize = ibByteSize;

	//Create resource on CPU
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mGeometries->vertexBufferCPU));
	CopyMemory(mGeometries->vertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mGeometries->indexBufferCPU));
	CopyMemory(mGeometries->indexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	//Committed to default heap intermediately. IASetVertex/IndexBuffer indicates the interpting ways
	CreateDefaultBuffer(mDevice.Get(), mCommandList.Get(), mGeometries->vertexBufferCPU->GetBufferPointer(), vbByteSize, mGeometries->vertexBufferGPU, mGeometries->vertexBufferUploader);
	CreateDefaultBuffer(mDevice.Get(), mCommandList.Get(), mGeometries->indexBufferCPU->GetBufferPointer(), ibByteSize, mGeometries->indexBufferGPU, mGeometries->indexBufferUploader);

	//Materials
	for (int i = 0; i < mtlList.size(); ++i)
	{
		auto& m = mtlList[i];
		auto material = std::make_unique<MaterialItem>();
		material->matCBIndex = i;
		material->texPath = m.texPath;
		material->matConsts.ambientAlbedo = XMFLOAT4(m.ka.x, m.ka.y, m.ka.z, 0.0f);
		material->matConsts.diffuseAlbedo = XMFLOAT4(m.kd.x, m.kd.y, m.kd.z, 0.0f);
		material->matConsts.specularAlbedo = XMFLOAT4(m.ks.x, m.ks.y, m.ks.z, 0.0f);
		material->matConsts.refraction = m.ni;
		material->matConsts.roughness = 1000.0f - min(1000.0f, m.ns); //transform to roughness.
		material->matConsts.hasTexture = 1;

		if (XMVector4Equal(XMLoadFloat4(&material->matConsts.ambientAlbedo), XMVectorZero()))
		{
			material->matConsts.ambientAlbedo = material->matConsts.diffuseAlbedo;
		}
		if (XMVector4Equal(XMLoadFloat4(&material->matConsts.diffuseAlbedo), XMVectorZero()))
		{
			material->matConsts.diffuseAlbedo = material->matConsts.ambientAlbedo;
		}
		//material->diffuseSRVHeapIndex
		if (mTextures.find(material->texPath) == mTextures.end())
		{
			auto tex = std::make_unique<Texture>();
			MaterialLoader::CreateTextureFromFile(material->texPath, mDevice, mCommandList, tex->resource, tex->uploadHeap);
			mTextures.emplace(material->texPath, std::move(tex));
		}
		mMaterialItems.emplace(m.mtlName, std::move(material));
	}
	auto defaultMtl = std::make_unique<MaterialItem>();
	defaultMtl->matCBIndex = mtlList.size();
	defaultMtl->matConsts.ambientAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
	defaultMtl->matConsts.diffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
	defaultMtl->matConsts.specularAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
	defaultMtl->matConsts.refraction = 1.0f;
	defaultMtl->matConsts.roughness = 1.0f; 
	defaultMtl->matConsts.hasTexture = 0;
	mMaterialItems.emplace("default", std::move(defaultMtl));
}
void D3DToy::SetLights()
{
	mLights.ambientLight = XMFLOAT4(0.1f, 0.1f, 0.1f, 0.0f);

	//mLights.pointLights[0].position = XMFLOAT3(200 * cos(2 * mTimer.CurrentTime()), 100.0f, 200 * sin(2 * mTimer.CurrentTime()));//Same as the cube
	mLights.pointLights[0].falloffStart = 300.0f;
	mLights.pointLights[0].falloffEnd = 1000.0f;
	mLights.pointLights[0].strength = XMFLOAT3(1.0f, 1.0f, 1.0f);
}
void D3DToy::CreatePipelineStateObject()
{
	//Compile at runtime
	//auto vertexShader = CompileShaderFromFile(L"src\\Shaders\\DefaultVertexShader.hlsl", nullptr, "VS", "vs_5_0");
	//auto pixelShader = CompileShaderFromFile(L"src\\Shaders\\DefaultPixelShader.hlsl", nullptr, "PS", "ps_5_0");

	//INput element Desc
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout = {
		// SemanticName, SemanticIndex, Format, InputSlot,  AlignedByteOffset, InputSlotClass(PER VERTEX / INSTANCE), InstanceDataStepRate
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXC", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
	//Create shader desc
	//psoDesc.VS = { reinterpret_cast<BYTE*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize() };
	//psoDesc.PS = { reinterpret_cast<BYTE*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize() };
	//gDefaultVertexShader, gDefaultPixelShader from inc file
	psoDesc.VS = { gDefaultVertexShader, sizeof(gDefaultVertexShader) };
	psoDesc.PS = { gDefaultPixelShader, sizeof(gDefaultPixelShader) };
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
	mCurrentInitialPSO = mPSOMap["triangle"];

	//Grid PSO
	psoDesc.PS = { gGridPixelShader, sizeof(gGridPixelShader) };
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOMap["line"])));
}
void D3DToy::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(
		mConstBufferDescHeap->GetGPUDescriptorHandleForHeapStart());
	CD3DX12_GPU_DESCRIPTOR_HANDLE handle = {};
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];
		cmdList->IASetVertexBuffers(0, 1, &ri->geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->primitiveType);
		// Offset to the CBV in the descriptor heap for this object and
		// for this frame resource.

		UINT cbvIndex = mCurrentFrameResIndex * mRenderItems.size() + ri->objCBIndex; //Changes from mOpaque to mRender
		handle.InitOffsetted(cbvHandle, cbvIndex, mCBVDescSize);
		cmdList->SetGraphicsRootDescriptorTable(0, handle);//CB

		auto& mat = mMaterialItems[ri->materialName];
		//Should follow the sequency in CreateCBV? (All objCB, then matCB)
		UINT matCBVIndex = mMaterialCbvOffset + mCurrentFrameResIndex * mMaterialItems.size() + mat->matCBIndex;
		handle.InitOffsetted(cbvHandle, matCBVIndex, mCBVDescSize);
		cmdList->SetGraphicsRootDescriptorTable(1, handle);

		//Shader Resource Buffer
		if (!mat->texPath.empty())
		{
			int heapIndex = mSRVOffset + mTextures[mat->texPath]->diffuseSRVHeapIndex;
			handle.InitOffsetted(cbvHandle, heapIndex, mCBVDescSize);
			cmdList->SetGraphicsRootDescriptorTable(4, handle);
		}

		cmdList->DrawIndexedInstanced(ri->indexCount, 1,
			ri->startIndexLocation, ri->baseVertexLocation, 0);//VB IB
	}
}
void D3DToy::ProcessObjEvent()
{
	std::lock_guard<std::mutex> lock(mEventQueueMutex);
	while (!mObjEventQueue.empty())
	{
		ObjEvent e = mObjEventQueue.front();
		mObjEventQueue.pop();

		ObjectConstants objConst;
		XMMATRIX world = XMMatrixIdentity();

		world = XMMatrixMultiply(e.trans, world);
		world = XMMatrixMultiply(e.rotation, world);

		XMMATRIX scaling = XMMatrixMultiply(e.scaling, XMLoadFloat4x4(&e.renderItem->scaling));
		XMStoreFloat4x4(&e.renderItem->scaling, XMMatrixTranspose(scaling));

		world = XMMatrixMultiply(scaling, world);

		XMStoreFloat4x4(&e.renderItem->world, XMMatrixTranspose(world));
		XMStoreFloat4x4(&objConst.world, XMMatrixTranspose(world));
		mCurrentFrameRes->objCB->CopyData(e.renderItem->objCBIndex, objConst);
		e.renderItem->numFramesDirty = numFrameResources - 1;
	}
}
void D3DToy::CheckFeatureSupport()
{
	//CD3DX12FeatureSupport::MultisampleQualityLevels
	// 
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

void D3DToy::BuildSingleGeometry(std::vector<std::unique_ptr<RenderItem>>& riList,
	GeometryGenerator::MeshData& meshData,
	MeshGeometry* geometry, 
	std::vector<Vertex>& vertices, UINT& vertexOffset, 
	std::vector<uint32_t>& indices, UINT& indexOffset,
	int objCBIndex, D3D12_PRIMITIVE_TOPOLOGY topology)
{
	for (size_t i = 0; i < meshData.vertices.size(); ++i) //
	{
		Vertex v;
		v.pos = meshData.vertices[i].position;
		v.normal = meshData.vertices[i].normal;
		v.texCoordinate = meshData.vertices[i].texCoordinate;
		//v.texCoordinate.y = 1 - v.texCoordinate.y; // ? v.texCoordinate.y *= -1
		vertices.push_back(v);
	}

	for (auto& e : meshData.idxGroups)
	{
		//Set submesh
		SubmeshGeometry submesh;
		submesh.indexCount = e.indices.size();
		//indexOffset in buffer
		submesh.startIndexLocation = indexOffset;
		//vertexOffset in buffer
		submesh.baseVertexLocation = vertexOffset;

		indices.insert(indices.end(), e.indices.begin(), e.indices.end());

		std::string subMeshName = meshData.name + "_" + e.mtlName;

		geometry->drawArgs[subMeshName] = submesh;

		auto renderItem = std::make_unique<RenderItem>();
		renderItem->objCBIndex = objCBIndex;
		renderItem->geo = geometry;
		renderItem->primitiveType = topology;
		renderItem->indexCount = renderItem->geo->drawArgs[subMeshName].indexCount;
		renderItem->startIndexLocation = renderItem->geo->drawArgs[subMeshName].startIndexLocation;
		renderItem->baseVertexLocation = renderItem->geo->drawArgs[subMeshName].baseVertexLocation;
		renderItem->materialName = e.mtlName;
		renderItem->id = meshData.name;

		riList.push_back(std::move(renderItem));

		indexOffset += (UINT)submesh.indexCount;
	}
	vertexOffset += (UINT)meshData.vertices.size();
}
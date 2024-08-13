#include "stdafx.h"
#include "D3D12Toy.h"

D3DToy::D3DToy(UINT width, UINT height, std::wstring name) : DXSample(width, height, name) 
{

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
//Check Feature support (4x MSAA here)
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS multiSampleLevel;
	multiSampleLevel.SampleCount = 4;
	multiSampleLevel.NumQualityLevels = 0; //Query num of quality levels
	multiSampleLevel.Format = mBackBufferFormat;
	multiSampleLevel.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	ThrowIfFailed(mDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &multiSampleLevel, sizeof(multiSampleLevel)));
	assert(multiSampleLevel.NumQualityLevels > 0);
//Create Command Allocator, List and Queue
	CreateCommandObjects();
//Create Swap Chain
	CreateSwapChain();
//Create descriptor heap and handle
//Create RTV using SwapChain buffer and descriptor heap handle
//Create Depth/Stencil buffer, DSV. Initialize DSV state
	CreateRTVAndDSVDescriptorHeap();
//Set Viewport
	mViewport.TopLeftX = 0.0f;
	mViewport.TopLeftY = 0.0f;
	mViewport.Width = static_cast<float>(mWidth);
	mViewport.Height = static_cast<float>(mHeight);
	mViewport.MinDepth = 0.0f;
	mViewport.MaxDepth = 1.0f;
	mCommandList->RSSetViewports(1, &mViewport); //cannot specify multiple viewports to the same render target
//Set Scissor Rectangles
	mCommandList->RSSetScissorRects(1, &mScissorRect); //cannot specify multiple scissor rectangles on the same render target
}
void D3DToy::OnUpdate()
{
	DXSample::OnUpdate();

}
void D3DToy::OnResize()
{

}
void D3DToy::OnRender()
{
	//only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(mCommandAllocator->Reset());
	//command list can be reset after it has been added to the command queue 
	ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));
	//Indicate a state transition on the resource usage
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mSwapChainBuffer[mSwapChain->GetCurrentBackBufferIndex()].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET));

	//Set viewport, scissorRect
	mCommandList->RSSetViewports(1, &mViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	//Clear back buffer and depth buffer  ????
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
		mRTVHeap->GetCPUDescriptorHandleForHeapStart(),
		mSwapChain->GetCurrentBackBufferIndex(), // index to offset
		mRTVDescSize // byte size of descriptor
	);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(mDSVHeap->GetCPUDescriptorHandleForHeapStart());
	const FLOAT clearColor[4] = {1.0f, 1.0f, 1.0f ,1.0f};
	mCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	mCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	//Specify the render buffer
	mCommandList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);
	//Transition
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
	// 
	//Wait commands to complete
	FlushCommandQueue();
}
void D3DToy::OnDestroy()
{

}

void D3DToy::FlushCommandQueue() //Or wait Commands to be excuted clearly
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
	bufDesc.RefreshRate.Numerator = 120;
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
	mCommandList->Close();

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
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc, dsvHeapDesc;
	rtvHeapDesc.NodeMask = 0; // GPU ID?
	rtvHeapDesc.NumDescriptors = mBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(mDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mRTVHeap)));

	dsvHeapDesc.NodeMask = 0;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(mDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&mDSVHeap)));
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
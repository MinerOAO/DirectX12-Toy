//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "stdafx.h"
#include "DXSample.h"

using namespace Microsoft::WRL;

DXSample::DXSample(UINT width, UINT height, std::wstring name) :
    mWidth(width),
    mHeight(height),
    m_title(name),
    m_useWarpDevice(false)
{
    mTimer = GameTimer();
}

DXSample::~DXSample()
{
    
}
void DXSample::OnResize(UINT nWidth, UINT nHeight)
{
	mWidth = nWidth;
	mHeight = nHeight;
}
// Helper function for acquiring the first available hardware adapter that supports Direct3D 12.
// If no such adapter can be found, *ppAdapter will be set to nullptr.
//_Use_decl_annotations_
//void DXSample::GetHardwareAdapter(IDXGIFactory1* pFactory, IDXGIAdapter1** ppAdapter, DXGI_GPU_PREFERENCE GPUPreference)
//{
//    *ppAdapter = nullptr;
//
//    ComPtr<IDXGIAdapter1> adapter;
//
//    ComPtr<IDXGIFactory6> factory6; // Introduce EnumAdapterByGpuPreference function. If not supported, fallback to IDXGIFactory1* pFactory
//    if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
//    {
//        for (
//            UINT adapterIndex = 0;
//            SUCCEEDED(factory6->EnumAdapterByGpuPreference(
//                adapterIndex, GPUPreference, IID_PPV_ARGS(&adapter)));
//            ++adapterIndex)
//        {
//            DXGI_ADAPTER_DESC1 desc;
//            adapter->GetDesc1(&desc);
//            
//            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
//            {
//                // Don't select the Basic Render Driver adapter.
//                // If you want a software adapter, pass in "/warp" on the command line.
//                continue;
//            }
//
//            // Check to see whether the adapter supports Direct3D 12, but don't create the
//            // actual device yet.
//            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
//            {
//                break;
//            }
//        }
//    }
//
//    if(adapter.Get() == nullptr)
//    {
//        for (UINT adapterIndex = 0; SUCCEEDED(pFactory->EnumAdapters1(adapterIndex, &adapter)); ++adapterIndex)
//        {
//            DXGI_ADAPTER_DESC1 desc;
//            adapter->GetDesc1(&desc);
//
//            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
//            {
//                // Don't select the Basic Render Driver adapter.
//                // If you want a software adapter, pass in "/warp" on the command line.
//                continue;
//            }
//
//            // Check to see whether the adapter supports Direct3D 12, but don't create the
//            // actual device yet.
//            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
//            {
//                break;
//            }
//        }
//    }
//    
//    *ppAdapter = adapter.Detach();
//}

// Helper function for setting the window's title text.
void DXSample::SetCustomWindowText(LPCWSTR text)
{
    std::wstring windowText = m_title + L": " + text + L" " + m_notification;
    SetWindowText(Win32Application::GetHwnd(), windowText.c_str());
}

// Helper function for parsing any supplied command line args.
_Use_decl_annotations_
void DXSample::ParseCommandLineArgs(WCHAR* argv[], int argc)
{
    for (int i = 1; i < argc; ++i)
    {
        if (_wcsnicmp(argv[i], L"-warp", wcslen(argv[i])) == 0 || 
            _wcsnicmp(argv[i], L"/warp", wcslen(argv[i])) == 0)
        {
            m_useWarpDevice = true;
            m_title = m_title + L" (WARP)";
        }
    }
}
void DXSample::OnInit()
{
    mTimer.OnReset();
}
void DXSample::OnUpdate()
{
    mTimer.Tick();
    CalculateFrameStats();
}
void DXSample::CalculateFrameStats()
{
    static int frameCount = 0;
    ++frameCount;
    if (mTimer.DeltaTime() >= 1.0f)
    {   
        // fps = frameCnt / 1
        std::wstring fpsStr = L"FPS:" + std::to_wstring(frameCount);
        SetCustomWindowText(fpsStr.c_str());
        frameCount = 0;
        mTimer.RecordPoint();
    }
}


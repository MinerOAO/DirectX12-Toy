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

#pragma once

#include "Tools/GameTimer.h"
#include "DXSampleHelper.h"
#include "Win32Application.h"

//Provide info about window and some functions
class DXSample 
{
public:
    DXSample(UINT width, UINT height, std::wstring name);
    virtual ~DXSample();

    virtual void OnInit() = 0;
    virtual void OnUpdate() = 0;
    virtual void OnResize() {}
    virtual void OnRender() = 0;
    virtual void OnDestroy() = 0;

    // Samples override the event handlers to handle specific messages.
    virtual void OnKeyDown(UINT8 /*key*/)   {}
    virtual void OnKeyUp(UINT8 /*key*/)     {}
    virtual void OnMouseMove(int xPos, int yPos, bool updatePos) {}
    virtual void OnZoom(short delta) {}

    // Accessors.
    UINT GetWidth() const           { return mWidth; }
    UINT GetHeight() const          { return mHeight; }
    const WCHAR* GetTitle() const   { return m_title.c_str(); }

    void ParseCommandLineArgs(_In_reads_(argc) WCHAR* argv[], int argc);

protected:
    void GetHardwareAdapter(_In_ IDXGIFactory1* pFactory, _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter, DXGI_GPU_PREFERENCE GPUPrefrence);

    void SetCustomWindowText(LPCWSTR text);

    void CalculateFrameStats();
    // Viewport dimensions.
    UINT mWidth;
    UINT mHeight;
    float m_aspectRatio;

    // Adapter info.
    bool m_useWarpDevice;
    //Timer
    GameTimer mTimer;
private:
    // Root assets path.
    std::wstring m_assetsPath;

    // Window title.
    std::wstring m_title;
};

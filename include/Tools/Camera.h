#pragma once
#include "stdafx.h"

using namespace DirectX;

class Camera
{
public:
	Camera(UINT width, UINT height, float r, float fov, float nZ, float fZ, XMFLOAT3 up, XMFLOAT4 target) : mWidth(width), mHeight(height), mRadius(r),
		mFOV(fov), nearZ(nZ), farZ(fZ), mUpVec(up), mLookAtTarget(target)
	{
		mLastMousePos = XMFLOAT2(0.0f, 0.0f);
		mAspectRatio = static_cast<float>(width) / static_cast<float>(height);
		XMStoreFloat4x4(&mView, XMMatrixIdentity());
		XMStoreFloat4x4(&mProj, XMMatrixPerspectiveFovLH(mFOV, mAspectRatio, nearZ, farZ));

		//Set viewport, scissorRect
		mViewport.TopLeftX = 0.0f;
		mViewport.TopLeftY = 0.0f;
		mViewport.Width = static_cast<float>(mWidth);
		mViewport.Height = static_cast<float>(mHeight);
		mViewport.MinDepth = 0.0f;
		mViewport.MaxDepth = 1.0f;

		mScissorRect = { 0, 0, static_cast<long>(mWidth), static_cast<long>(mHeight) };
	}
	void OnUpdate(XMMATRIX& v, XMMATRIX& p);
	void OnZoom(short delta);
	void OnMouseMove(int xPos, int yPos, bool updatePos);
	void OnResize(UINT width, UINT height);

	UINT mWidth, mHeight;
	float mAspectRatio = 0.0f;

	float mFOV;
	float nearZ ;
	float farZ;

	//non-copyable
	D3D12_VIEWPORT mViewport; //Reset whenever commandlist is reset
	D3D12_RECT mScissorRect;	//Reset whenever commandlist is reset

	XMFLOAT4 mPosition;
	XMFLOAT4 mLookAtTarget;
	XMFLOAT3 mUpVec;

	float mPhi = 0.0f; //y
	float mTheta = 0.0f; //xz plane
	float mRadius; //r

	const float zoomSense = 0.1f; // Speed of zooming
	const float mouseSense = 0.25f; //Speed of mouse
private:
	XMFLOAT4X4 mView, mProj;
	XMFLOAT2 mLastMousePos;
};
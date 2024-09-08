#include "Tools/Camera.h"
void Camera::OnUpdate(XMMATRIX& v, XMMATRIX& p)
{
	// sin* cos, cos, sin * sin?
	// cos * cos, sin, cos * sin?
	XMVECTOR position = XMVectorSet(mRadius * cosf(mPhi) * cosf(mTheta), mRadius * sinf(mPhi), -mRadius * cosf(mPhi) * sinf(mTheta), 0.0f);
	XMVECTOR target = XMLoadFloat4(&mLookAtTarget);
	position += target;
	XMVECTOR up = XMLoadFloat3(&mUpVec);

	v = XMMatrixLookAtLH(position, target, up);
	p = XMLoadFloat4x4(&mProj);
}
void Camera::OnResize()
{
	//When resized, compute projection matrix
	XMStoreFloat4x4(&mProj, XMMatrixPerspectiveFovLH(mFOV, mAspectRatio, nearZ, farZ));
}
void Camera::OnZoom(short delta)
{
	mRadius -= delta * zoomSense;
	if (mRadius < 0.1f)
	{
		mRadius = 0.1f;
	}
	//view matrix update in OnUpdate() pass constants.
}
void Camera::OnMouseMove(int xPos, int yPos, bool updatePos)
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
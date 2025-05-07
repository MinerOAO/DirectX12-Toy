#include <Tools/GameTimer.h>
#include <stdafx.h>

GameTimer::GameTimer()
{
	__int64 frequency;
	QueryPerformanceFrequency((LARGE_INTEGER*) & frequency);
	mSecondsPerCount = 1.0f / static_cast<float>(frequency);
};
void GameTimer::OnReset()
{
	QueryPerformanceCounter((LARGE_INTEGER*)&mCurrentTime);
	mStartTime = mRecordTime = mCurrentTime;
	mDeltaTime = 0;
}
void GameTimer::Tick()
{
	QueryPerformanceCounter((LARGE_INTEGER*)&mCurrentTime);
	mDeltaTime = mCurrentTime - mRecordTime;
	// Force nonnegative. The DXSDK¡¯s CDXUTTimer mentions that if the 
	// processor goes into a power save mode or we get shuffled to
	// another processor, then mDeltaTime can be negative.
	if (mDeltaTime < 0.0)
	{
		mDeltaTime = 0.0;
	}
}
void GameTimer::RecordPoint()
{
	QueryPerformanceCounter((LARGE_INTEGER*)&mRecordTime);
}
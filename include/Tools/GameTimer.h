#pragma once

class GameTimer
{
public:
	GameTimer();

	inline float GameTimer::CurrentTime() const
	{
		return static_cast<float>(mCurrentTime) * mSecondsPerCount;
	}
	inline float GameTimer::DeltaTime() const
	{
		return static_cast<float>(mDeltaTime) * mSecondsPerCount;
	}

	void OnReset();
	void Tick();
	void RecordPoint();

private:
	float mSecondsPerCount = 0.0f;
	float mDeltaTime = 0.0f;

	__int64 mStartTime = 0;
	__int64 mCurrentTime = 0;
	__int64 mRecordTime = 0;

};
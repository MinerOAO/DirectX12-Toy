#pragma once

class GameTimer
{
public:
	GameTimer();

	inline float GameTimer::CurrentTime() const
	{
		return static_cast<float>(mCurrentTime);
	}
	inline float GameTimer::DeltaTime() const
	{
		return static_cast<float>(mDeltaTime);
	}

	void OnReset();
	void Tick();
	void RecordPoint();

private:
	float mSecondsPerCount;
	float mDeltaTime;

	__int64 mStartTime;
	__int64 mCurrentTime;
	__int64 mRecordTime;

};
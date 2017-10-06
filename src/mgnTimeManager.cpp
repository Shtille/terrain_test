#include "mgnTimeManager.h"

#include "mgnTime.h"

#include <cstdlib>

namespace mgn {

    Timer::Timer(mgnU32_t interval)
    : mNext(NULL)
    , mInterval(interval)
    , mTime(0)
    , mEnabled(false)
    {

    }
    Timer::~Timer()
    {

    }
    void Timer::Reset()
    {
        mTime = 0U;
    }
    void Timer::Start()
    {
        mEnabled = true;
    }
    void Timer::Stop()
    {
        mEnabled = false;
    }
    bool Timer::HasExpired() const
    {
        return mTime >= mInterval;
    }
    bool Timer::IsEnabled() const
    {
        return mEnabled;
    }
    mgnU32_t Timer::GetInterval() const
    {
        return mInterval;
    }
    mgnU32_t Timer::GetTime() const
    {
        return mTime;
    }

    TimeManager * TimeManager::mInstance = NULL;

    TimeManager::TimeManager()
    : mLastTime(0)
    , mFrameTime(0)
    , mFrameRate(0.0f)
    , mFpsCounterTime(0.0f)
    , mFpsCounterCount(0.0f)
    , mTimerHead(NULL)
    {

    }
    TimeManager::~TimeManager()
    {
        Timer * timer = mTimerHead;
        while (timer)
        {
            Timer * timer_to_delete = timer;
            timer = timer->mNext;
            delete timer_to_delete;
        }
    }
    void TimeManager::CreateInstance()
    {
        mInstance = new TimeManager();
    }
    void TimeManager::DestroyInstance()
    {
        delete mInstance;
    }
    TimeManager * TimeManager::GetInstance()
    {
        return mInstance;
    }
    void TimeManager::Update()
    {
        mgnU32_t current_time = mgnGetTickCount();
        mFrameTime = current_time - mLastTime;
        mLastTime = current_time;

#if defined(_DEBUG) || defined(DEBUG)
        // Clamp update value when debugging step by step
        if (mFrameTime > 1000)
            mFrameTime = 16;
#endif

        // Update all timers
        Timer * timer = mTimerHead;
        while (timer)
        {
            if (timer->mEnabled)
                timer->mTime += mFrameTime;
            timer = timer->mNext;
        }

        // Compute current frame rate
        if (mFpsCounterTime < 1.0f)
        {
            mFpsCounterCount += 1.0f;
            mFpsCounterTime += (float)mFrameTime / 1000.0f; // from ms to seconds
        }
        else
        {
            mFrameRate = mFpsCounterCount / mFpsCounterTime;
            mFpsCounterCount = 0.0f;
            mFpsCounterTime = 0.0f;
        }
    }
    Timer * TimeManager::AddTimer(mgnU32_t interval)
    {
        Timer * timer = new Timer(interval);
        timer->mNext = mTimerHead;
        mTimerHead = timer;
        return timer;
    }
    void TimeManager::RemoveTimer(Timer * removed_timer)
    {
        Timer * prev = NULL;
        Timer * timer = mTimerHead;
        while (timer)
        {
            if (timer == removed_timer)
            {
                if (prev)
                    prev->mNext = timer->mNext;
                else
                    mTimerHead = timer->mNext;
                delete timer;
                break;
            }
            prev = timer;
            timer = timer->mNext;
        }
    }
    mgnU32_t TimeManager::GetTime() const
    {
        return mLastTime;
    }
    mgnU32_t TimeManager::GetFrameTime() const
    {
        return mFrameTime;
    }
    float TimeManager::GetFrameRate() const
    {
        return mFrameRate;
    }

} // namespace mgn
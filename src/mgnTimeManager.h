#pragma once
#ifndef __MGN_TIME_MANAGER_H__
#define __MGN_TIME_MANAGER_H__

#include "mgnBaseType.h"

namespace mgn {

    class Timer {
        friend class TimeManager;
    public:
        void Reset();
        void Start();
        void Stop();

        bool HasExpired() const;
        bool IsEnabled() const;
        mgnU32_t GetInterval() const;
        mgnU32_t GetTime() const;

    private:
        Timer(mgnU32_t interval);
        ~Timer();

        Timer * mNext;
        mgnU32_t mInterval;
        mgnU32_t mTime;
        bool mEnabled;
    };

    class TimeManager {
    public:
        TimeManager();
        ~TimeManager();

        void Update();

        Timer * AddTimer(mgnU32_t interval); //!< interval is in ms
        void RemoveTimer(Timer * removed_timer);

        mgnU32_t GetTime() const; //!< returns stored global time, ms
        mgnU32_t GetFrameTime() const; //!< returns time between two updates, ms
        float GetFrameRate() const; //!< returns number of frames per second (FPS)

    private:

        mgnU32_t mLastTime;
        mgnU32_t mFrameTime; //!< also known as "frame time"
        float mFrameRate; //!< FPS
        float mFpsCounterTime;  //!< for counting FPS
        float mFpsCounterCount; //!< for counting FPS

        Timer * mTimerHead;
    };

} // namespace mgn

#endif
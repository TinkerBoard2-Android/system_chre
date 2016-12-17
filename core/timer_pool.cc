/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "chre/core/event_loop.h"
#include "chre/core/timer_pool.h"
#include "chre/platform/fatal_error.h"
#include "chre/platform/system_time.h"

namespace chre {

TimerPool::TimerPool(EventLoop& eventLoop)
    : mEventLoop(eventLoop) {
  if (!mSystemTimer.init()) {
    FATAL_ERROR("Failed to initialize a system timer for the TimerPool");
  }
}

TimerHandle TimerPool::setTimer(const Nanoapp *nanoapp, Nanoseconds duration,
    const void *cookie, bool isOneShot) {
  ASSERT(nanoapp);

  std::lock_guard<Mutex> lock(mMutex);

  TimerRequest timerRequest;
  timerRequest.requestingNanoapp = nanoapp;
  timerRequest.timerHandle = generateTimerHandle();
  timerRequest.expirationTime = SystemTime::getMonotonicTime() + duration;
  timerRequest.duration = duration;
  timerRequest.isOneShot = isOneShot;
  timerRequest.cookie = cookie;
  size_t insertionIndex = insertTimerRequest(timerRequest);

  LOGI("App %" PRIx64 " requested timer with duration %" PRIu64 "ns",
      nanoapp->getAppId(), duration.toRawNanoseconds());

  // The newly created timer has the earliest expiration time. We cancel the
  // current timer and schedule the first timer. This also handles the case of a
  // currently idle system timer.
  if (insertionIndex == 0) {
    if (mSystemTimer.isActive()) {
      mSystemTimer.cancel();
    }

    mSystemTimer.set(handleSystemTimerCallback, this, duration);
  }

  return timerRequest.timerHandle;
}

bool TimerPool::cancelTimer(const Nanoapp *nanoapp, TimerHandle timerHandle) {
  ASSERT(nanoapp);
  std::lock_guard<Mutex> lock(mMutex);

  size_t index;
  bool success = false;
  TimerRequest *timerRequest = getTimerRequestByTimerHandle(timerHandle,
      &index);

  if (timerRequest == nullptr) {
    LOGW("Failed to cancel timer ID %" PRIu32 ": not found", timerHandle);
  } else if (timerRequest->requestingNanoapp != nanoapp) {
    LOGW("Failed to cancel timer ID %" PRIu32 ": permission denied",
         timerHandle);
  } else {
    TimerHandle cancelledTimerHandle = timerRequest->timerHandle;
    mTimerRequests.erase(index);
    if (index == 0) {
      if (mSystemTimer.isActive()) {
        mSystemTimer.cancel();
      }

      handleExpiredTimersAndScheduleNext();
    }

    LOGD("App %" PRIx64 " cancelled timer %" PRIu32, nanoapp->getAppId(),
         cancelledTimerHandle);
    success = true;
  }

  return success;
}

TimerPool::TimerRequest *TimerPool::getTimerRequestByTimerHandle(
    TimerHandle timerHandle, size_t *index) {
  for (size_t i = 0; i < mTimerRequests.size(); i++) {
    if (mTimerRequests[i].timerHandle == timerHandle) {
      if (index != nullptr) {
        *index = i;
      }
      return &mTimerRequests[i];
    }
  }

  return nullptr;
}

TimerHandle TimerPool::generateTimerHandle() {
  TimerHandle timerHandle;
  if (mGenerateTimerHandleMustCheckUniqueness) {
    timerHandle = generateUniqueTimerHandle();
  } else {
    timerHandle = mLastTimerHandle + 1;
    if (timerHandle == CHRE_TIMER_INVALID) {
      // TODO: Consider that uniqueness checking can be reset when the number of
      // timer requests reaches zero.
      mGenerateTimerHandleMustCheckUniqueness = true;
      timerHandle = generateUniqueTimerHandle();
    }
  }

  mLastTimerHandle = timerHandle;
  return timerHandle;
}

TimerHandle TimerPool::generateUniqueTimerHandle() {
  size_t timerHandle = mLastTimerHandle;
  while (1) {
    timerHandle++;
    if (timerHandle != CHRE_TIMER_INVALID) {
      TimerRequest *timerRequest = getTimerRequestByTimerHandle(timerHandle);
      if (timerRequest == nullptr) {
        return timerHandle;
      }
    }
  }
}

size_t TimerPool::insertTimerRequest(const TimerRequest& timerRequest) {
  // Try to insert the timer request into the list by iterating through all but
  // the last request.
  size_t insertionIndex = mTimerRequests.size();

  if (mTimerRequests.size() > 0) {
    for (size_t i = 0; i < mTimerRequests.size() - 1; i++) {
      if (timerRequest.expirationTime >= mTimerRequests[i].expirationTime
          && timerRequest.expirationTime < mTimerRequests[i + 1].expirationTime) {
        insertionIndex = i;
        break;
      }
    }
  }

  // If the timer request was not inserted, simply append it to the list.
  if (!mTimerRequests.insert(insertionIndex, timerRequest)) {
    FATAL_ERROR("Failed to insert a timer request. Out of memory");
  }
  return insertionIndex;
}

void TimerPool::onSystemTimerCallback() {
  // Gain exclusive access to the timer pool. This is needed because the context
  // of this callback is not defined.
  std::lock_guard<Mutex> lock(mMutex);
  if (!handleExpiredTimersAndScheduleNext()) {
    LOGE("Timer callback invoked with no outstanding timers");
  }
}

bool TimerPool::handleExpiredTimersAndScheduleNext() {
  bool eventWasPosted = false;
  while (!mTimerRequests.empty()) {
    Nanoseconds currentTime = SystemTime::getMonotonicTime();
    if (currentTime >= mTimerRequests[0].expirationTime) {
      // Post an event for an expired timer.
      TimerRequest *currentTimerRequest = &mTimerRequests[0];
      mEventLoop.postEvent(CHRE_EVENT_TIMER,
          const_cast<void *>(currentTimerRequest->cookie), nullptr,
          kSystemInstanceId,
          currentTimerRequest->requestingNanoapp->getInstanceId());
      eventWasPosted = true;

      // Reschedule the timer if needed.
      if (!currentTimerRequest->isOneShot) {
        currentTimerRequest->expirationTime = currentTime
            + currentTimerRequest->duration;
        insertTimerRequest(*currentTimerRequest);
      }

      // Release the current request.
      mTimerRequests.erase(0);
    } else {
      Nanoseconds duration = mTimerRequests[0].expirationTime - currentTime;
      mSystemTimer.set(handleSystemTimerCallback, this, duration);
      break;
    }
  }

  return eventWasPosted;
}

void TimerPool::handleSystemTimerCallback(void *timerPoolPtr) {
  // Cast the context pointer to a TimerPool context and call into the callback
  // handler.
  TimerPool *timerPool = static_cast<TimerPool *>(timerPoolPtr);
  timerPool->onSystemTimerCallback();
}

}  // namespace chre
/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef CHRE_PLATFORM_PLATFORM_AUDIO_H_
#define CHRE_PLATFORM_PLATFORM_AUDIO_H_

#include "chre_api/chre/audio.h"
#include "chre/target_platform/platform_audio_base.h"
#include "chre/util/non_copyable.h"

namespace chre {

/**
 * Defines the common interface to audio functionality.
 */
class PlatformAudio : public PlatformAudioBase,
                      public NonCopyable {
 public:
  /**
   * Initializes the audio subsystem. This must be called as part of the
   * initialization of the runtime.
   */
  static void init();

  /**
   * Deinitializes the audio subsystem, including releasing any outstanding
   * audio requests. This must be called as part of deinitialization of the
   * runtime.
   */
  static void deinit();

  /**
   * Obtains the audio source description for a given handle.
   *
   * @param handle the handle for the requested audio source.
   * @param audioSource the chreAudioSource to populate with details of the
   *     audio source. This pointer must never be null.
   */
  static bool getAudioSource(uint32_t handle, chreAudioSource *audioSource);
};

}  // namespace chre

#endif  // CHRE_PLATFORM_PLATFORM_AUDIO_H_

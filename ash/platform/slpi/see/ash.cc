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

#include "ash_api/ash.h"

bool ashSetCalibration(uint8_t sensorType, const struct ashCalInfo *calInfo) {
  // TODO(P1-fa2c16): Implement this.
  return false;
}

bool ashLoadCalibrationParams(uint8_t sensorType, uint8_t storage,
                              struct ashCalParams *params) {
  // TODO(P1-7a3c5b): Implement this.
  return false;
}

bool ashSaveCalibrationParams(uint8_t sensorType,
                              const struct ashCalParams *params) {
  // TODO(P1-f70a76): Implement this.
  return false;
}

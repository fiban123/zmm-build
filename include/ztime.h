/*
 * zmm-build: A compiled C build system
 * Copyright 2026 fiban123
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "export.h"
#include "num.h"

/**
 * Provides timestamp measuring functions. Useful for measuring compilation time.
 */

/**
 * Returns the current time in milliseconds.
 */
API u64 zmm_time_ms();

/**
 * Returns the current time in microseconds.
 */
API u64 zmm_time_us();

/**
 * Returns the current time in nanoseconds.
 */
API u64 zmm_time_ns();

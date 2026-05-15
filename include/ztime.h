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

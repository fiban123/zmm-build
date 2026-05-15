/*
 * zmm-build: A compiled C build system
 * Copyright (C) 2026 @fiban123
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
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

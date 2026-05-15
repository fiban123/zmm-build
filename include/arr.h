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

/**
 * This exists purely to explicitly denote what is a dynamic array
 * and what is a regular pointer.
 */

/**
 * Declares a dynamic array of type T.
 * 
 * [Note] Uses stb_ds.h under the hood.
 */
#define arr(T) T*

/**
 * Initializer for dynamic arrays.
 */
#define arrinit NULL

/**
 * Declares a string hash map with values of type T.
 * 
 * [Note] Uses stb_ds.h under the hood.
 */
#define stringhm(T) T*

/**
 * Initializer for string hash maps.
 */
#define stringhm_init NULL

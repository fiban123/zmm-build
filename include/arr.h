#pragma once

/**
 * Declares a dynamic array of type T.
 * Uses stb_ds.h under the hood.
 */
#define arr(T) T*

/**
 * Initializer for dynamic arrays.
 */
#define arrinit NULL

/**
 * Declares a string hash map with values of type T.
 * Uses stb_ds.h under the hood.
 */
#define stringhm(T) T*

/**
 * Initializer for string hash maps.
 */
#define stringhm_init NULL

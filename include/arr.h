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

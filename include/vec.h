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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "export.h"
#include "num.h"

typedef union {
    struct {
        usize size;
        usize cap;
    } md;
    long long alignment_long;
    double alignment_double;
    void* alignment_ptr;
} VecHeader;

#define arrsize(arr) (sizeof(arr) / sizeof((arr)[0]))

#define vec(T) T*

#define vecheader(v) (((VecHeader*)(v)) - 1)

#define vecsize(v) ((v) ? vecheader(v)->md.size : 0)
#define veccap(v) ((v) ? vecheader(v)->md.cap : 0)

#define vecsetsize(v, s) (vecheader(v)->md.size = (s))

#define vecreserve(v, n) \
    (veccap(v) < (usize)(n) ? ((v) = zmm_vec_grow((v), sizeof(*(v)), (n))) : 0)

#define vecpush(v, x) \
    (vecreserve((v), vecsize(v) + 1), ((v)[vecheader(v)->md.size++] = (x)))

#define vecpushn(v, xarr, n)                               \
    (vecreserve((v), vecsize(v) + (n)),                    \
     memcpy((v) + vecsize(v), (xarr), (n) * sizeof(*(v))), \
     vecheader(v)->md.size += (n))

#define vecpop(v) ((v)[--vecheader(v)->md.size])

#define vecfree(v) ((v) ? (free(vecheader(v)), (v) = NULL) : 0)

API void* zmm_vec_grow(void* v, usize elem_size, usize required);

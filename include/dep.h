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

#include "arr.h"
#include "export.h"
#include "slice.h"

/**
 * Parses a compiler dependency file (.d) and appends dependencies
 * as slices to an existing array.
 * 
 * @param deps Pointer to an stb_ds array of SliceU8. Can point to NULL initially.
 * @param path Path to the dependency file.
 * @return 0 on success, -1 if the file could not be read.
 */
API i32 zmm_dep_parse(arr(SliceU8) * deps, SliceCU8 path);

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

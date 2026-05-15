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

#include "arr.h"
#include "export.h"
#include "slice.h"

/**
 * Parses a compiler dependency file (.d) and returns an array of dependencies as slices.
 * @param path Path to the dependency file.
 * @return stb_ds array of SliceU8. The array and each slice's ptr must be freed.
 */
API arr(SliceU8) zmm_dep_parse(SliceCU8 path);

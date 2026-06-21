#include "vec.h"

#include <stdio.h>
#include <stdlib.h>

void* zmm_vec_grow(void* v, size_t elem_size, usize required) {
    size_t new_cap = 1;

    while (required > new_cap) {
        new_cap *= 2;
    }

    size_t new_size = sizeof(VecHeader) + (new_cap * elem_size);

    // If v is not NULL, get the actual start of the allocated block
    VecHeader* hdr = (v) ? vecheader(v) : NULL;

    VecHeader* new_hdr = realloc(hdr, new_size);
    if (!new_hdr) {
        return NULL;
    }

    new_hdr->md.cap = new_cap;
    if (!v) {
        new_hdr->md.size = 0;
    }

    return (void*)(new_hdr + 1);
}

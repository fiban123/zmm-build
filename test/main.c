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

#include <stdio.h>
#include "builder.h"

int dummy_builder(const StringView* sources, usize num_sources, StringView output) {
    (void)sources;
    (void)num_sources;
    (void)output;
    return 0;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    BuildGraph bg;
    zmm_bg_init(&bg);

    StringView src1 = strv("src/main.c");
    StringView src2 = strv("src/helper.c");
    StringView header = strv("include/helper.h");
    
    StringView obj1 = strv("build/main.o");
    StringView obj2 = strv("build/helper.o");
    
    StringView target = strv("build/app.out");

    // Add main.o depending on main.c and helper.h
    StringView deps1[] = { header };
    zmm_bg_add(&bg, &src1, 1, obj1, deps1, 1, dummy_builder, false);

    // Add helper.o depending on helper.c
    zmm_bg_add(&bg, &src2, 1, obj2, NULL, 0, dummy_builder, false);

    // Add app.out depending on main.o and helper.o
    StringView objs[] = { obj1, obj2 };
    zmm_bg_add(&bg, objs, 2, target, NULL, 0, dummy_builder, false);

    // Prepare target
    zmm_bg_prepare(&bg, &target, 1);

    // Visualize
    int res = zmm_bg_visualize(&bg, "test_graph.bg");
    if (res == 0) {
        printf("Successfully wrote test_graph.bg\n");
    } else {
        printf("Failed to write test_graph.bg\n");
    }

    zmm_bg_free(&bg);
    return res;
}


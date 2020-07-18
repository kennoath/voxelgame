#include "chunk_common.h"

void chunk_print(chunk c) {
    printf("x: %d y: %d z: %d\n blocks ptr: %p\n", spread(c.key), c.blocks);
}

chunk_rngs chunk_rngs_init(int64_t seed) {
    const int vec = 987234;
    return (chunk_rngs) {
        .noise_lf_heightmap = n2d_create(seed, 2, 0.01, 2, 50, 0.5),
        .noise_hf_heightmap = n2d_create(seed*vec, 3, 0.04, 2, 12.5, 0.5),
        .noise_cliff_carver = n3d_create(seed*vec*vec, 3, 0.03, 2, 20, 0.5),
        .noise_cave_carver = n3d_create(seed*vec*vec*vec, 4, 0.05, 2, 10, 0.7),
    };
}

vec3i chunk_1d_to_3d(int idx) {
    vec3i ret;
    ret.x = idx / CHUNK_RADIX_2;
    ret.y = (idx / CHUNK_RADIX) % CHUNK_RADIX;
    ret.z = idx % CHUNK_RADIX;
    return ret;
}

int chunk_3d_to_1d(vec3i pos) {
    return pos.x * CHUNK_RADIX_2 + pos.y * CHUNK_RADIX + pos.z;
}

// for checking if neighbour exists
// maybe not super efficient but who cares
bool neighbour_exists(vec3i pos, int direction) {
    if (direction == PLUS_X) {
        return pos.x < CHUNK_MAX;
    } else if (direction == MINUS_X) {
        return pos.x > 0;
    } else if (direction == PLUS_Y) {
        return pos.y < CHUNK_MAX;
    } else if (direction == MINUS_Y) {
        return pos.y > 0;
    } else if (direction == PLUS_Z) {
        return pos.z < CHUNK_MAX;
    } else if (direction == MINUS_Z) {
        return pos.z > 0;
    }
    panicf("shouldnt happen, direction %d\n", direction);
    return false;
}

chunk chunk_generate(chunk_manager *cm, chunk_rngs noise, int x, int y, int z) {
    block_tag *blocks = calloc(sizeof(block_tag), CHUNK_RADIX_3);
    uint8_t *block_light = calloc(sizeof(uint8_t), CHUNK_RADIX_3);
    uint8_t *sky_light = calloc(sizeof(uint8_t), CHUNK_RADIX_3);
    chunk c = {0};
    c.blocks = blocks;
    c.sky_light_levels = sky_light;
    c.block_light_levels = block_light;

    c.key = (vec3i) {x,y,z};

    float chunk_x = x*CHUNK_RADIX;
    float chunk_y = y*CHUNK_RADIX;
    float chunk_z = z*CHUNK_RADIX;

    for (int idx = 0; idx < CHUNK_RADIX_3; idx++) {
        vec3i block_pos = chunk_1d_to_3d(idx);

        double block_x = chunk_x + block_pos.x;
        double block_y = chunk_y + block_pos.y;
        double block_z = chunk_z + block_pos.z;

        float height_low = n2d_sample(&noise.noise_lf_heightmap, block_x, block_z);
        float height_high = n2d_sample(&noise.noise_hf_heightmap, block_x, block_z);
        float cliff_carve_density = n3d_sample(&noise.noise_cliff_carver, block_x, block_y, block_z);
        float cave_carve_density = n3d_sample(&noise.noise_cliff_carver, block_x, block_y, block_z);

        float cave_cutoff = remap(64, -80, -10, 2, block_y);

        block_tag place_block;

        if (cave_carve_density < cave_cutoff) {
            place_block = BLOCK_AIR; goto SET_BLOCK;
        }

        float height;
        if (cliff_carve_density < 1) {
            height = height_low;
        } else {
            height = height_low + height_high;
        }

        if (cave_carve_density < cave_cutoff + 0.05 && block_y < height) {
            place_block = BLOCK_GEMS; goto SET_BLOCK;
        }

        // grass and stuff
        if (block_y < height - 4) {
            place_block = BLOCK_STONE; goto SET_BLOCK;
        } else if (block_y < height - 0.5) {
            // not stone layers
            if (height > -25) {
                place_block = BLOCK_DIRT; goto SET_BLOCK;
            } else {
                place_block = BLOCK_SAND; goto SET_BLOCK;
            }
        } else if (block_y < height + 0.5) {
            // top layer
            if (height > 40) {
                place_block = BLOCK_SNOW; goto SET_BLOCK;
            } else if (height > -25) {
                place_block = BLOCK_GRASS; goto SET_BLOCK;
            } else {
                place_block = BLOCK_SAND; goto SET_BLOCK;
            }
        } else {
            place_block = BLOCK_AIR; goto SET_BLOCK;
        }
    
        SET_BLOCK:
        blocks[idx] = place_block;

        if (block_defs[place_block].opaque == false) {
            continue;
        }


        vec3l world_coorindates = world_block_chunk_to_posl(block_pos, c.key);
        world_update_surface_y(cm, spread(world_coorindates));
    }
        
    return c;
}

void chunk_fix_lighting(chunk_manager *cm, int x, int y, int z) {
    
    vec3i chunk_pos = {x,y,z};

    int chunk_idx = hmgeti(cm->chunk_hm, chunk_pos);
    
    if (chunk_idx == -1) {
        printf("trying to fix lighting of nonexistent chunk?\n");
        return;
    } 

    for (int bx = 0; bx < CHUNK_RADIX; bx++) {
        int32_t global_block_x = bx + x * CHUNK_RADIX;
        for (int bz = 0; bz < CHUNK_RADIX; bz++) {
            int32_t global_block_z = bz + z * CHUNK_RADIX;

            int32_t surface_y = world_get_surface_y(cm, global_block_x, global_block_z).value;

            for (int by = 0; by < CHUNK_RADIX; by++) {
                int32_t global_block_y = by + y * CHUNK_RADIX;

                vec3i block_pos = (vec3i){bx,by,bz};
                //print_vec3i(block_pos);
                int idx = chunk_3d_to_1d(block_pos);
                //printf("fix lighting index %d\n", idx);


                //printf("%d\n", chunk_idx);
                chunk *c = &cm->chunk_hm[chunk_idx];
                block_tag block = c->blocks[idx];
                //printf("%u\n", (uint16_t)block);

                uint8_t lum = block_defs[block].luminance;
                if (lum > 0) {
                    vec3l world_block_pos = world_block_chunk_to_posl(block_pos, (vec3i){x,y,z});
                    cm_add_light(cm, lum, global_block_x, global_block_y, global_block_z);
                }

                if (global_block_y > surface_y) {
                    c->sky_light_levels[idx] = SKY_LIGHT_FULL;
                }
            }

            // so we run the sunlight propagation algorithm at the highest y point in this chunk,
            // if this chunk is the one with the surface (highest opauqe block)
            if (floor_div(surface_y, CHUNK_RADIX) == y) {
                cm_propagate_sunlight(cm, x, CHUNK_MAX, z);
            }
        }
    }
}

void chunk_test() {
    assert_int_equal("0 corner", 0, chunk_3d_to_1d((vec3i){0,0,0}));
    assert_int_equal("r3 corner", CHUNK_MAX_3 + CHUNK_MAX_2 + CHUNK_MAX, chunk_3d_to_1d((vec3i){CHUNK_MAX,CHUNK_MAX,CHUNK_MAX}));
    assert_int_equal("r1 corner", CHUNK_MAX, chunk_3d_to_1d((vec3i){0, 0,CHUNK_MAX}));
    assert_int_equal("r11 corner", CHUNK_MAX + 1, chunk_3d_to_1d((vec3i){0, 1, 0}));
    assert_int_equal("r2 corner", CHUNK_MAX_2, chunk_3d_to_1d((vec3i){0,CHUNK_MAX,0}));
    assert_int_equal("r2+1 corner", CHUNK_MAX*CHUNK_RADIX + CHUNK_MAX, chunk_3d_to_1d((vec3i){0,CHUNK_MAX,CHUNK_MAX}));
    assert_int_equal("r2+1+1 corner", CHUNK_RADIX_2 + CHUNK_MAX_2 + CHUNK_MAX, chunk_3d_to_1d((vec3i){1,CHUNK_MAX,CHUNK_MAX}));

    vec3i pos3d = chunk_1d_to_3d(0);
    assert_int_equal("0x", 0, pos3d.x);
    assert_int_equal("0y", 0, pos3d.y);
    assert_int_equal("0z", 0, pos3d.z);

    pos3d = chunk_1d_to_3d(CHUNK_MAX);
    assert_int_equal("rx", 0, pos3d.x);
    assert_int_equal("ry", 0, pos3d.y);
    assert_int_equal("rz", CHUNK_MAX, pos3d.z);

    pos3d = chunk_1d_to_3d(CHUNK_RADIX * CHUNK_MAX);
    assert_int_equal("2rx", 0, pos3d.x);
    assert_int_equal("2ry", CHUNK_MAX, pos3d.y);
    assert_int_equal("2rz", 0, pos3d.z);

    pos3d = chunk_1d_to_3d(CHUNK_RADIX_2 * CHUNK_MAX);
    assert_int_equal("3rx", CHUNK_MAX, pos3d.x);
    assert_int_equal("3ry", 0, pos3d.y);
    assert_int_equal("3rz", 0, pos3d.z);

    vec3i pos = (vec3i) {5,5,5};
    printf("idx %d (%d %d %d)\n", CHUNK_RADIX_2 + CHUNK_RADIX + 4, pos3d.x, pos3d.y, pos3d.z);
    assert_int_equal("neighbours middle nx", 1, neighbour_exists(pos, PLUS_X));
    assert_int_equal("neighbours middle ny", 1, neighbour_exists(pos, MINUS_Y));
    assert_int_equal("neighbours middle nz", 1, neighbour_exists(pos, MINUS_Z));
    assert_int_equal("neighbours middle px", 1, neighbour_exists(pos, PLUS_X));
    assert_int_equal("neighbours middle py", 1, neighbour_exists(pos, PLUS_Y));
    assert_int_equal("neighbours middle pz", 1, neighbour_exists(pos, PLUS_Z));

    assert_int_equal("neighbours edge nx", 0, neighbour_exists((vec3i) {0, 3, 3}, MINUS_X));
    assert_int_equal("neighbours edge enx", 1, neighbour_exists((vec3i) {0, 3, 3}, PLUS_X));
    assert_int_equal("neighbours edge px", 0, neighbour_exists((vec3i) {CHUNK_MAX, 3, 3}, PLUS_X));    
    assert_int_equal("neighbours edge epx", 1, neighbour_exists((vec3i) {CHUNK_MAX, 3, 3}, MINUS_X));    
    assert_int_equal("neighbours edge ny", 0, neighbour_exists((vec3i) {3, 0, 3}, MINUS_Y));
    assert_int_equal("neighbours edge eny", 1, neighbour_exists((vec3i) {3, 0, 3}, PLUS_Y));
    assert_int_equal("neighbours edge py", 0, neighbour_exists((vec3i) {3, CHUNK_MAX, 3}, PLUS_Y));
    assert_int_equal("neighbours edge epy", 1, neighbour_exists((vec3i) {3, CHUNK_MAX, 3}, MINUS_Y));
    assert_int_equal("neighbours edge nz", 0, neighbour_exists((vec3i) {3, 3, 0}, MINUS_Z));
    assert_int_equal("neighbours edge enz", 1, neighbour_exists((vec3i) {3, 3, 0}, PLUS_Z));
    assert_int_equal("neighbours edge pz", 0, neighbour_exists((vec3i) {3, 3, CHUNK_MAX}, PLUS_Z));
    assert_int_equal("neighbours edge epz", 1, neighbour_exists((vec3i) {3, 3, CHUNK_MAX}, MINUS_Z));
}
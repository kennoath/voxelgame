#include "chunk_common.h"

void generate_tree(chunk_manager *cm, int gx, int gy, int gz);

void chunk_print(chunk c) {
    printf("x: %d y: %d z: %d\n blocks ptr: %p\n", spread(c.key), c.blocks);
}

vec3i chunk_1d_to_3d(int idx) {
    vec3i ret;
    ret.x = idx / CHUNK_RADIX_2;
    ret.y = (idx / CHUNK_RADIX) % CHUNK_RADIX;
    ret.z = idx % CHUNK_RADIX;
    return ret;
}

int chunk_3d_to_1d(int x, int y, int z) {
    return x * CHUNK_RADIX_2 + y * CHUNK_RADIX + z;
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

chunk generate_flat(chunk_manager *cm, int chunk_x, int chunk_y, int chunk_z) {
    block_tag *blocks = calloc(sizeof(block_tag), CHUNK_RADIX_3);
    uint8_t *block_light = calloc(sizeof(uint8_t), CHUNK_RADIX_3);
    uint8_t *sky_light = calloc(sizeof(uint8_t), CHUNK_RADIX_3);
    chunk c = {0};
    c.blocks = blocks;
    c.sky_light_levels = sky_light;
    c.block_light_levels = block_light;

    c.key = (vec3i) {chunk_x, chunk_y, chunk_z};

    int chunk_global_x = chunk_x * CHUNK_RADIX;
    int chunk_global_y = chunk_y * CHUNK_RADIX;
    int chunk_global_z = chunk_z * CHUNK_RADIX;

    for (int idx = 0; idx < CHUNK_RADIX_3; idx++) {
        vec3i block_pos_in_chunk = chunk_1d_to_3d(idx);
        vec3l block_pos_global = {
            chunk_global_x + block_pos_in_chunk.x,
            chunk_global_y + block_pos_in_chunk.y,
            chunk_global_z + block_pos_in_chunk.z,
        };
        
        if (block_pos_global.y > 0) {
            blocks[idx] = BLOCK_AIR;
            sky_light[idx] = SKY_LIGHT_FULL;
            block_light[idx] = 0;
        } else if (block_pos_global.y == 0) {
            blocks[idx] = BLOCK_GRASS;
            sky_light[idx] = SKY_LIGHT_FULL;
            block_light[idx] = 0;
        } else if (block_pos_global.y > -4) {
            blocks[idx] = BLOCK_DIRT;
            sky_light[idx] = SKY_LIGHT_FULL;
            block_light[idx] = 0;
        }  else {
            blocks[idx] = BLOCK_STONE;
            sky_light[idx] = SKY_LIGHT_FULL;
            block_light[idx] = 0;
        } 
        world_update_surface_y(cm, spread(block_pos_global));
    }
    return c;
}

float generate_sample_fractal_noise(struct osn_context *osn, float x, float z, float *A, float *f) {
    float ret = 0;
    for (int i = 0; i < arrlen(A); i++) {
        ret += A[i] * open_simplex_noise2(osn, f[i] * x, f[i] * z);
    }
    return ret;
}

// the point of factoring this out is it can be used for lod chunks as well as normal chunks, global gen
float generate_height(struct osn_context *osn, float x, float z, noise2d_params p) {
    float smooth = generate_sample_fractal_noise(osn, x, z, p.smooth_amplitude, p.smooth_frequency) + 1;
    float height = generate_sample_fractal_noise(osn, x, z, p.lf_height_amplitude, p.lf_height_frequency);
    float hf_height = generate_sample_fractal_noise(osn, x, z, p.hf_height_amplitude, p.hf_height_frequency);

    return height + smooth*hf_height + 100;
}

void chunk_decorate(chunk_manager *cm, int chunk_x, int chunk_y, int chunk_z) {
    vec3i chunk_coords = {
        chunk_x,
        chunk_y,
        chunk_z,
    };

    chunk_x = chunk_x * CHUNK_RADIX;
    chunk_y = chunk_y * CHUNK_RADIX;
    chunk_z = chunk_z * CHUNK_RADIX;

    // peace out if this isnt the chunk at surface level
    maybe_int32_t central_height = world_get_surface_y(cm, chunk_x + CHUNK_RADIX/2, chunk_z + CHUNK_RADIX/2);
    if (!central_height.ok) {
        return;
    }
    if (central_height.value < chunk_y || central_height.value > chunk_y + CHUNK_RADIX) {
        return;
    }

    // treeness just gonna sample once per chunk
    float treeness = generate_sample_fractal_noise(cm->osn, chunk_x + CHUNK_RADIX/2, chunk_z + CHUNK_RADIX/2, cm->noise_params.treeness_amplitude, cm->noise_params.treeness_frequency);
    if (treeness < 0) return;
    util_srand(chunk_coords.x + chunk_coords.y * 31 + chunk_coords.z * 217);
    int num_trees = treeness;
    for (int i = 0; i < num_trees; i++) {
        int tree_x = util_rand_intn(chunk_x, chunk_x + CHUNK_RADIX);
        int tree_z = util_rand_intn(chunk_z, chunk_z + CHUNK_RADIX);
        maybe_int32_t maybe_tree_y = world_get_surface_y(cm, tree_x, tree_z);
        if (!maybe_tree_y.ok) {
            continue;
        }
        int tree_y = maybe_tree_y.value;
        maybe_block_tag base = world_get_block(cm, tree_x, tree_y, tree_z);
        if (base.ok && base.value == BLOCK_GRASS) {
            generate_tree(cm, tree_x, tree_y, tree_z);
        }
    }

}

chunk chunk_initialize(int x, int y, int z) {
    block_tag *blocks = calloc(sizeof(block_tag), CHUNK_RADIX_3);
    uint8_t *block_light = calloc(sizeof(uint8_t), CHUNK_RADIX_3);
    uint8_t *sky_light = calloc(sizeof(uint8_t), CHUNK_RADIX_3);
    chunk c = {0}; 
    c.blocks = blocks;
    c.sky_light_levels = sky_light;
    c.block_light_levels = block_light;

    glGenVertexArrays(1, &c.vao);
    glGenBuffers(1, &c.vbo);
    glGenVertexArrays(1, &c.water_vao);
    glGenBuffers(1, &c.water_vbo);

    c.key = (vec3i) {x,y,z};

    return c;
}

void chunk_generate(chunk_manager *cm, int x, int y, int z) {
    int snow_height = cm->noise_params.snow_above_height;
    int waterlevel = cm->noise_params.water_below_height;
    int sandlevel = cm->noise_params.sand_below_height;

    const float fast_path_safety_factor = 32; // if this is too low there will be artifacts

    float chunk_x = x*CHUNK_RADIX;
    float chunk_y = y*CHUNK_RADIX;
    float chunk_z = z*CHUNK_RADIX;  

    vec3i chunk_coords = {x,y,z};
    chunk *c = hmgetp(cm->chunk_hm, chunk_coords);

    // fast path for air chunks
    if (chunk_y > (generate_height(cm->osn, chunk_x, chunk_z, cm->noise_params) + fast_path_safety_factor)) {
        if (chunk_y > waterlevel) {
            // air happens to be 0 already
            // block light happens to be 0 already
            memset(c->sky_light_levels, SKY_LIGHT_FULL, CHUNK_RADIX_3 * sizeof(uint8_t));
            return;
        } else if (chunk_y < waterlevel - 16) {
            memset(c->blocks, BLOCK_WATER, sizeof(block_tag));
            // skylight 0
            // blocklight 0
            return;
        }
        
    }

    for (int bx = 0; bx < CHUNK_RADIX; bx++) {
        float gx = bx + chunk_x;

        for (int bz = 0; bz < CHUNK_RADIX; bz++) {
            float gz = bz + chunk_z;

            float height = generate_height(cm->osn, gx, gz, cm->noise_params);

            for (int by = 0; by < CHUNK_RADIX; by++) {
                float gy = by + chunk_y;

                block_tag place_block;

                // dont waste time generating caves in the sky
                if (gy < height + 1) {

                    float cave_tendency_2d = 0.5;

                    for (int i = 0; i < arrlen(cm->noise_params.cave_tendency_amplitude); i++) {
                        cave_tendency_2d += cm->noise_params.cave_tendency_amplitude[i] * open_simplex_noise2(cm->osn, 
                            cm->noise_params.cave_tendency_frequency[i] * x, cm->noise_params.cave_tendency_frequency[i] * z);
                    }

                    if (cave_tendency_2d > 0.5) {
                        float cave_carve_density = 0;

                        float A_cave = 20;
                        float f_cave = 0.05;

                        cave_carve_density += A_cave * open_simplex_noise3(cm->osn, f_cave*gx, f_cave*gy, f_cave*gz);
                        A_cave /= 2;
                        f_cave *= 2;

                        cave_carve_density += A_cave * open_simplex_noise3(cm->osn, f_cave*gx, f_cave*gy, f_cave*gz);
                        A_cave /= 2;
                        f_cave *= 2;

                        float cave_cutoff = remap(64, -80, -10, 2, by);


                        if (cave_carve_density*cave_tendency_2d < cave_cutoff) {
                            place_block = BLOCK_AIR; goto SET_BLOCK;
                        }
                                

                        if (cave_carve_density*cave_tendency_2d < cave_cutoff + 0.05 && gy < height) {
                            place_block = BLOCK_GEMS; goto SET_BLOCK;
                        }
                    }
                }

                if (gy > height + 0.5) {
                    // above 2d heightmap: either water or air goes here
                    if (gy < waterlevel) {
                        place_block = BLOCK_WATER; goto SET_BLOCK;
                    } else {
                        place_block = BLOCK_AIR; goto SET_BLOCK;
                    }
                } else {
                    // select based on how far below we are, lowest first
                    if (gy < height - 4) {
                        place_block = BLOCK_STONE; goto SET_BLOCK;
                    } else if (gy < height - 0.5) {
                        // not stone layers
                        if (height > sandlevel) {
                            place_block = BLOCK_DIRT; goto SET_BLOCK;
                        } else {
                            place_block = BLOCK_SAND; goto SET_BLOCK;
                        }
                    } else if (gy < height + 0.5) {
                        // top layer
                        if (height > snow_height) {
                            place_block = BLOCK_SNOW; goto SET_BLOCK;
                        } else if (height > sandlevel) {
                            place_block = BLOCK_GRASS; goto SET_BLOCK;
                        } else {
                            place_block = BLOCK_SAND; goto SET_BLOCK;
                        }
                    }
                }

                SET_BLOCK:
                c->blocks[chunk_3d_to_1d(bx,by,bz)] = place_block;

                if (block_defs[place_block].opaque == false) {
                    continue;
                }

                vec3l world_coorindates = world_block_chunk_to_posl(bx,by,bz, spread(chunk_coords));
                world_update_surface_y(cm, spread(world_coorindates));

            }
        }
    }
}

void generate_tree(chunk_manager *cm, int gx, int gy, int gz) {
    // check there is enough space
    const int required_headroom = 4;
    const int min_height = 4;
    const int max_height = 9;

    const block_tag trunk = BLOCK_LOG;
    const block_tag leaves = BLOCK_LEAVES;

    int proposed_height = util_rand_intn(min_height, max_height);
    int headroom = 0;

    for (int i = 1; i <= max_height; i++) {
        vec3l pos = (vec3l) {gx, gy + i, gz};
        maybe_block_tag b = world_get_block(cm, gx, gy + i, gz);
        if (!b.ok) {
            return;
        } 

        if (block_defs[b.value].opaque) {
            break;
        } else {
            headroom++;
        }
    }

    if (headroom < required_headroom) {
        return;
    }

    int height = min(headroom, proposed_height);
    int leaf_start = util_rand_intn(1, 3);

    for (int i = 1; i <= height; i++) {
        world_set_block_without_lighting(cm, gx, gy + i, gz, trunk);
        if (i > leaf_start) {
            world_set_block_without_lighting(cm, gx + 1, gy + i, gz, leaves);
            world_set_block_without_lighting(cm, gx, gy + i, gz + 1, leaves);
            world_set_block_without_lighting(cm, gx - 1, gy + i, gz, leaves);
            world_set_block_without_lighting(cm, gx, gy + i, gz - 1, leaves);

            if (i < height - 1) {
                world_set_block_without_lighting(cm, gx + 1, gy + i, gz + 1, leaves);
                world_set_block_without_lighting(cm, gx + 1, gy + i, gz - 1, leaves);
                world_set_block_without_lighting(cm, gx - 1, gy + i, gz + 1, leaves);
                world_set_block_without_lighting(cm, gx - 1, gy + i, gz - 1, leaves);
            }

            if (i < height - 2) {
                world_set_block_without_lighting(cm, gx + 2, gy + i, gz + 1, leaves);
                world_set_block_without_lighting(cm, gx + 2, gy + i, gz - 1, leaves);
                world_set_block_without_lighting(cm, gx - 2, gy + i, gz + 1, leaves);
                world_set_block_without_lighting(cm, gx - 2, gy + i, gz - 1, leaves);                
                
                world_set_block_without_lighting(cm, gx + 2, gy + i, gz + 0, leaves);
                world_set_block_without_lighting(cm, gx + 2, gy + i, gz - 0, leaves);
                world_set_block_without_lighting(cm, gx - 2, gy + i, gz + 0, leaves);
                world_set_block_without_lighting(cm, gx - 2, gy + i, gz - 0, leaves);
                
                world_set_block_without_lighting(cm, gx + 2, gy + i, gz + 2, leaves);
                world_set_block_without_lighting(cm, gx + 2, gy + i, gz - 2, leaves);
                world_set_block_without_lighting(cm, gx - 2, gy + i, gz + 2, leaves);
                world_set_block_without_lighting(cm, gx - 2, gy + i, gz - 2, leaves);

                world_set_block_without_lighting(cm, gx + 1, gy + i, gz + 2, leaves);
                world_set_block_without_lighting(cm, gx + 1, gy + i, gz - 2, leaves);
                world_set_block_without_lighting(cm, gx - 1, gy + i, gz + 2, leaves);
                world_set_block_without_lighting(cm, gx - 1, gy + i, gz - 2, leaves);                
                
                world_set_block_without_lighting(cm, gx + 0, gy + i, gz + 2, leaves);
                world_set_block_without_lighting(cm, gx + 0, gy + i, gz - 2, leaves);
                world_set_block_without_lighting(cm, gx - 0, gy + i, gz + 2, leaves);
                world_set_block_without_lighting(cm, gx - 0, gy + i, gz - 2, leaves);
            }
        }

        // also do leaves on top
    }
    world_set_block(cm, gx, gy + 1 + height, gz, leaves);
}



void chunk_test() {

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
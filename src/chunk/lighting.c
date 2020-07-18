#include "chunk_common.h"

vec3l propagation_queue_array[CHUNK_RADIX_3];
vec3l_queue propagation_queue = {
    .items = propagation_queue_array,
    .start = 0,
    .end = 0,
    .size = CHUNK_RADIX_3,
};

vec3l deletion_queue_array[CHUNK_RADIX_3];
vec3l_queue deletion_queue = {
    .items = deletion_queue_array,
    .start = 0,
    .end = 0,
    .size = CHUNK_RADIX_3,
};

vec3l sunlight_queue_array[CHUNK_RADIX_3];
vec3l_queue sunlight_queue = {
    .items = sunlight_queue_array,
    .start = 0,
    .end = 0,
    .size = CHUNK_RADIX_3,
};

vec3l sunlight_deletion_queue_array[CHUNK_RADIX_3];
vec3l_queue sunlight_deletion_queue = {
    .items = sunlight_deletion_queue_array,
    .start = 0,
    .end = 0,
    .size = CHUNK_RADIX_3,
};

void finish_propagating(chunk_manager *cm) {
    while (vec3l_queue_len(&propagation_queue) > 0) {
        vec3l current_pos = vec3l_queue_pop(&propagation_queue);
        uint8_t current_illumination = world_get_illumination(cm, current_pos).value;
        if (current_illumination == 0) {
            continue;
        }

        for (direction dir = 0; dir < NUM_DIRS; dir++) {
            vec3l neighbour_pos = vec3l_add(current_pos, unit_vec3l[dir]);
            block_tag neighbour_block = world_get_block(cm, neighbour_pos).value;
            block_definition neighbour_props = block_defs[neighbour_block];

            if (neighbour_props.opaque || neighbour_props.luminance == 255) {
                continue;
            }

            uint8_t neighbour_illumination = world_get_illumination(cm, neighbour_pos).value;
            if (neighbour_illumination < current_illumination - 1) {
                world_set_illumination(cm, neighbour_pos, current_illumination - 1);
                vec3l_queue_push(&propagation_queue, neighbour_pos);
            }
        }
    }
}

void finish_propagating_sunlight(chunk_manager *cm) {
    while (vec3l_queue_len(&sunlight_queue) > 0) {
        vec3l current_pos = vec3l_queue_pop(&sunlight_queue);
        maybe_uint8_t current_sunlight = world_get_sunlight(cm, current_pos);
        if (current_sunlight.ok == false ||
            current_sunlight.value == 0) {
            continue;
        }

        for (direction dir = 0; dir < NUM_DIRS; dir++) {
            vec3l neighbour_pos = vec3l_add(current_pos, unit_vec3l[dir]);
            block_tag neighbour_block = world_get_block(cm, neighbour_pos).value;
            block_definition neighbour_props = block_defs[neighbour_block];
            maybe_uint8_t neighbour_sunlight = world_get_sunlight(cm, neighbour_pos);

            if (neighbour_sunlight.ok == false ||
                neighbour_props.opaque) {
                continue;
            }

            if (neighbour_sunlight.value < current_sunlight.value - 1) {
                // we subtract 0 if dir == -MY and current_sunlight == max
                world_set_sunlight(cm, neighbour_pos, current_sunlight.value - !(dir == DIR_MY && current_sunlight.value == SKY_LIGHT_FULL));
                vec3l_queue_push(&sunlight_queue, neighbour_pos);
            }
        }
    }
}

void finish_deleting(chunk_manager *cm) {
    while(vec3l_queue_len(&deletion_queue) > 0) {
        vec3l current_pos = vec3l_queue_pop(&deletion_queue);
        uint8_t current_illumination = world_get_illumination(cm, current_pos).value;

        for (direction dir = 0; dir < NUM_DIRS; dir++) {
            vec3l neighbour_pos = vec3l_add(current_pos, unit_vec3l[dir]);
            block_tag neighbour_block = world_get_block(cm, neighbour_pos).value;
            uint8_t neighbour_illumination = world_get_illumination(cm, neighbour_pos).value;

            if (block_defs[neighbour_block].opaque || neighbour_illumination == 0 || neighbour_illumination == 255) {
                continue;
            }

            if (neighbour_illumination >= current_illumination) {
                // must be illuminated in some other way
                vec3l_queue_push(&propagation_queue, neighbour_pos);
            } else {
                vec3l_queue_push(&deletion_queue, neighbour_pos);
            }
        }

        world_set_illumination(cm, current_pos, 0);
    }
}
void finish_deleting_sunlight(chunk_manager *cm) {
    while(vec3l_queue_len(&sunlight_deletion_queue) > 0) {
        vec3l current_pos = vec3l_queue_pop(&sunlight_deletion_queue);
        uint8_t current_sunlight = world_get_sunlight(cm, current_pos).value;

        for (direction dir = 0; dir < NUM_DIRS; dir++) {
            vec3l neighbour_pos = vec3l_add(current_pos, unit_vec3l[dir]);
            block_tag neighbour_block = world_get_block(cm, neighbour_pos).value;
            maybe_uint8_t neighbour_sunlight = world_get_sunlight(cm, neighbour_pos);

            if (block_defs[neighbour_block].opaque || 
                neighbour_sunlight.ok == false || 
                neighbour_sunlight.value == 0) {
                continue;
            }

            // this block will be darker, so we have to delete it and let propagation re run
            if ((current_sunlight == SKY_LIGHT_FULL && dir == DIR_MY) || current_sunlight < neighbour_sunlight.value) {
                vec3l_queue_push(&sunlight_deletion_queue, neighbour_pos);
            } else {
                // block is illuminated some other way
                vec3l_queue_push(&sunlight_queue, neighbour_pos);
            }
        }

        world_set_sunlight(cm, current_pos, 0);
    }
}

void cm_add_light(chunk_manager *cm, uint8_t luminance, long x, long y, long z) {
    vec3l new_light_pos = {x,y,z};
    world_set_illumination(cm, new_light_pos, luminance);
    vec3l_queue_push(&propagation_queue, new_light_pos);

    finish_propagating(cm);
}

void cm_delete_light(chunk_manager *cm, long x, long y, long z) {
    vec3l current_pos = {x,y,z};

    vec3l_queue_push(&deletion_queue, current_pos);
    finish_deleting(cm);
    finish_propagating(cm);
}


void cm_update_light_for_block_placement(chunk_manager *cm, long x, long y, long z) {
    vec3l placed_pos = {x,y,z};
    vec3l_queue_push(&deletion_queue, placed_pos);
    vec3l_queue_push(&sunlight_deletion_queue, placed_pos);

    finish_deleting(cm);
    finish_propagating(cm);

    finish_deleting_sunlight(cm);
    finish_propagating_sunlight(cm);
}

void cm_propagate_sunlight(chunk_manager *cm, int32_t x, int32_t y, int32_t z) {
    vec3l pos = {x,y,z};
    vec3l_queue_push(&sunlight_queue, pos);
    finish_propagating_sunlight(cm);
}

void cm_update_light_for_block_deletion(chunk_manager *cm, long x, long y, long z) {

    vec3l deleted_block_pos = {x,y,z};
    for (direction dir = 0; dir < NUM_DIRS; dir++) {
        vec3l neighbour_pos = vec3l_add(deleted_block_pos, unit_vec3l[dir]);
        vec3l_queue_push(&propagation_queue, neighbour_pos);
        vec3l_queue_push(&sunlight_queue, neighbour_pos);
    }
    finish_propagating(cm);
    finish_propagating_sunlight(cm);
}

void cm_lighting_touch_block(chunk_manager *cm, vec3l pos) {
    vec3i_pair coords = world_posl_to_block_chunk(pos);
    if (coords.l.x == 0) {
        vec3i update_coords = vec3i_add(coords.r, unit_vec3i[DIR_MX]);
        chunk *c = hmgetp(cm->chunk_hm, update_coords);
        c->needs_remesh = true;
    } else if (coords.l.x == 15) {
        vec3i update_coords = vec3i_add(coords.r, unit_vec3i[DIR_PX]);
        chunk *c = hmgetp(cm->chunk_hm, update_coords);
        c->needs_remesh = true;
    }
    if (coords.l.y == 0) {
        vec3i update_coords = vec3i_add(coords.r, unit_vec3i[DIR_MY]);
        chunk *c = hmgetp(cm->chunk_hm, update_coords);
        c->needs_remesh = true;
    } else if (coords.l.y == 15) {
        vec3i update_coords = vec3i_add(coords.r, unit_vec3i[DIR_PY]);
        chunk *c = hmgetp(cm->chunk_hm, update_coords);
        c->needs_remesh = true;
    }
    if (coords.l.z == 0) {
        vec3i update_coords = vec3i_add(coords.r, unit_vec3i[DIR_MZ]);
        chunk *c = hmgetp(cm->chunk_hm, update_coords);
        c->needs_remesh = true;
    } else if (coords.l.z == 15) {
        vec3i update_coords = vec3i_add(coords.r, unit_vec3i[DIR_PZ]);
        chunk *c = hmgetp(cm->chunk_hm, update_coords);
        c->needs_remesh = true;
    }
    chunk *c = hmgetp(cm->chunk_hm, coords.r);
    c->needs_remesh = true;
}
#include "graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include "glad.h"
#include <GLFW/glfw3.h>
#include <cglm/struct.h>
#include "shader.h"
#include "texture.h"
#include "util.h"

graphics_context gc = {0};

graphics_context *graphics_init(int *w, int *h, camera *cam) {
    gc.w = w;
    gc.h = h;
    gc.cam = cam;

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("failed to initialize GLAD\n");
        exit(1);
    }

    
    glViewport(0,0,*w,*h);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);
    glLineWidth(3);

    gc.mesh_program = make_shader_program("shaders/vertex.glsl", "shaders/fragment.glsl");
    gc.chunk_program = make_shader_program("shaders/chunk.vert", "shaders/chunk.frag");
    gc.lodmesh_program = make_shader_program("shaders/lodmesh.vert", "shaders/lodmesh.frag");
    gc.pgm_2d = make_shader_program("shaders/2d.vert", "shaders/2d.frag");


    // make cube
   float vertices[] =
   #include "cube.h"

    unsigned int vao;
    glGenVertexArrays(1, &vao);

    unsigned int vbo;
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // texture coords
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // normals
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);


    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    gc.cube.vertex_data = vertices;
    gc.cube.vao = vao;
    gc.cube.texture = load_texture("assets/tromp.jpg");
    gc.cube.num_triangles = 12;


    // load textures
    gc.tromp = load_texture("assets/tromp.jpg");
    gc.spoderman = load_texture("assets/spoderman.jpg");
    gc.atlas = load_texture("assets/atlas.png");
    gc.reticle = load_texture_rgba("assets/reticle.png");

    // vbo and vao for 2d stuff
    mat4s projection = glms_ortho(0, *gc.w, 0, *gc.h, 0,100);
    // upload the uniform as well
    // probably this should get updated when viewport size changes @todo
    glUseProgram(gc.pgm_2d);
    glUniformMatrix4fv(glGetUniformLocation(gc.pgm_2d, "projection"), 1, GL_FALSE, projection.raw[0]);

    glGenVertexArrays(1, &gc.vao_2d);
    glGenBuffers(1, &gc.vbo_2d);
    glBindVertexArray(gc.vao_2d);
    glBindBuffer(GL_ARRAY_BUFFER, gc.vbo_2d);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return &gc;
}

void draw_mesh(graphics_context *gc, mesh m, vec3s translate, vec3s rotate_axis, float rotate_amt) {
    glUseProgram(gc->mesh_program);

    mat4s model = GLMS_MAT4_IDENTITY_INIT;
    model = glms_translate(model, translate);
    model = glms_rotate(model, rotate_amt, rotate_axis);
    m.transform = model;

    // upload mesh transform
    glUniformMatrix4fv(glGetUniformLocation(gc->mesh_program, "model"), 1, GL_FALSE, m.transform.raw[0]);
    glBindTexture(GL_TEXTURE_2D, m.texture);
    glBindVertexArray(m.vao);
    glDrawArrays(GL_TRIANGLES, 0, m.num_triangles * 3);
}

#define GL_GPU_MEM_INFO_TOTAL_AVAILABLE_MEM_NVX 0x9048
#define GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX 0x9049

// nvidia smi better breakdown
// this is total vram usage not just ur app
uint32_t get_vram_usage() {
    GLint total_mem_kb = 0;
    glGetIntegerv(GL_GPU_MEM_INFO_TOTAL_AVAILABLE_MEM_NVX, 
                &total_mem_kb);

    GLint cur_avail_mem_kb = 0;
    glGetIntegerv(GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX, 
                &cur_avail_mem_kb);

    return total_mem_kb - cur_avail_mem_kb;                
}

// pixels or screen %? this can be pixels
void draw_2d_image(graphics_context *gc, unsigned int texture, int x, int y, int w, int h) {
    float vertices[6][4] = {
        {x, y + h, 0, 0},
        {x, y, 0, 1},
        {x + w, y, 1, 1},

        {x, y + h, 0, 0},
        {x + w, y, 1, 1},
        {x + w, y + h, 1, 0}
    };
    glUseProgram(gc->pgm_2d);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(gc->vao_2d);
    glBindTexture(GL_TEXTURE_2D, texture);

    glBindBuffer(GL_ARRAY_BUFFER, gc->vbo_2d);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);
}

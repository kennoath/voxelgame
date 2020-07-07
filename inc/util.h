#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <cglm/struct.h>

typedef struct {
    long int x;
    long int y;
    long int z;
} vec3l;

typedef struct {
    int x;
    int y;
    int z;
} vec3i;

typedef enum {
    DIR_PX,
    DIR_MX,
    DIR_PY,
    DIR_MY,
    DIR_PZ,
    DIR_MZ,
    NUM_DIRS,
} direction;

float min(float a, float b);
float max(float a, float b);

int mod(int val, int modulus);
int signum(float x);

void assert_int_equal(char *desc, int a, int b);
void assert_float_equal(char *desc, float a, float b);
void assert_vec3i_equal(char *desc, vec3i a, int bx, int by, int bz);
void assert_bool(char *desc, bool a, bool b);

bool fequals(float a, float b);

unsigned long int get_ram_usage();

void test_util();

vec3i from_vec3s(vec3s a);
vec3i vec3i_add(vec3i a, vec3i b);
vec3i vec3i_sub(vec3i a, vec3i b);
vec3i vec3i_mul(vec3i a, int b);
vec3i vec3i_div(vec3i a, int b);
void print_vec3i(vec3i a);

// might be a terrible idea
#define spread(X) X.x, X.y, X.z

extern bool enable_debug;

#define debugf(...) if(enable_debug) {printf(__VA_ARGS__);}

#endif
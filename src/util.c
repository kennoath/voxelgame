#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <stdlib.h>
#include <cglm/struct.h>

#include "util.h"

int signum(float x) {
    if (x > 0) {
        return 1;
    } else {
        return -1;
    }
}

float min(float a, float b) { return a < b? a : b; }
float max(float a, float b) { return a > b? a : b; }

void print_vec3i(vec3i a) {
    printf("{%d %d %d}", spread(a));
}

vec3i from_vec3s(vec3s a) {
    return (vec3i) {
        (int)a.x,
        (int)a.y,
        (int)a.z,
    };
}

#define vec3i_binary_op(OP) .x = a.x OP b.x, .y = a.y OP b.y, .z = a.z OP b.z,

vec3i vec3i_add(vec3i a, vec3i b) { return (vec3i) {vec3i_binary_op(+)}; }
vec3i vec3i_sub(vec3i a, vec3i b) { return (vec3i) {vec3i_binary_op(-)}; }

#define vec3i_num_binary_op(OP) .x = a.x OP b, .y = a.y OP b, .z = a.z OP b,

vec3i vec3i_mul(vec3i a, int b) { return (vec3i) {vec3i_num_binary_op(*)}; }
vec3i vec3i_div(vec3i a, int b) { return (vec3i) {vec3i_num_binary_op(/)}; }

int mod(int val, int modulus) {
    // ok i think this is because its fake modulus (divisor)
    return (val % modulus + modulus) % modulus;
}

const float epsilion = 0.000001;
bool fequals(float a, float b) {
    if (a == b) return true;

    if (fabs(a - b) < epsilion) {
        return true;
    }
    return false;
}

// todo make it say true false
void assert_bool(char *desc, bool a, bool b) {
    if (a == b) {
        printf("\033[032m");
    } else {
        printf("\033[031m");
    }
    printf("%s \t", desc);
    if (a == b) {
        printf("%d == %d \t -- \t pass", a, b);
    } else {
        printf("%d != %d \t -- \t fail", a, b);
    }
    printf("\n\033[037m");
}

void assert_int_equal(char *desc, int a, int b) {
    if (a == b) {
        printf("\033[032m");
    } else {
        printf("\033[031m");
    }
    printf("%s \t", desc);
    if (a == b) {
        printf("%d == %d \t -- \t pass", a, b);
    } else {
        printf("%d != %d \t -- \t fail", a, b);
    }
    printf("\n\033[037m");
}

void assert_vec3i_equal(char *desc, vec3i a, int bx, int by, int bz) {
    if (a.x == bx && a.y == by && a.z == bz) {
        printf("\033[032m");
    } else {
        printf("\033[031m");
    }
    printf("%s \t", desc);
    if (a.x == bx && a.y == by && a.z == bz) {
        printf("{%d %d %d} == {%d %d %d}\t -- \t pass", a.x, a.y, a.z, bx, by, bz);
    } else {
        printf("{%d %d %d} != {%d %d %d}\t -- \t pass", a.x, a.y, a.z, bx, by, bz);        
    }
    printf("\n\033[037m");
}

void assert_float_equal(char *desc, float a, float b) {
    if (fequals(a,b)) {
        printf("\033[032m");
    } else {
        printf("\033[031m");
    }
    printf("%s \t", desc);
    if (fequals(a,b)) {
        printf("%f == %f \t -- \t pass", a, b);
    } else {
        printf("%f != %f \t -- \t fail", a, b);
    }
    printf("\n\033[037m");
}


typedef struct {
    unsigned long size,resident,share,text,lib,data,dt;
} statm_t;

#define PAGESIZE 4096

void read_off_memory_status(statm_t *result)
{
  unsigned long dummy;
  const char* statm_path = "/proc/self/statm";

  FILE *f = fopen(statm_path,"r");
  if(!f){
    perror(statm_path);
    return;
  }
  if(7 != fscanf(f,"%ld %ld %ld %ld %ld %ld %ld",
    &result->size, &result->resident,&result->share,&result->text,&result->lib,&result->data,&result->dt))
  {
    perror(statm_path);
    return;
  }
  fclose(f);
}

unsigned long int get_ram_usage() {
    statm_t res = {0};
    read_off_memory_status(&res);
    return PAGESIZE * res.size;
}

void test_util() {
    // fequals
    assert_bool("feq 0.4 0.4", fequals(0.4, 0.4), true);
    assert_bool("feq 0.5 0.5", fequals(0.5, 0.5), true);
    assert_bool("feq 0.8 0.8", fequals(0.8, 0.8), true);
    assert_bool("feq 0.8 big", fequals(0.8, 123098.34234), false);
    assert_bool("feq 0.81 0.8", fequals(0.81, 0.8), false);    

    // vec3i
    assert_vec3i_equal("123 + 456 = 579", vec3i_add((vec3i){1,2,3}, (vec3i){4,5,6}), 5,7,9);
    assert_vec3i_equal("123 - 456 = -3-3-3", vec3i_sub((vec3i){1,2,3}, (vec3i){4,5,6}), -3,-3,-3);
    assert_vec3i_equal("123 * 2 = 246", vec3i_mul((vec3i){1,2,3}, 2), 2,4,6);
    assert_vec3i_equal("123 / 2 = 011", vec3i_div((vec3i){1,2,3}, 2), 0,1,1);
}
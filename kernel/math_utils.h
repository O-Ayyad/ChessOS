#pragma once
#include "types.h"

const float PI = 3.14159265f;

static inline int   min_int(int a, int b)   { return a < b ? a : b; }
static inline int   max_int(int a, int b)   { return a > b ? a : b; }
static inline float min_float(float a, float b) { return a < b ? a : b; }
static inline float max_float(float a, float b) { return a > b ? a : b; }
static inline int   abs_int(int x)          { return x < 0 ? -x : x; }
static inline float abs_float(float x)      { return x < 0 ? -x : x; }

static inline int clamp_int(int x, int low, int high) {
    if (x < low) return low;
    if (x > high) return high;
    return x;
}
static inline float clamp_float(float x, float low, float high) {
    if (x < low) return low;
    if (x > high) return high;
    return x;
}

static inline int wrap_around(int value, int count) {
    return ((value % count) + count) % count;
}

float square_root(float x);
float round_down(float x);
float sine(float angle);
float cosine(float angle);
float arc_tangent2(float y, float x);
float degrees_to_radians(float degrees);
float radians_to_degrees(float radians);
float exponential(float x);

// A point or direction in 3D space.
struct Vector3 {
    float x, y, z;
};
Vector3 make_vector(float x, float y, float z);
Vector3 add_vectors(Vector3 a, Vector3 b);
Vector3 subtract_vectors(Vector3 a, Vector3 b);
Vector3 scale_vector(Vector3 v, float factor);
float   dot_product(Vector3 a, Vector3 b);
Vector3 cross_product(Vector3 a, Vector3 b);
Vector3 normalize_vector(Vector3 v);

u32 random_number();
int random_below(int limit);
void seed_random(u64 seed);

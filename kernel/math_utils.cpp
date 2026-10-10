#include "math_utils.h"

float square_root(float x) {
    if (x <= 0) {
        return 0;
    }
    return __builtin_sqrtf(x);
}

float round_down(float x) {
    int whole = (int)x;
    if (x < whole) {
        whole = whole - 1;
    }
    return (float)whole;
}

float exponential(float x) {
    if (x < -80) return 0;
    if (x > 80) x = 80;
    float power_of_two = x * 1.44269504f; // log2(e)
    int whole = (int)round_down(power_of_two);
    float fraction = power_of_two - whole;
    float result = 1 + fraction * (0.6931472f + fraction * (0.2402265f + fraction * (0.0555041f + fraction * 0.0096181f)));
    union { float as_float; u32 as_bits; } number;
    number.as_float = result;
    number.as_bits += (u32)(whole << 23);
    return number.as_float;
}

float degrees_to_radians(float degrees) {
    return degrees * PI / 180.0f;
}

float radians_to_degrees(float radians) {
    return radians * 180.0f / PI;
}

float sine(float angle) {
    float turns = round_down(angle / (2 * PI) + 0.5f);
    angle = angle - turns * 2 * PI;
    if (angle > PI / 2) {
        angle = PI - angle;
    } else if (angle < -PI / 2) {
        angle = -PI - angle;
    }

    float x2 = angle * angle;
    return angle * (1 - x2 / 6 * (1 - x2 / 20 * (1 - x2 / 42 * (1 - x2 / 72 * (1 - x2 / 110)))));
}

float cosine(float angle) {
    return sine(angle + PI / 2);
}


float arc_tangent2(float y, float x) {
    if (x == 0 && y == 0) {
        return 0;
    }
    float ax = abs_float(x);
    float ay = abs_float(y);

    float ratio = (ax > ay) ? ay / ax : ax / ay;
    float r2 = ratio * ratio;
    float angle = ((-0.0464964749f * r2 + 0.15931422f) * r2 - 0.327622764f) * r2 * ratio + ratio;
    if (ay > ax) angle = PI / 2 - angle;
    if (x < 0)   angle = PI - angle;
    if (y < 0)   angle = -angle;
    return angle;
}

Vector3 make_vector(float x, float y, float z) {
    Vector3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

Vector3 add_vectors(Vector3 a, Vector3 b)      { return make_vector(a.x + b.x, a.y + b.y, a.z + b.z); }
Vector3 subtract_vectors(Vector3 a, Vector3 b) { return make_vector(a.x - b.x, a.y - b.y, a.z - b.z); }
Vector3 scale_vector(Vector3 v, float factor)  { return make_vector(v.x * factor, v.y * factor, v.z * factor); }
float dot_product(Vector3 a, Vector3 b)        { return a.x * b.x + a.y * b.y + a.z * b.z; }

Vector3 cross_product(Vector3 a, Vector3 b) {
    return make_vector(a.y * b.z - a.z * b.y,
                       a.z * b.x - a.x * b.z,
                       a.x * b.y - a.y * b.x);
}

Vector3 normalize_vector(Vector3 v) {
    float length = square_root(dot_product(v, v));
    if (length == 0) {
        return v;
    }
    return scale_vector(v, 1.0f / length);
}


static u64 random_state = 0x2545F4914F6CDD1DULL;

void seed_random(u64 seed) {
    if (seed != 0) {
        random_state = seed;
    }
}

u32 random_number() {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 7;
    random_state ^= random_state << 17;
    return (u32)(random_state >> 16);
}

int random_below(int limit) {
    if (limit <= 0) {
        return 0;
    }
    return random_number() % limit;
}

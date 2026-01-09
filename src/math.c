#include "math.h"
#include <math.h>
#include <string.h>

void mat4_identity(float* m) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void mat4_ortho(float* m, float left, float right, float bottom, float top) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = 2.0f / (right - left);
    m[5] = 2.0f / (top - bottom);
    m[10] = -1.0f;
    m[12] = -(right + left) / (right - left);
    m[13] = -(top + bottom) / (top - bottom);
    m[15] = 1.0f;
}

void mat4_perspective(float* m, float width, float height, float camera_dist, float far) {
    // Perspective projection for screen-space coordinates
    // Camera at (width/2, height/2, camera_dist), looking toward -z
    // At z=0, objects appear at same size as orthographic
    // At z<0 (negative), objects appear smaller (farther from camera)
    //
    // The projection works as follows:
    // - x_clip = (2*d/w)*x - d
    // - y_clip = (2*d/h)*y - d  
    // - z_clip maps depth to [0,1] for depth buffer
    // - w_clip = d - z (perspective divide factor)
    //
    // After divide by w: x_ndc = x_clip/w_clip, etc.
    // At z=0: w=d, so x_ndc = ((2d/w)*x - d)/d = 2x/w - 1 (matches ortho)
    
    memset(m, 0, 16 * sizeof(float));
    
    float d = camera_dist;
    float f = far;
    
    // Column 0 (x coefficients)
    m[0] = 2.0f * d / width;    // x_clip += (2d/w) * x
    
    // Column 1 (y coefficients)
    m[5] = 2.0f * d / height;   // y_clip += (2d/h) * y
    
    // Column 2 (z coefficients)
    m[10] = -(d + f) / f;       // z_clip += -(d+f)/f * z, maps z to [0,1] after divide
    m[11] = -1.0f;              // w_clip += -z
    
    // Column 3 (constant terms)
    m[12] = -d;                 // x_clip += -d (center x)
    m[13] = -d;                 // y_clip += -d (center y)
    m[15] = d;                  // w_clip += d
}

void mat4_translate(float* m, float x, float y) {
    mat4_identity(m);
    m[12] = x;
    m[13] = y;
}

void mat4_translate_3d(float* m, float x, float y, float z) {
    mat4_identity(m);
    m[12] = x;
    m[13] = y;
    m[14] = z;
}

void mat4_rotate_z(float* m, float angle) {
    mat4_identity(m);
    float c = cosf(angle);
    float s = sinf(angle);
    m[0] = c;
    m[1] = s;
    m[4] = -s;
    m[5] = c;
}

void mat4_scale(float* m, float sx, float sy) {
    mat4_identity(m);
    m[0] = sx;
    m[5] = sy;
}

void mat4_multiply(float* result, const float* a, const float* b) {
    float temp[16];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            temp[i * 4 + j] = 0;
            for (int k = 0; k < 4; k++) {
                temp[i * 4 + j] += a[k * 4 + j] * b[i * 4 + k];
            }
        }
    }
    memcpy(result, temp, 16 * sizeof(float));
}

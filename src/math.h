#ifndef MATH_H
#define MATH_H

// Math constants
#define PI 3.14159265358979323846f

// Matrix helper functions for 4x4 matrices (column-major order)

// Set matrix to identity
void mat4_identity(float* m);

// Create orthographic projection matrix
void mat4_ortho(float* m, float left, float right, float bottom, float top);

// Create perspective projection matrix for screen-space coordinates
// Camera is conceptually at (width/2, height/2, camera_dist), looking toward -z
// Objects at z=0 appear at same size as orthographic projection
// Objects with z<0 appear smaller (farther from camera)
void mat4_perspective(float* m, float width, float height, float camera_dist, float far);

// Create translation matrix (2D, z=0)
void mat4_translate(float* m, float x, float y);

// Create translation matrix (3D)
void mat4_translate_3d(float* m, float x, float y, float z);

// Create rotation matrix around Z axis
void mat4_rotate_z(float* m, float angle);

// Create scale matrix
void mat4_scale(float* m, float sx, float sy);

// Multiply two matrices: result = a * b
void mat4_multiply(float* result, const float* a, const float* b);

#endif // MATH_H

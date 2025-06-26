#pragma once

#include <math.h>
#include <string.h>

static inline void matrix_identity(float *m)
{
	memset(m, 0, 16 * sizeof(float));
	m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static inline void matrix_multiply(float *result, const float *a, const float *b)
{
	float temp[16];
	for (int col = 0; col < 4; col++)
	{
		for (int row = 0; row < 4; row++)
		{
			temp[col * 4 + row] = 0.0f;
			for (int k = 0; k < 4; k++)
			{
				temp[col * 4 + row] += a[k * 4 + row] * b[col * 4 + k];
			}
		}
	}
	memcpy(result, temp, 16 * sizeof(float));
}

static inline void matrix_translate(float *m, float x, float y, float z)
{
	float trans[16];
	matrix_identity(trans);
	trans[12] = x;
	trans[13] = y;
	trans[14] = z;
	matrix_multiply(m, m, trans);
}

static inline void matrix_rotate_x(float *m, float angle)
{
	float rot[16];
	float rad = (float)(angle * M_PI / 180.0);
	float c = cosf(rad);
	float s = sinf(rad);

	matrix_identity(rot);
	rot[5] = c;
	rot[6] = s;
	rot[9] = -s;
	rot[10] = c;
	matrix_multiply(m, m, rot);
}

static inline void matrix_rotate_y(float *m, float angle)
{
	float rot[16];
	float rad = (float)(angle * M_PI / 180.0);
	float c = cosf(rad);
	float s = sinf(rad);

	matrix_identity(rot);
	rot[0] = c;
	rot[2] = -s;
	rot[8] = s;
	rot[10] = c;
	matrix_multiply(m, m, rot);
}

static inline void matrix_rotate_z(float *m, float angle)
{
	float rot[16];
	float rad = (float)(angle * M_PI / 180.0);
	float c = cosf(rad);
	float s = sinf(rad);

	matrix_identity(rot);
	rot[0] = c;
	rot[1] = s;
	rot[4] = -s;
	rot[5] = c;
	matrix_multiply(m, m, rot);
}

static inline void matrix_frustum(float *m, float left, float right, float bottom, float top, float near, float far)
{
	matrix_identity(m);
	m[0] = 2.0f * near / (right - left);
	m[5] = 2.0f * near / (top - bottom);
	m[8] = (right + left) / (right - left);
	m[9] = (top + bottom) / (top - bottom);
	m[10] = -(far + near) / (far - near);
	m[11] = -1.0f;
	m[14] = -2.0f * far * near / (far - near);
	m[15] = 0.0f;
}

/* extract 3x3 matrix and store in std140 layout (3 vec4s) */
static inline void matrix_extract_3x3_std140(float *dest, const float *src)
{
	/* first column */
	dest[0] = src[0];
	dest[1] = src[1];
	dest[2] = src[2];
	dest[3] = 0.0f;
	/* second column */
	dest[4] = src[4];
	dest[5] = src[5];
	dest[6] = src[6];
	dest[7] = 0.0f;
	/* third column */
	dest[8] = src[8];
	dest[9] = src[9];
	dest[10] = src[10];
	dest[11] = 0.0f;
}

/*
 * Copyright (C) 1999-2001  Brian Paul        All Rights Reserved. (For gear shapes)
 * Copyright (C) 2025       William Horvath   All Rights Reserved.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL3/SDL_gpu.h>

#include "sdlgpu_gear_creation.h"
#include "sdlgpu_render.h"

#ifndef PI
#define PI 3.14159265358979323846
#endif

static inline uint32_t add_vertex(Vertex *vertices, uint32_t *count, float x, float y, float z, float nx, float ny, float nz)
{
	uint32_t index = *count;
	vertices[index].position[0] = x;
	vertices[index].position[1] = y;
	vertices[index].position[2] = z;
	vertices[index].normal[0] = nx;
	vertices[index].normal[1] = ny;
	vertices[index].normal[2] = nz;
	(*count)++;
	return index;
}

static inline void add_triangle(uint32_t *indices, uint32_t *count, uint32_t a, uint32_t b, uint32_t c)
{
	indices[(*count)++] = a;
	indices[(*count)++] = b;
	indices[(*count)++] = c;
}

static void create_face(Vertex *vertices, uint32_t *vertex_count, uint32_t *indices, uint32_t *index_count, float inner_radius, float outer_radius, int teeth,
                        float tooth_depth, float z, float normal_z)
{
	float r0 = inner_radius;
	float r1 = outer_radius - tooth_depth / 2.0f;
	float da = (float)(2.0 * PI / teeth / 4.0);

	/* create main ring face like an OpenGL GL_QUAD_STRIP */
	uint32_t *ring_vertices = (uint32_t *)malloc((4 * teeth + 4) * sizeof(uint32_t)); /* store vertex indices for triangulation */
	uint32_t ring_count = 0;

	for (int i = 0; i <= teeth; i++)
	{
		float angle = (float)(i * 2.0 * PI / teeth);

		ring_vertices[ring_count++] = add_vertex(vertices, vertex_count, r0 * cosf(angle), r0 * sinf(angle), z, 0, 0, normal_z);
		ring_vertices[ring_count++] = add_vertex(vertices, vertex_count, r1 * cosf(angle), r1 * sinf(angle), z, 0, 0, normal_z);

		if (i < teeth)
		{
			ring_vertices[ring_count++] = add_vertex(vertices, vertex_count, r0 * cosf(angle), r0 * sinf(angle), z, 0, 0, normal_z);
			ring_vertices[ring_count++] = add_vertex(vertices, vertex_count, r1 * cosf(angle + 3 * da), r1 * sinf(angle + 3 * da), z, 0, 0, normal_z);
		}
	}

	/* triangulate the quad strip with proper winding order */
	for (uint32_t i = 0; i < ring_count - 2; i += 2)
	{
		uint32_t v0 = ring_vertices[i];
		uint32_t v1 = ring_vertices[i + 1];
		uint32_t v2 = ring_vertices[i + 2];
		uint32_t v3 = ring_vertices[i + 3];

		if (normal_z > 0) /* front face */
		{
			add_triangle(indices, index_count, v0, v1, v3);
			add_triangle(indices, index_count, v0, v3, v2);
		}
		else /* back face */
		{
			add_triangle(indices, index_count, v0, v3, v1);
			add_triangle(indices, index_count, v0, v2, v3);
		}
	}

	free(ring_vertices);
}

static void create_tooth_faces(Vertex *vertices, uint32_t *vertex_count, uint32_t *indices, uint32_t *index_count, float outer_radius, int teeth, float tooth_depth,
                               float z, float normal_z)
{
	float r1 = outer_radius - tooth_depth / 2.0f;
	float r2 = outer_radius + tooth_depth / 2.0f;
	float da = (float)(2.0 * PI / teeth / 4.0);

	for (int i = 0; i < teeth; i++)
	{
		float angle = (float)(i * 2.0 * PI / teeth);

		/* single quad per tooth */
		uint32_t t0 = add_vertex(vertices, vertex_count, r1 * cosf(angle), r1 * sinf(angle), z, 0, 0, normal_z);
		uint32_t t1 = add_vertex(vertices, vertex_count, r2 * cosf(angle + da), r2 * sinf(angle + da), z, 0, 0, normal_z);
		uint32_t t2 = add_vertex(vertices, vertex_count, r2 * cosf(angle + 2 * da), r2 * sinf(angle + 2 * da), z, 0, 0, normal_z);
		uint32_t t3 = add_vertex(vertices, vertex_count, r1 * cosf(angle + 3 * da), r1 * sinf(angle + 3 * da), z, 0, 0, normal_z);

		if (normal_z > 0) /* front face */
		{
			add_triangle(indices, index_count, t0, t1, t2);
			add_triangle(indices, index_count, t0, t2, t3);
		}
		else /* back face */
		{
			add_triangle(indices, index_count, t0, t2, t1);
			add_triangle(indices, index_count, t0, t3, t2);
		}
	}
}

bool create_gear(SDL_GPUDevice *device, GearData *gear_data, float inner_radius, float outer_radius, float width, int teeth, float tooth_depth, const float color[3])
{
	float r0 = inner_radius;
	float r1 = outer_radius - tooth_depth / 2.0f;
	float r2 = outer_radius + tooth_depth / 2.0f;
	float da = (float)(2.0 * PI / teeth / 4.0);

	/* estimate max vertices and indices needed */
	int max_vertices = teeth * 100 + 200;
	int max_indices = teeth * 300 + 600;

	Vertex *vertices = (Vertex *)malloc(max_vertices * sizeof(Vertex));
	uint32_t *indices = (uint32_t *)malloc(max_indices * sizeof(uint32_t));

	if (!vertices || !indices)
	{
		free(vertices);
		free(indices);
		return false;
	}

	uint32_t vertex_count = 0;
	uint32_t index_count = 0;

	/* create front face - main ring */
	create_face(vertices, &vertex_count, indices, &index_count, inner_radius, outer_radius, teeth, tooth_depth, width * 0.5f, 1.0f);

	/* create front sides of teeth */
	create_tooth_faces(vertices, &vertex_count, indices, &index_count, outer_radius, teeth, tooth_depth, width * 0.5f, 1.0f);

	/* create back face - main ring */
	create_face(vertices, &vertex_count, indices, &index_count, inner_radius, outer_radius, teeth, tooth_depth, -width * 0.5f, -1.0f);

	/* create back sides of teeth */
	create_tooth_faces(vertices, &vertex_count, indices, &index_count, outer_radius, teeth, tooth_depth, -width * 0.5f, -1.0f);

	/* create outward faces of teeth, like an OpenGL GL_QUAD_STRIP */
	uint32_t *strip_vertices = (uint32_t *)malloc((8 * teeth + 2) * sizeof(uint32_t)); /* vertices for the quad strip */
	uint32_t strip_count = 0;

	for (int i = 0; i < teeth; i++)
	{
		float angle = (float)(i * 2.0 * PI / teeth);

		/* first edge vertices get the radial normal from previous iteration */
		float radial_nx = cosf(angle);
		float radial_ny = sinf(angle);
		strip_vertices[strip_count++] = add_vertex(vertices, &vertex_count, r1 * cosf(angle), r1 * sinf(angle), width * 0.5f, radial_nx, radial_ny, 0);
		strip_vertices[strip_count++] = add_vertex(vertices, &vertex_count, r1 * cosf(angle), r1 * sinf(angle), -width * 0.5f, radial_nx, radial_ny, 0);

		/* calculate normal for first slanted face */
		float u = r2 * cosf(angle + da) - r1 * cosf(angle);
		float v = r2 * sinf(angle + da) - r1 * sinf(angle);
		float len = sqrtf(u * u + v * v);
		float slant1_nx = v / len;
		float slant1_ny = -u / len;

		/* vertices at angle+da get the slanted normal */
		strip_vertices[strip_count++] = add_vertex(vertices, &vertex_count, r2 * cosf(angle + da), r2 * sinf(angle + da), width * 0.5f, slant1_nx, slant1_ny, 0);
		strip_vertices[strip_count++] = add_vertex(vertices, &vertex_count, r2 * cosf(angle + da), r2 * sinf(angle + da), -width * 0.5f, slant1_nx, slant1_ny, 0);

		/* vertices at angle+2*da get the radial normal */
		strip_vertices[strip_count++] =
		    add_vertex(vertices, &vertex_count, r2 * cosf(angle + 2 * da), r2 * sinf(angle + 2 * da), width * 0.5f, radial_nx, radial_ny, 0);
		strip_vertices[strip_count++] =
		    add_vertex(vertices, &vertex_count, r2 * cosf(angle + 2 * da), r2 * sinf(angle + 2 * da), -width * 0.5f, radial_nx, radial_ny, 0);

		/* calculate normal for second slanted face */
		u = r1 * cosf(angle + 3 * da) - r2 * cosf(angle + 2 * da);
		v = r1 * sinf(angle + 3 * da) - r2 * sinf(angle + 2 * da);
		len = sqrtf(u * u + v * v);
		float slant2_nx = v / len;
		float slant2_ny = -u / len;

		/* vertices at angle+3*da get the second slanted normal */
		strip_vertices[strip_count++] =
		    add_vertex(vertices, &vertex_count, r1 * cosf(angle + 3 * da), r1 * sinf(angle + 3 * da), width * 0.5f, slant2_nx, slant2_ny, 0);
		strip_vertices[strip_count++] =
		    add_vertex(vertices, &vertex_count, r1 * cosf(angle + 3 * da), r1 * sinf(angle + 3 * da), -width * 0.5f, slant2_nx, slant2_ny, 0);
	}

	/* close the strip - vertices at angle 0 get radial normal */
	strip_vertices[strip_count++] = add_vertex(vertices, &vertex_count, r1 * cosf(0), r1 * sinf(0), width * 0.5f, 1.0f, 0.0f, 0);
	strip_vertices[strip_count++] = add_vertex(vertices, &vertex_count, r1 * cosf(0), r1 * sinf(0), -width * 0.5f, 1.0f, 0.0f, 0);

	/* triangulate the quad strip */
	for (uint32_t i = 0; i < strip_count - 2; i += 2)
	{
		uint32_t v0 = strip_vertices[i];
		uint32_t v1 = strip_vertices[i + 1];
		uint32_t v2 = strip_vertices[i + 2];
		uint32_t v3 = strip_vertices[i + 3];

		/* create quad as two triangles */
		add_triangle(indices, &index_count, v0, v1, v3);
		add_triangle(indices, &index_count, v0, v3, v2);
	}

	/* create inside radius cylinder */
	for (int i = 0; i < teeth; i++)
	{
		float angle = (float)(i * 2.0 * PI / teeth);
		float next_angle = (float)((i + 1) * 2.0 * PI / teeth);

		/* normals pointing inward (toward axis) */
		uint32_t c0 = add_vertex(vertices, &vertex_count, r0 * cosf(angle), r0 * sinf(angle), -width * 0.5f, -cosf(angle), -sinf(angle), 0);
		uint32_t c1 = add_vertex(vertices, &vertex_count, r0 * cosf(angle), r0 * sinf(angle), width * 0.5f, -cosf(angle), -sinf(angle), 0);
		uint32_t c2 = add_vertex(vertices, &vertex_count, r0 * cosf(next_angle), r0 * sinf(next_angle), width * 0.5f, -cosf(next_angle), -sinf(next_angle), 0);
		uint32_t c3 = add_vertex(vertices, &vertex_count, r0 * cosf(next_angle), r0 * sinf(next_angle), -width * 0.5f, -cosf(next_angle), -sinf(next_angle), 0);

		/* winding order for inside faces (viewed from outside) */
		add_triangle(indices, &index_count, c0, c1, c2);
		add_triangle(indices, &index_count, c0, c2, c3);
	}

#ifdef _DEBUG
	printf("Gear %d: Generated %u vertices, %u indices\n", teeth, vertex_count, index_count);
#endif

	/* create GPU buffers */
	SDL_GPUBufferCreateInfo vertex_buffer_info = {.usage = SDL_GPU_BUFFERUSAGE_VERTEX, .size = (uint32_t)(vertex_count * sizeof(Vertex)), .props = 0};

	SDL_GPUBufferCreateInfo index_buffer_info = {.usage = SDL_GPU_BUFFERUSAGE_INDEX, .size = (uint32_t)(index_count * sizeof(uint32_t)), .props = 0};

	gear_data->vertex_buffer = SDL_CreateGPUBuffer(device, &vertex_buffer_info);
	gear_data->index_buffer = SDL_CreateGPUBuffer(device, &index_buffer_info);
	gear_data->index_count = index_count;
	gear_data->color[0] = color[0];
	gear_data->color[1] = color[1];
	gear_data->color[2] = color[2];

	if (!gear_data->vertex_buffer || !gear_data->index_buffer)
	{
		printf("Failed to create GPU buffers\n");
		free(vertices);
		free(indices);
		free(strip_vertices);
		return false;
	}

	/* upload data */
	SDL_GPUTransferBufferCreateInfo transfer_info = {.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
	                                                 .size = SDL_max((uint32_t)(vertex_count * sizeof(Vertex)), (uint32_t)(index_count * sizeof(uint32_t))),
	                                                 .props = 0};

	SDL_GPUTransferBuffer *transfer_buffer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
	if (!transfer_buffer)
	{
		printf("Failed to create transfer buffer\n");
		free(vertices);
		free(indices);
		free(strip_vertices);
		return false;
	}

	SDL_GPUCommandBuffer *upload_cmd = SDL_AcquireGPUCommandBuffer(device);
	SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(upload_cmd);

	/* upload vertices */
	void *mapped = SDL_MapGPUTransferBuffer(device, transfer_buffer, false);
	memcpy(mapped, vertices, vertex_count * sizeof(Vertex));
	SDL_UnmapGPUTransferBuffer(device, transfer_buffer);

	SDL_GPUTransferBufferLocation src = {transfer_buffer, 0};
	SDL_GPUBufferRegion dst = {gear_data->vertex_buffer, 0, (uint32_t)(vertex_count * sizeof(Vertex))};
	SDL_UploadToGPUBuffer(copy_pass, &src, &dst, false);

	/* upload indices */
	mapped = SDL_MapGPUTransferBuffer(device, transfer_buffer, true);
	memcpy(mapped, indices, index_count * sizeof(uint32_t));
	SDL_UnmapGPUTransferBuffer(device, transfer_buffer);

	src.offset = 0;
	dst.buffer = gear_data->index_buffer;
	dst.offset = 0;
	dst.size = index_count * sizeof(uint32_t);
	SDL_UploadToGPUBuffer(copy_pass, &src, &dst, false);

	SDL_EndGPUCopyPass(copy_pass);
	SDL_SubmitGPUCommandBuffer(upload_cmd);

	SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
	free(vertices);
	free(indices);
	free(strip_vertices);

	return true;
}

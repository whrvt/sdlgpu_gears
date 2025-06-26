#include <SDL3/SDL_time.h>
#include <SDL3/SDL_timer.h>

#include <stdio.h>
#include <stdlib.h>

#include "sdlgpu_math.h"
#include "sdlgpu_render.h"

/* uniform data passed to shaders, in std140 layout */
typedef struct
{
	float mvp_matrix[16];    /* mat4: 64 bytes */
	float model_matrix[16];  /* mat4: 64 bytes */
	float normal_matrix[12]; /* mat3 in std140: 3 vec3s, each padded to vec4 = 48 bytes */
	float light_position[4]; /* vec3 padded to vec4: 16 bytes */
	float light_color[4];    /* vec3 padded to vec4: 16 bytes */
	float object_color[4];   /* vec3 padded to vec4: 16 bytes */
} Uniforms;

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
	float da = (float)(2.0 * M_PI / teeth / 4.0);

	/* create main ring face like an OpenGL GL_QUAD_STRIP */
	uint32_t ring_vertices[4 * teeth + 4]; /* store vertex indices for triangulation */
	uint32_t ring_count = 0;

	for (int i = 0; i <= teeth; i++)
	{
		float angle = (float)(i * 2.0 * M_PI / teeth);

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
}

static void create_tooth_faces(Vertex *vertices, uint32_t *vertex_count, uint32_t *indices, uint32_t *index_count, float outer_radius, int teeth, float tooth_depth,
                               float z, float normal_z)
{
	float r1 = outer_radius - tooth_depth / 2.0f;
	float r2 = outer_radius + tooth_depth / 2.0f;
	float da = (float)(2.0 * M_PI / teeth / 4.0);

	for (int i = 0; i < teeth; i++)
	{
		float angle = (float)(i * 2.0 * M_PI / teeth);

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
	float da = (float)(2.0 * M_PI / teeth / 4.0);

	/* estimate max vertices and indices needed */
	int max_vertices = teeth * 100 + 200;
	int max_indices = teeth * 300 + 600;

	Vertex *vertices = malloc(max_vertices * sizeof(Vertex));
	uint32_t *indices = malloc(max_indices * sizeof(uint32_t));

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
	uint32_t strip_vertices[8 * teeth + 2]; /* vertices for the quad strip */
	uint32_t strip_count = 0;

	for (int i = 0; i < teeth; i++)
	{
		float angle = (float)(i * 2.0 * M_PI / teeth);

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
		float angle = (float)(i * 2.0 * M_PI / teeth);
		float next_angle = (float)((i + 1) * 2.0 * M_PI / teeth);

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
	SDL_GPUBufferCreateInfo vertex_buffer_info = {.usage = SDL_GPU_BUFFERUSAGE_VERTEX, .size = vertex_count * sizeof(Vertex), .props = 0};

	SDL_GPUBufferCreateInfo index_buffer_info = {.usage = SDL_GPU_BUFFERUSAGE_INDEX, .size = index_count * sizeof(uint32_t), .props = 0};

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
		return false;
	}

	/* upload data */
	SDL_GPUTransferBufferCreateInfo transfer_info = {
	    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = SDL_max(vertex_count * sizeof(Vertex), index_count * sizeof(uint32_t)), .props = 0};

	SDL_GPUTransferBuffer *transfer_buffer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
	if (!transfer_buffer)
	{
		printf("Failed to create transfer buffer\n");
		free(vertices);
		free(indices);
		return false;
	}

	SDL_GPUCommandBuffer *upload_cmd = SDL_AcquireGPUCommandBuffer(device);
	SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(upload_cmd);

	/* upload vertices */
	void *mapped = SDL_MapGPUTransferBuffer(device, transfer_buffer, false);
	memcpy(mapped, vertices, vertex_count * sizeof(Vertex));
	SDL_UnmapGPUTransferBuffer(device, transfer_buffer);

	SDL_GPUTransferBufferLocation src = {transfer_buffer, 0};
	SDL_GPUBufferRegion dst = {gear_data->vertex_buffer, 0, vertex_count * sizeof(Vertex)};
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

	return true;
}

static bool create_depth_texture(SDL_GPUDevice *device, uint32_t width, uint32_t height)
{
	if (render_state.depth_texture && render_state.depth_texture_width == width && render_state.depth_texture_height == height)
		return true; /* already have correct size */

	/* release old depth texture */
	if (render_state.depth_texture)
		SDL_ReleaseGPUTexture(device, render_state.depth_texture);

	SDL_GPUTextureCreateInfo depth_info = {.type = SDL_GPU_TEXTURETYPE_2D,
	                                       .format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
	                                       .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
	                                       .width = width,
	                                       .height = height,
	                                       .layer_count_or_depth = 1,
	                                       .num_levels = 1,
	                                       .sample_count = SDL_GPU_SAMPLECOUNT_1,
	                                       .props = 0};

	render_state.depth_texture = SDL_CreateGPUTexture(device, &depth_info);
	if (!render_state.depth_texture)
		return false;

	render_state.depth_texture_width = width;
	render_state.depth_texture_height = height;
	return true;
}

static inline double current_time(void)
{
	static SDL_Time time = 0;
	if (!SDL_GetCurrentTime(&time))
		printf("SDL_GetCurrentTime error: %s\n", SDL_GetError());

	return (double)time / (double)SDL_NS_PER_SECOND;
}

void draw_frame(SDL_Window *window)
{
	static int frames = 0;
	static double tRot0 = -1.0;
	static double tRate0 = -1.0;
	double dt = 0.0f;
	double t = current_time();

	if (tRot0 < 0.0)
		tRot0 = t;
	dt = t - tRot0;
	tRot0 = t;

	if (animate)
	{
		render_state.angle += (float)(70.0 * dt);
		if (render_state.angle > 3600.0f)
			render_state.angle -= 3600.0f;
	}

	if (!render_state.swapchain_valid)
	{
		/* this might be an SDL_gpu bug, but resize events aren't properly synchronized sometimes */
		if (!SDL_WaitForGPUIdle(render_state.device))
			return; /* maybe try again? this should never really return false... */
		render_state.swapchain_valid = true;
	}

	/* acquire command buffer and swapchain texture */
	SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(render_state.device);
	if (!cmd)
		return;

	SDL_GPUTexture *swapchain_texture = NULL;
	uint32_t w = 0, h = 0;
	if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, window, &swapchain_texture, &w, &h))
	{
		SDL_CancelGPUCommandBuffer(cmd);
		return;
	}

	if (!swapchain_texture)
	{
		SDL_CancelGPUCommandBuffer(cmd);
		return;
	}

	if (!create_depth_texture(render_state.device, w, h))
	{
		SDL_CancelGPUCommandBuffer(cmd);
		return;
	}

	/* setup matrices and aspect ratio like OpenGL */
	float projection[16], view[16], model[16], mvp[16];

	float h_aspect = (float)h / (float)w;
	matrix_frustum(projection, -1.0f, 1.0f, -h_aspect, h_aspect, 5.0f, 60.0f);

	matrix_identity(view);
	matrix_translate(view, 0.0f, 0.0f, -40.0f);
	matrix_rotate_x(view, render_state.view_rotx);
	matrix_rotate_y(view, render_state.view_roty);
	matrix_rotate_z(view, render_state.view_rotz);

	/* replicate OpenGL eye-space lighting exactly */
	/* original OpenGL light position in eye space: (5.0, 5.0, 10.0, 0.0) */
	float eye_light_dir[3] = {5.0f, 5.0f, 10.0f};

	/* setup render pass */
	SDL_GPUColorTargetInfo color_target = {.texture = swapchain_texture,
	                                       .mip_level = 0,
	                                       .layer_or_depth_plane = 0,
	                                       .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
	                                       .load_op = SDL_GPU_LOADOP_CLEAR,
	                                       .store_op = SDL_GPU_STOREOP_STORE,
	                                       .resolve_texture = NULL,
	                                       .resolve_mip_level = 0,
	                                       .resolve_layer = 0,
	                                       .cycle = false,
	                                       .cycle_resolve_texture = false};

	SDL_GPUDepthStencilTargetInfo depth_target = {.texture = render_state.depth_texture,
	                                              .clear_depth = 1.0f,
	                                              .load_op = SDL_GPU_LOADOP_CLEAR,
	                                              .store_op = SDL_GPU_STOREOP_DONT_CARE,
	                                              .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
	                                              .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
	                                              .cycle = false,
	                                              .clear_stencil = 0};

	SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, &depth_target);

	/* setup viewport */
	SDL_GPUViewport viewport = {.x = 0, .y = 0, .w = (float)w, .h = (float)h, .min_depth = 0.0f, .max_depth = 1.0f};
	SDL_SetGPUViewport(render_pass, &viewport);

	/* bind pipeline */
	SDL_BindGPUGraphicsPipeline(render_pass, render_state.pipeline);

	/* draw gears */
	struct
	{
		float x, y, z;
		float rot;
	} gear_transforms[3] = {
	    {-3.0f, -2.0f, 0.0f, render_state.angle}, {3.1f, -2.0f, 0.0f, -2.0f * render_state.angle - 9.0f}, {-3.1f, 4.2f, 0.0f, -2.0f * render_state.angle - 25.0f}};

	for (int i = 0; i < 3; i++)
	{
		matrix_identity(model);
		matrix_translate(model, gear_transforms[i].x, gear_transforms[i].y, gear_transforms[i].z);
		matrix_rotate_z(model, gear_transforms[i].rot);

		/* compute model-view matrix for proper view-space lighting */
		float model_view[16];
		matrix_multiply(model_view, view, model);

		matrix_multiply(mvp, projection, model_view);

		/* setup uniforms with proper std140 layout */
		Uniforms uniforms = {0};
		memcpy(uniforms.mvp_matrix, mvp, sizeof(mvp));
		memcpy(uniforms.model_matrix, model, sizeof(model));
		/* extract normal matrix from model-view for view-space lighting */
		matrix_extract_3x3_std140(uniforms.normal_matrix, model_view);

		/* use eye-space light direction directly (like OpenGL) */
		uniforms.light_position[0] = eye_light_dir[0];
		uniforms.light_position[1] = eye_light_dir[1];
		uniforms.light_position[2] = eye_light_dir[2];
		uniforms.light_position[3] = 0.0f; /* w=0 for directional light */

		uniforms.light_color[0] = 1.0f;
		uniforms.light_color[1] = 1.0f;
		uniforms.light_color[2] = 1.0f;
		uniforms.light_color[3] = 0.0f; /* padding */

		uniforms.object_color[0] = render_state.gears[i].color[0];
		uniforms.object_color[1] = render_state.gears[i].color[1];
		uniforms.object_color[2] = render_state.gears[i].color[2];
		uniforms.object_color[3] = 0.0f; /* padding */

		/* push uniforms to vertex shader */
		SDL_PushGPUVertexUniformData(cmd, 0, &uniforms, sizeof(uniforms));

		/* bind vertex buffer */
		SDL_GPUBufferBinding vertex_binding = {.buffer = render_state.gears[i].vertex_buffer, .offset = 0};
		SDL_BindGPUVertexBuffers(render_pass, 0, &vertex_binding, 1);

		/* bind index buffer */
		SDL_GPUBufferBinding index_binding = {.buffer = render_state.gears[i].index_buffer, .offset = 0};
		SDL_BindGPUIndexBuffer(render_pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

		/* draw */
		SDL_DrawGPUIndexedPrimitives(render_pass, render_state.gears[i].index_count, 1, 0, 0, 0);
	}

	SDL_EndGPURenderPass(render_pass);
	SDL_SubmitGPUCommandBuffer(cmd);

	frames++;

	if (tRate0 < 0.0)
		tRate0 = t;
	if (t - tRate0 >= 5.0)
	{
		double seconds = t - tRate0;
		double fps = frames / seconds;
		printf("%d frames in %3.1f seconds = %6.3f FPS\n", frames, seconds, fps);
		fflush(stdout);
		tRate0 = t;
		frames = 0;
	}
}

/*
 * Copyright (C) 2025 William Horvath
 */

#include <stdio.h>

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_time.h>
#include <SDL3/SDL_timer.h>

#include "sdlgpu_math.h"
#include "sdlgpu_render.h"

#ifdef __cplusplus
#define Z_INIT {}
#else
#define Z_INIT {0}
#endif

/* global */
RenderState render_state = Z_INIT;

static inline double current_time(void)
{
	static SDL_Time time = 0;
	if (!SDL_GetCurrentTime(&time))
		printf("SDL_GetCurrentTime error: %s\n", SDL_GetError());

	return (double)time / (double)SDL_NS_PER_SECOND;
}

static bool create_depth_texture(SDL_GPUDevice *device, uint32_t width, uint32_t height);
void draw_frame(SDL_Window *window)
{
	/* uniform data passed to shaders, in std140 layout */
	static struct Uniforms
	{
		float mvp_matrix[16];    /* mat4: 64 bytes */
		float model_matrix[16];  /* mat4: 64 bytes */
		float normal_matrix[12]; /* mat3 in std140: 3 vec3s, each padded to vec4 = 48 bytes */
		float light_position[4]; /* vec3 padded to vec4: 16 bytes */
		float light_color[4];    /* vec3 padded to vec4: 16 bytes */
		float object_color[4];   /* vec3 padded to vec4: 16 bytes */
	} uniforms = Z_INIT;

	static int frames = 0;
	static double tRot0 = -1.0;
	static double tRate0 = -1.0;
	double dt = 0.0f;
	double t = current_time();

	if (tRot0 < 0.0)
		tRot0 = t;
	dt = t - tRot0;
	tRot0 = t;

	if (!render_state.pause_animation)
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
	float *model = uniforms.model_matrix;
	float *mvp = uniforms.mvp_matrix;
	float projection[16], view[16];

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

/* lazy creation/recreation */
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

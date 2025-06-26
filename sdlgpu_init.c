#include <stdio.h>

#include "sdlgpu_init.h"
#include "sdlgpu_render.h"

/* pre-compiled shaders */

static const unsigned char vertex_shader[] = {
#ifdef _WIN32
#embed "vertex.dxil"
#else
#embed "vertex.spv"
#endif
};

static const unsigned char fragment_shader[] = {
#ifdef _WIN32
#embed "fragment.dxil"
#else
#embed "fragment.spv"
#endif
};

// #define DEBUG_SHADERS

bool init_gpu(SDL_Window *window)
{
	/* create GPU device */
	SDL_PropertiesID props = SDL_CreateProperties();

#if defined(DEBUG_SHADERS) || (defined(_DEBUG) && !defined(_WIN32)) // SDL tries to load "dxgidebug.dll" for windows, isn't always available (e.g. wine)
	SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, true);
#else
	SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, false);
#endif

#ifdef _WIN32
	SDL_SetStringProperty(props, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, "direct3d12");
	SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN, true);
#else
	SDL_SetStringProperty(props, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, "vulkan");
	SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true);
#endif

	render_state.device = SDL_CreateGPUDeviceWithProperties(props);
	SDL_DestroyProperties(props);

	if (!render_state.device)
	{
		printf("Failed to create GPU device: %s\n", SDL_GetError());
		return false;
	}

	/* claim window for GPU rendering */
	if (!SDL_ClaimWindowForGPUDevice(render_state.device, window))
	{
		printf("Failed to claim window for GPU device: %s\n", SDL_GetError());
		return false;
	}

	/* create shaders */
#ifdef _WIN32
	SDL_GPUShaderFormat shader_format = SDL_GPU_SHADERFORMAT_DXIL;
#else
	SDL_GPUShaderFormat shader_format = SDL_GPU_SHADERFORMAT_SPIRV;
#endif

	SDL_GPUShaderCreateInfo vertex_shader_info = {.code_size = sizeof(vertex_shader),
	                                              .code = vertex_shader,
	                                              .entrypoint = "main",
	                                              .format = shader_format,
	                                              .stage = SDL_GPU_SHADERSTAGE_VERTEX,
	                                              .num_samplers = 0,
	                                              .num_storage_textures = 0,
	                                              .num_storage_buffers = 0,
	                                              .num_uniform_buffers = 1,
	                                              .props = 0};

	SDL_GPUShaderCreateInfo fragment_shader_info = {.code_size = sizeof(fragment_shader),
	                                                .code = fragment_shader,
	                                                .entrypoint = "main",
	                                                .format = shader_format,
	                                                .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
	                                                .num_samplers = 0,
	                                                .num_storage_textures = 0,
	                                                .num_storage_buffers = 0,
	                                                .num_uniform_buffers = 0,
	                                                .props = 0};

	render_state.vertex_shader = SDL_CreateGPUShader(render_state.device, &vertex_shader_info);
	render_state.fragment_shader = SDL_CreateGPUShader(render_state.device, &fragment_shader_info);

	if (!render_state.vertex_shader || !render_state.fragment_shader)
	{
		printf("Failed to create shaders: %s\n", SDL_GetError());
		return false;
	}

	/* create graphics pipeline */
	SDL_GPUVertexAttribute vertex_attributes[2] = {{.location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = 0},
	                                               {.location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = 12}};

	SDL_GPUVertexBufferDescription vertex_buffer_desc = {.slot = 0, .pitch = sizeof(Vertex), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX, .instance_step_rate = 0};

	SDL_GPUVertexInputState vertex_input_state = {
	    .vertex_buffer_descriptions = &vertex_buffer_desc, .num_vertex_buffers = 1, .vertex_attributes = vertex_attributes, .num_vertex_attributes = 2};

	SDL_GPUColorTargetDescription color_target = {
	    .format = SDL_GetGPUSwapchainTextureFormat(render_state.device, window),
	    .blend_state = {.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
	                    .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO,
	                    .color_blend_op = SDL_GPU_BLENDOP_ADD,
	                    .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
	                    .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO,
	                    .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
	                    .color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A,
	                    .enable_blend = false,
	                    .enable_color_write_mask = false}};

	SDL_GPUGraphicsPipelineTargetInfo target_info = {.color_target_descriptions = &color_target,
	                                                 .num_color_targets = 1,
	                                                 .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
	                                                 .has_depth_stencil_target = true};

	SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {.vertex_shader = render_state.vertex_shader,
	                                                   .fragment_shader = render_state.fragment_shader,
	                                                   .vertex_input_state = vertex_input_state,
	                                                   .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
	                                                   .rasterizer_state = {.fill_mode = SDL_GPU_FILLMODE_FILL,
	                                                                        .cull_mode = SDL_GPU_CULLMODE_BACK,
	                                                                        .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
	                                                                        .depth_bias_constant_factor = 0.0f,
	                                                                        .depth_bias_clamp = 0.0f,
	                                                                        .depth_bias_slope_factor = 0.0f,
	                                                                        .enable_depth_bias = false,
	                                                                        .enable_depth_clip = true},
	                                                   .multisample_state = {.sample_count = SDL_GPU_SAMPLECOUNT_1, .sample_mask = 0, .enable_mask = false},
	                                                   .depth_stencil_state = {.compare_op = SDL_GPU_COMPAREOP_LESS,
	                                                                           .back_stencil_state = {0},
	                                                                           .front_stencil_state = {0},
	                                                                           .compare_mask = 0,
	                                                                           .write_mask = 0,
	                                                                           .enable_depth_test = true,
	                                                                           .enable_depth_write = true,
	                                                                           .enable_stencil_test = false},
	                                                   .target_info = target_info,
	                                                   .props = 0};

	render_state.pipeline = SDL_CreateGPUGraphicsPipeline(render_state.device, &pipeline_info);
	if (!render_state.pipeline)
	{
		printf("Failed to create graphics pipeline: %s\n", SDL_GetError());
		return false;
	}

	/* create gears */
	float red[3] = {0.8f, 0.1f, 0.0f};
	float green[3] = {0.0f, 0.8f, 0.2f};
	float blue[3] = {0.2f, 0.2f, 1.0f};

	if (!create_gear(render_state.device, &render_state.gears[0], 1.0f, 4.0f, 1.0f, 20, 0.7f, red) ||
	    !create_gear(render_state.device, &render_state.gears[1], 0.5f, 2.0f, 2.0f, 10, 0.7f, green) ||
	    !create_gear(render_state.device, &render_state.gears[2], 1.3f, 2.0f, 0.5f, 10, 0.7f, blue))
	{
		printf("Failed to create gear geometry\n");
		return false;
	}

	/* initialize view parameters */
	render_state.view_rotx = 20.0f;
	render_state.view_roty = 30.0f;
	render_state.view_rotz = 0.0f;
	render_state.angle = 0.0f;

	render_state.swapchain_valid = true;

	return true;
}

void cleanup_gpu(void)
{
	if (render_state.device)
	{
		for (int i = 0; i < 3; i++)
		{
			if (render_state.gears[i].vertex_buffer)
				SDL_ReleaseGPUBuffer(render_state.device, render_state.gears[i].vertex_buffer);
			if (render_state.gears[i].index_buffer)
				SDL_ReleaseGPUBuffer(render_state.device, render_state.gears[i].index_buffer);
		}

		if (render_state.depth_texture)
			SDL_ReleaseGPUTexture(render_state.device, render_state.depth_texture);
		if (render_state.pipeline)
			SDL_ReleaseGPUGraphicsPipeline(render_state.device, render_state.pipeline);
		if (render_state.vertex_shader)
			SDL_ReleaseGPUShader(render_state.device, render_state.vertex_shader);
		if (render_state.fragment_shader)
			SDL_ReleaseGPUShader(render_state.device, render_state.fragment_shader);

		SDL_DestroyGPUDevice(render_state.device);
	}

	memset(&render_state, 0, sizeof(render_state));
}

void set_swapchain_params(SDL_Window *window, SDL_GPUPresentMode *present_mode, unsigned int *image_count)
{
	/* the documentation says this is always supported, but not in reality... */
	if (!SDL_WindowSupportsGPUSwapchainComposition(render_state.device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR))
	{
		printf("Warning: GPU swapchain composition isn't supported for setting a custom present mode: %s\n", SDL_GetError());
		*present_mode = SDL_GPU_PRESENTMODE_VSYNC;
	}
	else
	{
		/* check if requested present mode is supported and set it */
		if (!SDL_WindowSupportsGPUPresentMode(render_state.device, window, *present_mode))
		{
			printf("Notice: %s present mode not supported, using vsync\n", *present_mode == SDL_GPU_PRESENTMODE_MAILBOX ? "mailbox" : "immediate");
			*present_mode = SDL_GPU_PRESENTMODE_VSYNC;
		}

		/* this is already the default, so only set the present_mode explicitly if it's not VSYNC */
		if (*present_mode != SDL_GPU_PRESENTMODE_VSYNC)
		{
			if (!SDL_SetGPUSwapchainParameters(render_state.device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, *present_mode))
			{
				printf("Warning: couldn't set swapchain parameters for custom present mode: %s\n", SDL_GetError());
				*present_mode = SDL_GPU_PRESENTMODE_VSYNC; /* if it failed, that means it must be using the "always supported" VSYNC */
			}
		}
	}

	/* 2 is already default, so only set image_count explicitly if it's requested to be different */
	if (!SDL_SetGPUAllowedFramesInFlight(render_state.device, *image_count))
	{
		printf("Warning: couldn't set max frames in flight to %d: %s\n", *image_count, SDL_GetError());
		*image_count = 2;
	}
}

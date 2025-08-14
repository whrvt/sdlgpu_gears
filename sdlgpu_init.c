/*
 * Copyright (C) 2025 William Horvath
 */

#include <assert.h>
#include <stdio.h>

#include <SDL3/SDL_gpu.h>

#include "sdlgpu_gear_creation.h"
#include "sdlgpu_init.h"
#include "sdlgpu_render.h"
#include "sdlgpu_shader_data.h"

#ifdef __cplusplus
#define Z_INIT \
	{ \
	}
#else
#define Z_INIT {0}
#endif

// #define DEBUG_SHADERS

/* SDL tries to load "dxgidebug.dll" for windows, isn't always available (e.g. wine) */
#if defined(DEBUG_SHADERS) || (defined(_DEBUG) && !defined(_WIN32))
#define SHADER_DEBUG_VAL true
#else
#define SHADER_DEBUG_VAL false
#endif

static int init_with_retry(InitParams *usercfg);
bool init_gpu(InitParams *usercfg)
{
	int success = init_with_retry(usercfg);

	/* cycle through available sdl drivers */
	while (success == 0)
	{
		cleanup_gpu();
		success = init_with_retry(usercfg);
	}

	return (success == 1);
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

static void set_swapchain_params(SDL_Window *window, enum PresentMode *present_mode, unsigned int *image_count);
static Renderer get_actual_renderer(Renderer choice, bool print_driver_enumeration);
static int init_with_retry(InitParams *usercfg)
{
	Renderer actual_renderer = get_actual_renderer(usercfg->renderer, usercfg->verbose);
	if (actual_renderer == DEFAULT)
	{
		printf("Failed to find a usable renderer\n");
		return -1; /* exhausted */
	}

	/* create GPU device */
	SDL_PropertiesID props = SDL_CreateProperties();
	assert(props > 0);

	SDL_GPUShaderFormat shader_format = 0;

	const unsigned char *vsh = NULL;
	unsigned long long vsh_size = 0;

	const unsigned char *fsh = NULL;
	unsigned long long fsh_size = 0;

	if (actual_renderer == VULKAN)
	{
		SDL_SetStringProperty(props, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, "vulkan");
		SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true);

		shader_format = SDL_GPU_SHADERFORMAT_SPIRV;
		vsh = vsh_spv;
		vsh_size = vsh_spv_size();
		fsh = fsh_spv;
		fsh_size = fsh_spv_size();
	}
	else
	{
		SDL_SetStringProperty(props, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, "direct3d12");
		SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN, true);

		shader_format = SDL_GPU_SHADERFORMAT_DXIL;
		vsh = vsh_dx;
		vsh_size = vsh_dx_size();
		fsh = fsh_dx;
		fsh_size = fsh_dx_size();
	}

	SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, SHADER_DEBUG_VAL);

	render_state.device = SDL_CreateGPUDeviceWithProperties(props);
	SDL_DestroyProperties(props);

	if (!render_state.device)
	{
		printf("Failed to create GPU device: %s\n", SDL_GetError());
		return 0;
	}

	/* claim window for GPU rendering */
	if (!SDL_ClaimWindowForGPUDevice(render_state.device, usercfg->window))
	{
		printf("Failed to claim window for GPU device: %s\n", SDL_GetError());
		return 0;
	}

	/* create shaders */
	SDL_GPUShaderCreateInfo vertex_shader_info = {.code_size = vsh_size,
	                                              .code = vsh,
	                                              .entrypoint = "main",
	                                              .format = shader_format,
	                                              .stage = SDL_GPU_SHADERSTAGE_VERTEX,
	                                              .num_samplers = 0,
	                                              .num_storage_textures = 0,
	                                              .num_storage_buffers = 0,
	                                              .num_uniform_buffers = 1,
	                                              .props = 0};

	SDL_GPUShaderCreateInfo fragment_shader_info = {.code_size = fsh_size,
	                                                .code = fsh,
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
		return 0;
	}

	/* create graphics pipeline */
	SDL_GPUVertexAttribute vertex_attributes[2] = {{.location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = 0},
	                                               {.location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = 12}};

	SDL_GPUVertexBufferDescription vertex_buffer_desc = {.slot = 0, .pitch = sizeof(Vertex), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX, .instance_step_rate = 0};

	SDL_GPUVertexInputState vertex_input_state = {
	    .vertex_buffer_descriptions = &vertex_buffer_desc, .num_vertex_buffers = 1, .vertex_attributes = vertex_attributes, .num_vertex_attributes = 2};

	SDL_GPUColorTargetDescription color_target = {
	    .format = SDL_GetGPUSwapchainTextureFormat(render_state.device, usercfg->window),
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

	SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {
	    .vertex_shader = render_state.vertex_shader,
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
	    .multisample_state = {.sample_count = SDL_GPU_SAMPLECOUNT_1, .sample_mask = 0, .enable_mask = false, .enable_alpha_to_coverage = false},
	    .depth_stencil_state = {.compare_op = SDL_GPU_COMPAREOP_LESS,
	                            .back_stencil_state = Z_INIT,
	                            .front_stencil_state = Z_INIT,
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
		return 0;
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
		return 0;
	}

	/* initialize view parameters */
	render_state.view_rotx = 20.0f;
	render_state.view_roty = 30.0f;
	render_state.view_rotz = 0.0f;
	render_state.angle = 0.0f;

	render_state.swapchain_valid = true;

	set_swapchain_params(usercfg->window, &usercfg->present_mode, &usercfg->image_count);

	if (usercfg->verbose)
	{
		printf("GPU driver: %s\n", SDL_GetGPUDeviceDriver(render_state.device));
		printf("Shader formats: 0x%08X\n", SDL_GetGPUShaderFormats(render_state.device));
		const char *present_mode_name = "UNKNOWN";
		switch (usercfg->present_mode)
		{
		case VSYNC:
			present_mode_name = "VSYNC";
			break;
		case IMMEDIATE:
			present_mode_name = "IMMEDIATE";
			break;
		case MAILBOX:
			present_mode_name = "MAILBOX";
			break;
		}
		printf("Present mode: %s\n", present_mode_name);
		printf("Image count: %u\n", usercfg->image_count);
	}

	/* save successful renderer */
	usercfg->renderer = actual_renderer;

	return 1;
}

static void set_swapchain_params(SDL_Window *window, enum PresentMode *present_mode, unsigned int *image_count)
{
	/* the documentation says this is always supported, but not in reality... */
	if (!SDL_WindowSupportsGPUSwapchainComposition(render_state.device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR))
	{
		printf("Warning: GPU swapchain composition isn't supported for setting a custom present mode: %s\n", SDL_GetError());
		*present_mode = VSYNC;
	}
	else
	{
		/* check if requested present mode is supported and set it */
		if (!SDL_WindowSupportsGPUPresentMode(render_state.device, window, (SDL_GPUPresentMode)*present_mode))
		{
			printf("Notice: %s present mode not supported, using vsync\n", *present_mode == MAILBOX ? "mailbox" : "immediate");
			*present_mode = VSYNC;
		}

		/* this is already the default, so only set the present_mode explicitly if it's not VSYNC */
		if (*present_mode != VSYNC)
		{
			if (!SDL_SetGPUSwapchainParameters(render_state.device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, (SDL_GPUPresentMode)*present_mode))
			{
				printf("Warning: couldn't set swapchain parameters for custom present mode: %s\n", SDL_GetError());
				*present_mode = VSYNC; /* if it failed, that means it must be using the "always supported" VSYNC */
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

static Renderer get_actual_renderer(Renderer choice, bool print_driver_enumeration)
{
	static bool printed = false;
	if (print_driver_enumeration)
	{
		if (printed)
			print_driver_enumeration = false;
		else /* print once */
			printed = true;
	}

	static bool vulkan_seen = false;
	static bool d3d12_seen = false;
	static Renderer ret_saved = DEFAULT;

	/* fallback logic (Windows can use either D3D12 or Vulkan) */
	if (ret_saved != DEFAULT)
	{
		if ((ret_saved == VULKAN) && d3d12_seen)
			return D3D12;
		else if ((ret_saved == D3D12) && vulkan_seen)
			return VULKAN;
		else /* error */
			return DEFAULT;
	}

	int num_avail = SDL_GetNumGPUDrivers();
	if (num_avail <= 0)
		return DEFAULT; /* error */

	if (print_driver_enumeration)
		printf("Found %d SDL_gpu driver backend%s:", num_avail, num_avail > 1 ? "s" : "");

	for (int i = 0; i < num_avail; i++)
	{
		const char *driver = SDL_GetGPUDriver(i);
		if (print_driver_enumeration)
			printf(" %s", driver);

		if (strcmp(driver, "vulkan") == 0)
			vulkan_seen = true;
		else if (strcmp(driver, "direct3d12") == 0)
			d3d12_seen = true;
	}

	if (print_driver_enumeration)
		printf(".\n");

	if (!d3d12_seen && !vulkan_seen) /* error */
		return DEFAULT;

	ret_saved = (vulkan_seen && (choice == VULKAN || !d3d12_seen)) ? VULKAN : D3D12;

	return ret_saved;
}

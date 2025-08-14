/*
 * Copyright (C) 2025 William Horvath
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct SDL_GPUBuffer SDL_GPUBuffer;
typedef struct SDL_GPUDevice SDL_GPUDevice;
typedef struct SDL_GPUGraphicsPipeline SDL_GPUGraphicsPipeline;
typedef struct SDL_GPUShader SDL_GPUShader;
typedef struct SDL_GPUTexture SDL_GPUTexture;
typedef struct SDL_Window SDL_Window;

/* vertex structure for gear geometry */
typedef struct Vertex
{
	float position[3];
	float normal[3];
} Vertex;

/* gear geometry data */
typedef struct GearData
{
	SDL_GPUBuffer *vertex_buffer;
	SDL_GPUBuffer *index_buffer;
	uint32_t index_count;
	float color[3];
} GearData;

/* rendering state */
typedef struct RenderState
{
	SDL_GPUDevice *device;
	SDL_GPUGraphicsPipeline *pipeline;
	SDL_GPUShader *vertex_shader;
	SDL_GPUShader *fragment_shader;
	SDL_GPUTexture *depth_texture;
	uint32_t depth_texture_width;
	uint32_t depth_texture_height;
	GearData gears[3];
	float view_rotx, view_roty, view_rotz;
	float angle;
	bool swapchain_valid;
	bool pause_animation;
} RenderState;

/* called from main loop */
void draw_frame(SDL_Window *window);

/* global render state info */
extern RenderState render_state;

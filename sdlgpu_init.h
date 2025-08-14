/*
 * Copyright (C) 2025 William Horvath
 */

#pragma once
#include <stdbool.h>

typedef struct SDL_Window SDL_Window;

typedef enum Renderer
{
	DEFAULT,
	VULKAN,
	D3D12
} Renderer;

typedef enum PresentMode /* this matches the SDL_GPUPresentMode enum exactly */
{
	VSYNC,
	IMMEDIATE,
	MAILBOX
} PresentMode;

typedef struct InitParams
{
	SDL_Window *window;
	PresentMode present_mode;
	Renderer renderer;
	unsigned int image_count;
	bool verbose;
} InitParams;

bool init_gpu(InitParams *usercfg);
void cleanup_gpu(void);

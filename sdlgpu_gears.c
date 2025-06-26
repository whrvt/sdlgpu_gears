/*
 * Copyright (C) 1999-2001  Brian Paul        All Rights Reserved.
 * Copyright (C) 2025       William Horvath   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <SDL3/SDL.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "sdlgpu_init.h"
#include "sdlgpu_render.h"

/* globals */
bool animate = true;
RenderState render_state = {};

/* event handler results */
enum
{
	NOP = 0,
	EXIT = 1,
	DRAW = 2
};

static int handle_event(SDL_Event *event)
{
	switch (event->type)
	{
	case SDL_EVENT_QUIT:
		return EXIT;
	case SDL_EVENT_KEY_DOWN:
		switch (event->key.key)
		{
		case SDLK_LEFT:
			render_state.view_roty += 5.0f;
			break;
		case SDLK_RIGHT:
			render_state.view_roty -= 5.0f;
			break;
		case SDLK_UP:
			render_state.view_rotx += 5.0f;
			break;
		case SDLK_DOWN:
			render_state.view_rotx -= 5.0f;
			break;
		case SDLK_ESCAPE:
			return EXIT;
		case SDLK_A:
			animate = !animate;
			break;
		default:
			break;
		}
		return DRAW;
	case SDL_EVENT_WINDOW_EXPOSED:
		return DRAW;
	case SDL_EVENT_WINDOW_RESIZED:
	case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
		/* invalidate now, so that we can wait until the gpu is idle to recreate the swapchain with the right size
		 * this might be an SDL_gpu bug, because i'm pretty sure it's supposed to handle resizing internally */
		render_state.swapchain_valid = 0;
		return DRAW;
	default:
		break;
	}
	return NOP;
}

static void event_loop(SDL_Window *window)
{
	SDL_Event event;

	while (1)
	{
		int op = NOP;

		while (!animate || SDL_PollEvent(&event))
		{
			if (!animate)
				SDL_WaitEvent(&event);

			op = handle_event(&event);
			if (op == EXIT)
				return;
			if (op == DRAW)
				break;
		}

		draw_frame(window);
	}
}

static void usage(void)
{
	printf("Usage:\n");
	/*TODO: printf("  -samples N              run in multisample mode with at least N samples\n"); */
	printf("  -fullscreen             run in fullscreen mode\n");
	printf("  -info                   display GPU renderer info\n");
	printf("  -geometry WxH+X+Y       window geometry\n");
	printf("  -present_mode MODE      presentation mode: vsync, immediate, mailbox (default: mailbox)\n");
	printf("  -image_count N          force the maximum number of frames queued on the gpu (default: 2, min: 1, max: 3)\n");
}

int main(int argc, char *argv[])
{
	SDL_Window *window = NULL;

	int win_width = 300;
	int win_height = 300;
	int x = SDL_WINDOWPOS_CENTERED;
	int y = SDL_WINDOWPOS_CENTERED;

	int samples = 0; /* TODO?: doesn't do anything atm */
	(void)samples;

	bool fullscreen = false;

	SDL_GPUPresentMode present_mode = SDL_GPU_PRESENTMODE_MAILBOX; /* prefer mailbox, fallback to vsync */

	unsigned int image_count = 2;

	bool print_info = false;

	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-info") == 0)
		{
			print_info = true;
		}
		else if (i < argc - 1 && strcmp(argv[i], "-samples") == 0)
		{
			samples = (int)strtol(argv[i + 1], NULL, 0);
			++i;
		}
		else if (i < argc - 1 && strcmp(argv[i], "-image_count") == 0)
		{
			image_count = (unsigned int)strtoul(argv[i + 1], NULL, 0);
			if (image_count < 1)
			{
				image_count = 1;
			}
			else if (image_count > 3)
			{
				image_count = 3;
			}
			++i;
		}
		else if (strcmp(argv[i], "-fullscreen") == 0)
		{
			fullscreen = true;
		}
		else if (i < argc - 1 && strcmp(argv[i], "-present_mode") == 0)
		{
			char *mode = argv[i + 1];
			if (strcmp(mode, "vsync") == 0)
			{
				present_mode = SDL_GPU_PRESENTMODE_VSYNC;
			}
			else if (strcmp(mode, "immediate") == 0)
			{
				present_mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
			}
			else if (strcmp(mode, "mailbox") == 0)
			{
				present_mode = SDL_GPU_PRESENTMODE_MAILBOX;
			}
			else
			{
				printf("Error: invalid present mode '%s'\n", mode);
				usage();
				return -1;
			}
			i++;
		}
		else if (i < argc - 1 && strcmp(argv[i], "-geometry") == 0)
		{
			char *geom = argv[i + 1];
			char *ptr = strchr(geom, 'x');
			if (ptr)
			{
				*ptr = '\0';
				win_width = (int)strtol(geom, NULL, 0);
				geom = ptr + 1;

				ptr = strchr(geom, '+');
				if (ptr)
				{
					*ptr = '\0';
					win_height = (int)strtol(geom, NULL, 0);
					geom = ptr + 1;

					x = (int)strtol(geom, NULL, 0);
					ptr = strchr(geom, '+');
					if (ptr)
						y = (int)strtol(ptr + 1, NULL, 0);
				}
				else
				{
					win_height = (int)strtol(geom, NULL, 0);
				}
			}
			i++;
		}
		else
		{
			usage();
			return -1;
		}
	}

	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		printf("Error: couldn't initialize SDL: %s\n", SDL_GetError());
		return -1;
	}

	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "sdlgpu_gears");
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, x);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, y);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, win_width);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, win_height);
	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, fullscreen);
	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, false); /* set it as resizeable only after it's created */

	window = SDL_CreateWindowWithProperties(props);
	SDL_DestroyProperties(props);

	if (!window)
	{
		printf("Error: couldn't create window: %s\n", SDL_GetError());
		SDL_Quit();
		return -1;
	}

	/* make sure the window state (size/position/fullscreen) has settled before doing anything else */
	SDL_SyncWindow(window);
	SDL_SetWindowResizable(window, true);

	if (!init_gpu(window))
	{
		SDL_DestroyWindow(window);
		SDL_Quit();
		return -1;
	}

	set_swapchain_params(window, &present_mode, &image_count);

	if (print_info)
	{
		printf("GPU driver: %s\n", SDL_GetGPUDeviceDriver(render_state.device));
		printf("Shader formats: 0x%08X\n", SDL_GetGPUShaderFormats(render_state.device));
		const char *present_mode_name = "UNKNOWN";
		switch (present_mode)
		{
		case SDL_GPU_PRESENTMODE_VSYNC:
			present_mode_name = "VSYNC";
			break;
		case SDL_GPU_PRESENTMODE_IMMEDIATE:
			present_mode_name = "IMMEDIATE";
			break;
		case SDL_GPU_PRESENTMODE_MAILBOX:
			present_mode_name = "MAILBOX";
			break;
		}
		printf("Present mode: %s\n", present_mode_name);
		printf("Image count: %d\n", image_count);
	}

	event_loop(window);

	cleanup_gpu();
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}

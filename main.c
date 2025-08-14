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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>

#include "sdlgpu_init.h"
#include "sdlgpu_render.h"

/* event handler results */
typedef enum Action
{
	NOP = 0,
	EXIT = 1,
	DRAW = 2
} Action;

static Action handle_event(SDL_Event *event)
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
			render_state.pause_animation = !render_state.pause_animation;
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

		while (render_state.pause_animation || SDL_PollEvent(&event))
		{
			if (render_state.pause_animation)
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
#ifdef _WIN32
	printf("  -vulkan                 use the Vulkan backend instead of D3D12\n");
#define D3D_POSSIBLE 1
#else
#define D3D_POSSIBLE 0
#endif
}

int main(int argc, char *argv[])
{
	int win_width = 300;
	int win_height = 300;
	int x = SDL_WINDOWPOS_CENTERED;
	int y = SDL_WINDOWPOS_CENTERED;

	int samples = 0; /* TODO?: doesn't do anything atm */
	(void)samples;

	bool fullscreen = false;

	InitParams cfg = {.window = NULL,
	                  .present_mode = MAILBOX, /* prefer mailbox, fallback to vsync */
	                  .renderer = DEFAULT,     /* d3d12 on Windows, Vulkan otherwise */
	                  .image_count = 2,
	                  .verbose = false};

	for (int i = 1; i < argc; i++)
	{
		if (D3D_POSSIBLE && strcmp(argv[i], "-vulkan") == 0)
		{
			cfg.renderer = VULKAN;
		}
		else if (strcmp(argv[i], "-info") == 0)
		{
			cfg.verbose = true;
		}
		else if (i < argc - 1 && strcmp(argv[i], "-samples") == 0)
		{
			samples = (int)strtol(argv[i + 1], NULL, 0);
			++i;
		}
		else if (i < argc - 1 && strcmp(argv[i], "-image_count") == 0)
		{
			cfg.image_count = (unsigned int)strtoul(argv[i + 1], NULL, 0);
			if (cfg.image_count < 1)
			{
				cfg.image_count = 1;
			}
			else if (cfg.image_count > 3)
			{
				cfg.image_count = 3;
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
				cfg.present_mode = VSYNC;
			}
			else if (strcmp(mode, "immediate") == 0)
			{
				cfg.present_mode = IMMEDIATE;
			}
			else if (strcmp(mode, "mailbox") == 0)
			{
				cfg.present_mode = MAILBOX;
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

	/* also respect SDL hint */
	if (D3D_POSSIBLE && cfg.renderer != VULKAN)
	{
		const char *sdl_renderer_hint = SDL_GetHint(SDL_HINT_GPU_DRIVER);
		if (sdl_renderer_hint && strcmp(sdl_renderer_hint, "vulkan") == 0)
		{
			cfg.renderer = VULKAN;
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

	cfg.window = SDL_CreateWindowWithProperties(props);
	SDL_DestroyProperties(props);

	if (!cfg.window)
	{
		printf("Error: couldn't create window: %s\n", SDL_GetError());
		SDL_Quit();
		return -1;
	}

	/* make sure the window state (size/position/fullscreen) has settled before doing anything else */
	SDL_SyncWindow(cfg.window);
	SDL_SetWindowResizable(cfg.window, true);

	if (!init_gpu(&cfg))
	{
		cleanup_gpu();
		SDL_DestroyWindow(cfg.window);
		SDL_Quit();
		return -1;
	}

	event_loop(cfg.window);

	cleanup_gpu();
	SDL_DestroyWindow(cfg.window);
	SDL_Quit();

	return 0;
}

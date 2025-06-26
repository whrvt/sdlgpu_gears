#pragma once

typedef struct SDL_Window SDL_Window;

typedef enum SDL_GPUPresentMode SDL_GPUPresentMode;

bool init_gpu(SDL_Window *window);
void cleanup_gpu(void);

/* 0 = vsync, 1 = immediate, 2 = mailbox */
void set_swapchain_params(SDL_Window *window, SDL_GPUPresentMode *present_mode, unsigned int *image_count);
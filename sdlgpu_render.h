#pragma once

#include <SDL3/SDL_gpu.h>

/* vertex structure for gear geometry */
typedef struct
{
	float position[3];
	float normal[3];
} Vertex;

/* gear geometry data */
typedef struct
{
	SDL_GPUBuffer *vertex_buffer;
	SDL_GPUBuffer *index_buffer;
	uint32_t index_count;
	float color[3];
} GearData;

/* rendering state */
typedef struct
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
} RenderState;

extern RenderState render_state;
extern bool animate;

/* build a gear with some adjustable parameters */
bool create_gear(SDL_GPUDevice *device, GearData *gear_data, float inner_radius, float outer_radius, float width, int teeth, float tooth_depth,
                 const float color[3]);

void draw_frame(SDL_Window *window);

bool init_gpu(SDL_Window *window);
void cleanup_gpu(void);

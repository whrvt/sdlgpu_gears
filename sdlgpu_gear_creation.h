/*
 * Copyright (C) 2025 William Horvath
 */

#pragma once
#include <stdbool.h>

typedef struct SDL_GPUDevice SDL_GPUDevice;
typedef struct GearData GearData;

/* build a gear with some adjustable parameters */
bool create_gear(SDL_GPUDevice *device, GearData *gear_data, float inner_radius, float outer_radius, float width, int teeth, float tooth_depth,
                 const float color[3]);

/*
 * Copyright (C) 2025 William Horvath
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/* precompiled shaders, included as binary data in the executable */
extern const unsigned char vsh_spv[];
extern const unsigned char fsh_spv[];
unsigned long long vsh_spv_size(void);
unsigned long long fsh_spv_size(void);

/* Windows builds can use either Vulkan or D3D12 */
extern const unsigned char vsh_dx[];
extern const unsigned char fsh_dx[];
unsigned long long vsh_dx_size(void);
unsigned long long fsh_dx_size(void);

#ifdef __cplusplus
}
#endif

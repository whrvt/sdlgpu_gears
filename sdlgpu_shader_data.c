#include "sdlgpu_shader_data.h"

#if defined(__GNUC__)
#define HAVE_GNU_ASSEMBLER /* .incbin */
#endif

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 202311L)
#if (defined(__GNUC__) && (__GNUC__ >= 15)) || (defined(__clang__) && (__clang_major__ >= 19))
#define HAVE_EMBED /* C23 embed */
#endif
#elif defined(__cplusplus) && (defined(__cpp_pp_embed) && (__cpp_pp_embed >= 202502L))
#define HAVE_EMBED /* C++26 embed */
#endif
#if !(defined(HAVE_EMBED) || defined(HAVE_GNU_ASSEMBLER))
#error "Missing support for embedded binary resources (C23 #embed or GNU as-compatible assembler)"
#endif

#ifdef _WIN32

#ifdef _WIN64
#define INCBIN_PREFIX ""
#else
#define INCBIN_PREFIX "_"
#endif

#define INCBIN_(file, sym) \
	__asm__(".section .rdata,\"dr\"\n" \
	        ".balign 1\n" \
	        ".globl " INCBIN_PREFIX #sym "\n" INCBIN_PREFIX #sym ":\n" \
	        ".incbin \"" file "\"\n" \
	        ".globl " INCBIN_PREFIX #sym "_end\n" INCBIN_PREFIX #sym "_end:\n" \
	        ".balign 1\n" \
	        ".section .text\n");

#else
#define INCBIN_(file, sym) \
	__asm__(".section .rodata\n" \
	        ".balign 1\n" \
	        ".globl " #sym "\n" \
	        ".type " #sym ", @object\n" #sym ":\n" \
	        ".incbin \"" file "\"\n" \
	        ".globl " #sym "_end\n" \
	        ".type " #sym "_end, @object\n" #sym "_end:\n" \
	        ".size " #sym ", " #sym "_end - " #sym "\n" \
	        ".balign 1\n" \
	        ".section \".text\"\n")
#endif

/* Vulkan/SPIR-V shaders, all platforms */
#ifdef HAVE_EMBED
const unsigned char vsh_spv[] = {
#embed "vertex.spv"
};
const unsigned char fsh_spv[] = {
#embed "fragment.spv"
};
unsigned long long vsh_spv_size(void)
{
	return sizeof(vsh_spv);
}
unsigned long long fsh_spv_size(void)
{
	return sizeof(fsh_spv);
}
#else  /* HAVE_GNU_ASSEMBLER */
INCBIN_("vertex.spv", vsh_spv);
INCBIN_("fragment.spv", fsh_spv);
/* clang-format off */
#ifdef __cplusplus
extern "C" {
#endif
extern const unsigned char vsh_spv_end[];
extern const unsigned char fsh_spv_end[];
#ifdef __cplusplus
}
#endif
/* clang-format on */
unsigned long long vsh_spv_size(void)
{
	return &vsh_spv_end[0] - &vsh_spv[0];
}
unsigned long long fsh_spv_size(void)
{
	return &fsh_spv_end[0] - &fsh_spv[0];
}
#endif /* HAVE_EMBED || HAVE_GNU_ASSEMBLER */

/* DXIL/D3D12 shaders, Windows-only */
#ifdef _WIN32
#ifdef HAVE_EMBED
const unsigned char vsh_dx[] = {
#embed "vertex.dxil"
};
const unsigned char fsh_dx[] = {
#embed "fragment.dxil"
};
unsigned long long vsh_dx_size(void)
{
	return sizeof(vsh_dx);
}
unsigned long long fsh_dx_size(void)
{
	return sizeof(fsh_dx);
}
#else
INCBIN_("vertex.dxil", vsh_dx);
INCBIN_("fragment.dxil", fsh_dx);
/* clang-format off */
#ifdef __cplusplus
extern "C" {
#endif
extern const unsigned char vsh_dx_end[];
extern const unsigned char fsh_dx_end[];
#ifdef __cplusplus
}
#endif
/* clang-format on */
unsigned long long vsh_dx_size(void)
{
	return &vsh_dx_end[0] - &vsh_dx[0];
}
unsigned long long fsh_dx_size(void)
{
	return &fsh_dx_end[0] - &fsh_dx[0];
}
#endif
#else
/* dummy defines for platforms without D3D12 support */
const unsigned char vsh_dx[] = {(unsigned char)0};
const unsigned char fsh_dx[] = {(unsigned char)0};
unsigned long long vsh_dx_size(void)
{
	return 0;
}
unsigned long long fsh_dx_size(void)
{
	return 0;
}
#endif /* _WIN32 */

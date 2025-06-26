# SDL_gpu gears demo Makefile
# thanks Claude Sonnet 4.0!

# Project settings
NAME = sdlgpu_gears
TARGET = $(NAME)
SOURCES = sdlgpu_gears.c sdlgpu_render.c sdlgpu_init.c
HEADERS = sdlgpu_init.h sdlgpu_render.h sdlgpu_math.h

# Compiler settings
CC = cc
CFLAGS = -std=gnu23 -Wall -Wextra
LIBS = -lSDL3 -lm

# Windows cross-compilation settings
MINGW_PREFIX = x86_64-w64-mingw32
MINGW_CC = $(MINGW_PREFIX)-gcc
MINGW_PKG_CONFIG = $(MINGW_PREFIX)-pkg-config
MINGW_CFLAGS = -static -std=gnu23 -Wall -Wextra
MINGW_LIBS = -lm

# Build mode (debug/release)
MODE ?= release

ifeq ($(MODE),debug)
    CFLAGS += -g -O0 -D_DEBUG
    MINGW_CFLAGS += -g -O0 -D_DEBUG
    TARGET = $(NAME)-debug
else
    CFLAGS += -O2 -DNDEBUG
    MINGW_CFLAGS += -O2 -DNDEBUG
endif

# Shader files
VULKAN_SHADERS = vertex.spv fragment.spv
DXIL_SHADERS = vertex.dxil fragment.dxil
SHADER_SOURCES = vertex.glsl fragment.glsl vertex.hlsl fragment.hlsl

# Default target
.PHONY: all
all: shaders $(TARGET)

# Platform-specific builds
.PHONY: linux windows
linux: shaders-vulkan $(TARGET)
windows: shaders-dxil $(TARGET).exe

# Native Linux build
$(TARGET): $(SOURCES) $(HEADERS) $(VULKAN_SHADERS)
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LIBS)

# Windows cross-compilation build
$(TARGET).exe: $(SOURCES) $(HEADERS) $(DXIL_SHADERS)
	$(MINGW_CC) $(MINGW_CFLAGS) $$($(MINGW_PKG_CONFIG) --cflags --static sdl3) \
		$(SOURCES) -o $@ \
		$$($(MINGW_PKG_CONFIG) --libs --static sdl3) $(MINGW_LIBS)

# Shader compilation targets
.PHONY: shaders shaders-vulkan shaders-dxil
shaders: shaders-vulkan shaders-dxil

shaders-vulkan: $(VULKAN_SHADERS)

shaders-dxil: $(DXIL_SHADERS)

# Vulkan/SPIR-V shader compilation (requires Vulkan SDK)
vertex.spv: vertex.glsl
	@echo "Compiling vertex shader (SPIR-V)..."
	glslc -fshader-stage=vertex vertex.glsl -o vertex.spv

fragment.spv: fragment.glsl
	@echo "Compiling fragment shader (SPIR-V)..."
	glslc -fshader-stage=fragment fragment.glsl -o fragment.spv

# DirectX/DXIL shader compilation (requires DXC)
vertex.dxil: vertex.hlsl
	@echo "Compiling vertex shader (DXIL)..."
	dxc -T vs_6_0 -E main vertex.hlsl -Fo vertex.dxil

fragment.dxil: fragment.hlsl
	@echo "Compiling fragment shader (DXIL)..."
	dxc -T ps_6_0 -E main fragment.hlsl -Fo fragment.dxil

# Check for required tools
.PHONY: check-tools check-vulkan check-dxc check-mingw
check-tools: check-vulkan check-dxc

check-vulkan:
	@which glslc >/dev/null 2>&1 || { echo "Error: glslc not found. Install Vulkan SDK."; exit 1; }

check-dxc:
	@which dxc >/dev/null 2>&1 || { echo "Error: dxc not found. Install DirectX Shader Compiler."; exit 1; }

check-mingw:
	@which $(MINGW_CC) >/dev/null 2>&1 || { echo "Error: $(MINGW_CC) not found. Install mingw-w64."; exit 1; }
	@$(MINGW_PKG_CONFIG) --exists sdl3 || { echo "Error: SDL3 not found for mingw. Install mingw SDL3 development packages."; exit 1; }

# Development targets
.PHONY: debug
debug:
	$(MAKE) MODE=debug

.PHONY: run
run: $(TARGET)
	./$(TARGET)

.PHONY: run-debug
run-debug: debug
	./$(TARGET)

# Information targets
.PHONY: info
info:
	@echo "SDL_gpu gears demo build system"
	@echo "Available targets:"
	@echo "  all        - Build native binary with all shaders"
	@echo "  linux      - Build for Linux (native)"
	@echo "  windows    - Cross-compile for Windows"
	@echo "  debug      - Build with debug symbols"
	@echo "  shaders    - Compile all shaders"
	@echo "  run        - Build and run"
	@echo "  clean      - Remove build artifacts"
	@echo "  info       - Show this help"
	@echo ""
	@echo "Build modes (set with MODE=):"
	@echo "  release    - Optimized build (default)"
	@echo "  debug      - Debug build with symbols"
	@echo ""
	@echo "Examples:"
	@echo "  make                    # Build for current platform"
	@echo "  make windows            # Cross-compile for Windows"
	@echo "  make MODE=debug run     # Debug build and run"

# Shader size information
.PHONY: shader-info
shader-info: $(VULKAN_SHADERS) $(DXIL_SHADERS)
	@echo "Shader sizes:"
	@for shader in $(VULKAN_SHADERS) $(DXIL_SHADERS); do \
		if [ -f "$$shader" ]; then \
			echo "  $$shader: $$(wc -c < $$shader) bytes"; \
		fi; \
	done

# Clean up build artifacts
.PHONY: clean clean-shaders clean-all
clean:
	rm -f $(TARGET) $(TARGET)-debug $(TARGET).exe $(TARGET)-debug.exe

clean-shaders:
	rm -f $(VULKAN_SHADERS) $(DXIL_SHADERS)

clean-all: clean clean-shaders

# Install target (Linux only)
PREFIX ?= /usr/local
.PHONY: install uninstall
install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)

# Mark shader files as intermediate to prevent auto-deletion
.PRECIOUS: $(VULKAN_SHADERS) $(DXIL_SHADERS)

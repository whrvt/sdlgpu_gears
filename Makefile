# SDL_gpu gears demo Makefile

# Project settings
NAME = sdlgpu_gears
TARGET = $(NAME)
SOURCES = main.c sdlgpu_render.c sdlgpu_init.c sdlgpu_gear_creation.c sdlgpu_shader_data.c
HEADERS = sdlgpu_init.h sdlgpu_render.h sdlgpu_math.h sdlgpu_gear_creation.h sdlgpu_shader_data.h

PKG_CONFIG ?= pkg-config
SDL3_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags sdl3)
SDL3_LIBS ?= $(shell $(PKG_CONFIG) --libs sdl3)

# Compiler settings
CC ?= cc
CFLAGS = -Wall -Wextra $(SDL3_CFLAGS) -g
LIBS = -lm $(SDL3_LIBS) -g

# Windows cross-compilation settings
MINGW_PREFIX ?= x86_64-w64-mingw32
MINGW_CC ?= $(MINGW_PREFIX)-gcc
MINGW_PKG_CONFIG ?= $(MINGW_PREFIX)-pkg-config
MINGW_CFLAGS ?= -static -Wall -Wextra
MINGW_LIBS = -lm

# Build mode (debug/release)
MODE ?= release

ifeq ($(MODE),debug)
    CFLAGS += -O0 -D_DEBUG
    MINGW_CFLAGS += -O0 -D_DEBUG
    TARGET = $(NAME)-debug
else
    CFLAGS += -O2 -DNDEBUG
    MINGW_CFLAGS += -O2 -DNDEBUG
endif

CFLAGS += $(EXTRACFLAGS)
LIBS += $(EXTRALDFLAGS)
MINGW_CFLAGS += $(EXTRACFLAGS)
MINGW_LIBS += $(EXTRALDFLAGS)

# Detect host OS so the default target Does The Right Thing.
UNAME_S := $(shell uname -s)

# Shader files
VULKAN_SHADERS = vertex.spv fragment.spv
DXIL_SHADERS = vertex.dxil fragment.dxil
METAL_SHADERS = vertex.metal fragment.metal
SHADER_SOURCES = vertex.glsl fragment.glsl vertex.hlsl fragment.hlsl $(METAL_SHADERS)

# On Apple, the .metal source is embedded into the executable so the binary
# depends on it. Elsewhere it's a no-op.
ifeq ($(UNAME_S),Darwin)
NATIVE_SHADER_DEPS = $(VULKAN_SHADERS) $(METAL_SHADERS)
# Build into a proper .app bundle so macOS picks up the Info.plist (and
# Game Mode kicks in for fullscreen). The binary inside the bundle is
# always named $(NAME) to match CFBundleExecutable; the bundle directory
# carries the -debug suffix when MODE=debug. $(TARGET) at the project
# root is a convenience symlink pointing into the bundle.
APP_BUNDLE  := $(TARGET).app
APP_EXE     := $(APP_BUNDLE)/Contents/MacOS/$(NAME)
APP_INFO    := $(APP_BUNDLE)/Contents/Info.plist
APP_PKGINFO := $(APP_BUNDLE)/Contents/PkgInfo
else
NATIVE_SHADER_DEPS = $(VULKAN_SHADERS)
endif

# Default target
.PHONY: all
all: $(TARGET)

# Platform-specific builds
.PHONY: native linux macos windows
native: shaders-vulkan $(TARGET)
linux: native
macos: native
windows: shaders-vulkan shaders-dxil $(TARGET).exe

ifeq ($(UNAME_S),Darwin)
# Native build (macOS): produce $(APP_BUNDLE) and a top-level symlink.
$(TARGET): $(APP_EXE) $(APP_INFO) $(APP_PKGINFO)
	@ln -sfn $(APP_EXE) $@

$(APP_EXE): $(SOURCES) $(HEADERS) $(NATIVE_SHADER_DEPS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LIBS)

$(APP_INFO): Info.plist
	@mkdir -p $(@D)
	@cp $< $@

$(APP_PKGINFO):
	@mkdir -p $(@D)
	@printf 'APPL????' > $@
else
# Native build (Linux)
$(TARGET): $(SOURCES) $(HEADERS) $(NATIVE_SHADER_DEPS)
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LIBS)
endif

# Windows cross-compilation build
$(TARGET).exe: $(SOURCES) $(HEADERS) $(VULKAN_SHADERS) $(DXIL_SHADERS)
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
	@echo "  native     - Build for the host platform (Linux or macOS)"
	@echo "  linux      - Alias for native"
	@echo "  macos      - Alias for native"
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
	@echo "  make macos              # Build native macOS binary (Metal default)"
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
	rm -f $(NAME) $(NAME)-debug $(NAME).exe $(NAME)-debug.exe
ifeq ($(UNAME_S),Darwin)
	rm -rf $(NAME).app $(NAME)-debug.app
endif

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

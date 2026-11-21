# 1. Environment and OS Detection
ifeq ($(OS),Windows_NT)
    CC = C:/msys64/ucrt64/bin/gcc.exe
    GLSLC = glslc
    CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic -Wshadow -D_DEBUG -DUNICODE -D_UNICODE -DPHOENIX_STATIC_LINK -DVK_USE_64_BIT_PTR_DEFINES=1
    SHARED_FLAG = -shared
    EXE_FLAG = 
    LIBS = -luser32 -lgdi32 -lkernel32 -lwinmm -lshell32
    
    ifneq ($(VULKAN_SDK),)
        INCLUDES += -I"$(VULKAN_SDK)/Include"
        LDFLAGS += -L"$(VULKAN_SDK)/Lib" -lvulkan-1
        GLSLC = "$(VULKAN_SDK)/Bin/glslc"
    endif
else
    CC = x86_64-w64-mingw32-gcc
    CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic -Wshadow -D_DEBUG -DUNICODE -D_UNICODE -DPHOENIX_STATIC_LINK -DVK_USE_64_BIT_PTR_DEFINES=1
    SHARED_FLAG = -shared
    EXE_FLAG = 
    LIBS = -luser32 -lgdi32 -lkernel32 -lwinmm -lshell32
    
    # Check if the path value is already set before launching expensive background tasks
    ifndef VULKAN_LINUX_PATH
        WSL_VULKAN_SDK := $(shell cmd.exe /c "echo %VULKAN_SDK%" 2>/dev/null | tr -d '\r')
        ifneq ($(WSL_VULKAN_SDK),)
            VULKAN_LINUX_PATH := $(shell wslpath -u "$(WSL_VULKAN_SDK)")
        endif
    endif

    ifneq ($(VULKAN_LINUX_PATH),)
        INCLUDES += -I"$(VULKAN_LINUX_PATH)/Include"
        LDFLAGS += -L"$(VULKAN_LINUX_PATH)/Lib" -lvulkan-1
    endif

    # Fallback to Linux glslc if installed in WSL, otherwise route safely to Windows host binary via cmd
    IF_LINUX_GLSLC := $(shell which glslc 2>/dev/null)
    ifneq ($(IF_LINUX_GLSLC),)
        GLSLC = glslc
    else ifneq ($(WSL_VULKAN_SDK),)
        GLSLC = cmd.exe /c "$(WSL_VULKAN_SDK)\Bin\glslc.exe"
    else
        GLSLC = glslc.exe
    endif
endif

# Force colored diagnostics even though --output-sync pipes compiler output (no TTY)
CFLAGS += -fdiagnostics-color=always

# Vulkan validation layer: OFF by default. With an immediate-mode UI (thousands of draws per frame)
# it costs tens of ms of CPU per frame. Opt in with:  make VALIDATION=1
ifeq ($(VALIDATION),1)
    CFLAGS += -DPHOENIX_VULKAN_VALIDATION
endif

# Parallel build: one job per CPU core by default. Override with  make JOBS=4  or  make JOBS=1
ifeq ($(OS),Windows_NT)
    JOBS ?= $(NUMBER_OF_PROCESSORS)
else
    JOBS ?= $(shell nproc)
endif
MAKEFLAGS += -j$(JOBS) --output-sync=target

# Output Directories
BIN_DIR = 01_bin
OBJ_DIR = 02_obj

# Inclusive Include Matrix Matching
INCLUDES += -I03_system_editor/include \
            -I06_system_loader/include \
            -I09_system_installer/include \
            -I10_system_build/include \
            -I07_system_engine/include \
            -I08_system_game/include \
            -I08_system_game/include/system_core_game \
            -I08_system_game/include/system_game_management \
            -I08_system_game/include/system_state_game

# Target Outputs Linked to Output Binary Locations
ENGINE_EXE    = $(BIN_DIR)/phoenix_engine.exe
EDITOR_DLL    = $(BIN_DIR)/phoenix_editor.dll
GAME_DLL      = $(BIN_DIR)/system_game.dll
# the engine exe exports its symbols; the editor and game dlls link against this import library
ENGINE_IMPLIB = $(OBJ_DIR)/libphoenix_engine.a
PHOENIX_LOADER_DLL = $(BIN_DIR)/phoenix_loader.dll
PHOENIX_LOADER_OBJ = $(OBJ_DIR)/system_phoenix_loader.o

# build tool (10_system_build) and installer (09_system_installer): small stand alone exes, no engine, editor or vulkan.
# phoenix_build.exe turns 01_bin into a game folder without the editor. phoenix_installer.exe installs that folder.
BUILD_EXE     = $(BIN_DIR)/phoenix_build.exe
BUILD_OBJ     = $(OBJ_DIR)/system_build.o
INSTALLER_EXE = $(BIN_DIR)/phoenix_installer.exe
INSTALLER_OBJ = $(OBJ_DIR)/system_installer.o

# Shaders Build Target Targets
VERT_SPV = $(BIN_DIR)/object_vert.spv
FRAG_SPV = $(BIN_DIR)/object_frag.spv
GRID_VERT_SPV = $(BIN_DIR)/grid_vert.spv
GRID_FRAG_SPV = $(BIN_DIR)/grid_frag.spv
MESH_VERT_SPV = $(BIN_DIR)/mesh_vert.spv
MESH_FRAG_SPV = $(BIN_DIR)/mesh_frag.spv
BG_VERT_SPV = $(BIN_DIR)/bg_vert.spv
BG_FRAG_SPV = $(BIN_DIR)/bg_frag.spv
UIB_VERT_SPV = $(BIN_DIR)/ui_batch_vert.spv
UIB_FRAG_SPV = $(BIN_DIR)/ui_batch_frag.spv
FXAA_VERT_SPV = $(BIN_DIR)/fxaa_vert.spv
FXAA_FRAG_SPV = $(BIN_DIR)/fxaa_frag.spv

# -------------------------------------------------------------------------
# THE FOLDER DECIDES THE BINARY
#   03_system_editor/source/**  -> phoenix_editor.dll   (editor + 3d modeler)
#   07_system_engine/source/**  -> phoenix_engine.exe   (host, core, renderer)
#   08_system_game/source/**    -> system_game.dll      (game)
#   06_system_loader/source/**  -> phoenix_loader.dll   (model / image loader)
# Move a .c file to another module's folder and it is built into that binary.
# No per-file rules: object names are the file name, so file names must stay unique.
# -------------------------------------------------------------------------
EDITOR_SRC = $(wildcard 03_system_editor/source/*.c 03_system_editor/source/*/*.c 03_system_editor/source/*/*/*.c 03_system_editor/source/*/*/*/*.c)
EDITOR_SRC := $(filter-out %system_direction_light.c,$(EDITOR_SRC))
ENGINE_SRC = $(wildcard 07_system_engine/source/*.c 07_system_engine/source/*/*.c 07_system_engine/source/*/*/*.c 07_system_engine/source/*/*/*/*.c)
ENGINE_SRC := $(filter-out %engine_transform_gizmo.c %engine_scene_gizmo_overlay.c %engine_scene_manipulator.c,$(ENGINE_SRC))
GAME_SRC   = $(wildcard 08_system_game/source/*.c 08_system_game/source/*/*.c 08_system_game/source/*/*/*.c 08_system_game/source/*/*/*/*.c)

EDITOR_OBJS = $(addprefix $(OBJ_DIR)/,$(notdir $(EDITOR_SRC:.c=.o)))
ENGINE_OBJS = $(addprefix $(OBJ_DIR)/,$(notdir $(ENGINE_SRC:.c=.o)))
GAME_OBJS   = $(addprefix $(OBJ_DIR)/,$(notdir $(GAME_SRC:.c=.o)))

# lets one pattern rule find every .c file wherever it sits
vpath %.c $(sort $(dir $(EDITOR_SRC) $(ENGINE_SRC) $(GAME_SRC)))

.PHONY: all editor engine game loader build installer phoenix_loader clean clean_prefs clean_all clean_phoenix_loader directories shaders

# Build includes compiled shader binaries natively
all: directories shaders $(ENGINE_EXE) $(EDITOR_DLL) $(PHOENIX_LOADER_DLL) $(GAME_DLL) $(BUILD_EXE) $(INSTALLER_EXE)

shaders: $(VERT_SPV) $(FRAG_SPV) $(GRID_VERT_SPV) $(GRID_FRAG_SPV) $(MESH_VERT_SPV) $(MESH_FRAG_SPV) $(BG_VERT_SPV) $(BG_FRAG_SPV) $(UIB_VERT_SPV) $(UIB_FRAG_SPV) $(FXAA_VERT_SPV) $(FXAA_FRAG_SPV)

$(BIN_DIR)/object_vert.spv: 00_assets/shaders/object_vert.glsl
	@echo [Compiling Shader] $< -> $@
	@$(GLSLC) -fshader-stage=vertex $< -o $@

$(BIN_DIR)/object_frag.spv: 00_assets/shaders/object_frag.glsl
	@echo [Compiling Shader] $< -> $@
	@$(GLSLC) -fshader-stage=fragment $< -o $@

$(BIN_DIR)/grid_vert.spv: 00_assets/shaders/grid_vert.glsl
	@echo [Compiling Shader] $< -> $@
	@$(GLSLC) -fshader-stage=vertex $< -o $@

$(BIN_DIR)/grid_frag.spv: 00_assets/shaders/grid_frag.glsl
	@echo [Compiling Shader] $< -> $@
	@$(GLSLC) -fshader-stage=fragment $< -o $@

$(BIN_DIR)/mesh_vert.spv: 00_assets/shaders/mesh_vert.glsl
	@echo [Compiling Shader] $< -> $@
	@$(GLSLC) -fshader-stage=vertex $< -o $@

$(BIN_DIR)/mesh_frag.spv: 00_assets/shaders/mesh_frag.glsl
	@echo [Compiling Shader] $< -> $@
	@$(GLSLC) -fshader-stage=fragment $< -o $@

$(BIN_DIR)/bg_vert.spv: 00_assets/shaders/bg_vert.glsl
	@echo [Compiling Shader] $< -> $@
	@$(GLSLC) -fshader-stage=vertex $< -o $@

$(BIN_DIR)/bg_frag.spv: 00_assets/shaders/bg_frag.glsl
	@echo [Compiling Shader] $< -> $@
	@$(GLSLC) -fshader-stage=fragment $< -o $@

$(BIN_DIR)/ui_batch_vert.spv: 00_assets/shaders/ui_batch_vert.glsl
	@echo [Compiling Shader] $< -> $@
	@$(GLSLC) -fshader-stage=vertex $< -o $@

$(BIN_DIR)/ui_batch_frag.spv: 00_assets/shaders/ui_batch_frag.glsl
	@echo [Compiling Shader] $< -> $@
	@$(GLSLC) -fshader-stage=fragment $< -o $@

$(BIN_DIR)/fxaa_vert.spv: 00_assets/shaders/fxaa_vert.glsl
	@echo [Compiling Shader] $< -> $@
	@$(GLSLC) -fshader-stage=vertex $< -o $@

$(BIN_DIR)/fxaa_frag.spv: 00_assets/shaders/fxaa_frag.glsl
	@echo [Compiling Shader] $< -> $@
	@$(GLSLC) -fshader-stage=fragment $< -o $@

# Target shortcuts
engine: directories shaders $(ENGINE_EXE)
editor: directories $(EDITOR_DLL)
game: directories $(GAME_DLL)
loader: directories $(PHOENIX_LOADER_DLL)
build: directories $(BUILD_EXE)
installer: directories $(INSTALLER_EXE)

directories:
ifeq ($(OS),Windows_NT)
	@if not exist "$(BIN_DIR)" mkdir "$(BIN_DIR)"
	@if not exist "$(OBJ_DIR)" mkdir "$(OBJ_DIR)"
else
	@mkdir -p "$(BIN_DIR)"
	@mkdir -p "$(OBJ_DIR)"
endif

# order-only (the | ): the folders must exist first, but a newer folder never forces a rebuild
$(VERT_SPV) $(FRAG_SPV) $(3D_VERT_SPV) $(GRID_FRAG_SPV) $(MESH_VERT_SPV) $(MESH_FRAG_SPV) $(BG_VERT_SPV) $(BG_FRAG_SPV) $(UIB_VERT_SPV) $(UIB_FRAG_SPV) $(FXAA_VERT_SPV) $(FXAA_FRAG_SPV): | directories
$(ENGINE_EXE) $(EDITOR_DLL) $(GAME_DLL) $(PHOENIX_LOADER_DLL) $(BUILD_EXE) $(INSTALLER_EXE): | directories

# engine exe: exports every symbol and writes the import library the plugins link against
$(ENGINE_EXE): $(ENGINE_OBJS)
	@echo [Linking Executable with Vulkan] $@
	$(CC) $(CFLAGS) $(ENGINE_OBJS) -o $@ $(LDFLAGS) $(LIBS) $(EXE_FLAG) -Wl,--export-all-symbols -Wl,--out-implib,$(ENGINE_IMPLIB)

# plugins: built after the exe, because they need its import library
$(EDITOR_DLL): $(EDITOR_OBJS) $(ENGINE_EXE)
	@echo [Building Library] $@
	$(CC) $(CFLAGS) $(SHARED_FLAG) -Wl,--export-all-symbols $(EDITOR_OBJS) $(ENGINE_IMPLIB) -o $@ $(LDFLAGS) $(LIBS)

$(GAME_DLL): $(GAME_OBJS) $(ENGINE_EXE)
	@echo [Building Library] $@
	$(CC) $(CFLAGS) $(SHARED_FLAG) $(GAME_OBJS) $(ENGINE_IMPLIB) -o $@

# one rule for every source file of the editor, engine and game (found through vpath above)
$(OBJ_DIR)/%.o: %.c | directories
	@echo [Compiling Unit] $<
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# build tool and installer: own objects, no vpath, no engine import library
$(BUILD_OBJ): 10_system_build/source/system_build.c | directories
	@echo [Compiling Unit] $<
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_EXE): $(BUILD_OBJ)
	@echo [Linking Executable] $@
	$(CC) $(CFLAGS) $^ -o $@ -lkernel32

$(INSTALLER_OBJ): 09_system_installer/source/system_installer.c | directories
	@echo [Compiling Unit] $<
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(INSTALLER_EXE): $(INSTALLER_OBJ)
	@echo [Linking Executable] $@
	$(CC) $(CFLAGS) $^ -o $@ -lkernel32

# phoenix loader: its own dll. "make phoenix_loader" cleans and rebuilds ONLY this (06_system_loader -> 01_bin/phoenix_loader.dll).
# It is not part of "all" and not linked into the editor or the engine.
$(PHOENIX_LOADER_OBJ): 06_system_loader/source/system_phoenix_loader.c | directories
	@echo [Compiling Unit] $<
	$(CC) $(CFLAGS) -DPHOENIX_EXPORT $(INCLUDES) -c $< -o $@

$(PHOENIX_LOADER_DLL): $(PHOENIX_LOADER_OBJ)
	@echo [Building Library] $@
	$(CC) $(CFLAGS) -DPHOENIX_EXPORT $(SHARED_FLAG) $^ -o $@ $(LDFLAGS) $(LIBS)

phoenix_loader: directories
	@$(MAKE) --no-print-directory clean_phoenix_loader
	@$(MAKE) --no-print-directory $(PHOENIX_LOADER_DLL)

clean_phoenix_loader:
	@echo [Cleaning Phoenix Loader] Removing $(PHOENIX_LOADER_OBJ) and $(PHOENIX_LOADER_DLL)
ifeq ($(OS),Windows_NT)
	@if exist "$(PHOENIX_LOADER_OBJ)" del /q "$(PHOENIX_LOADER_OBJ)"
	@if exist "$(PHOENIX_LOADER_DLL)" del /q "$(PHOENIX_LOADER_DLL)"
else
	@rm -f "$(PHOENIX_LOADER_OBJ)" "$(PHOENIX_LOADER_DLL)"
endif

# settings file the editor writes next to the exe (SETTINGS_DB_FILE_NAME in system_settings_database.h).
# "make clean" keeps it. "make clean_prefs" deletes only it. "make clean_all" does both.
PREFS_DB = phoenix_editor.pdb

# clean empties 01_bin and 02_obj but keeps both folders and keeps $(PREFS_DB)
clean:
	@echo [Cleaning Workspace] Emptying $(BIN_DIR) and $(OBJ_DIR). Keeping the folders and $(PREFS_DB): use make clean_prefs to delete it.
ifeq ($(OS),Windows_NT)
	@if exist "$(BIN_DIR)\$(PREFS_DB)" move /y "$(BIN_DIR)\$(PREFS_DB)" "$(PREFS_DB).keep" >nul
	@if exist "$(BIN_DIR)\*" del /q "$(BIN_DIR)\*"
	@if exist "$(OBJ_DIR)\*" del /q "$(OBJ_DIR)\*"
	@if exist "$(PREFS_DB).keep" move /y "$(PREFS_DB).keep" "$(BIN_DIR)\$(PREFS_DB)" >nul
else
	@if [ -d "$(BIN_DIR)" ]; then find "$(BIN_DIR)" -mindepth 1 -maxdepth 1 -type f ! -name "$(PREFS_DB)" -delete; fi
	@if [ -d "$(OBJ_DIR)" ]; then find "$(OBJ_DIR)" -mindepth 1 -delete; fi
endif

clean_prefs:
	@echo [Cleaning Prefs] Deleting $(PREFS_DB)
ifeq ($(OS),Windows_NT)
	@if exist "$(BIN_DIR)\$(PREFS_DB)" del /q "$(BIN_DIR)\$(PREFS_DB)"
else
	@rm -f "$(BIN_DIR)/$(PREFS_DB)"
endif

.PHONY: clean_engine clean_editor clean_game

clean_engine:
	@echo [Cleaning Engine] Removing $(ENGINE_EXE), $(ENGINE_IMPLIB), and engine objects.
	@rm -f "$(ENGINE_EXE)" "$(ENGINE_IMPLIB)" $(ENGINE_OBJS)

clean_editor:
	@echo [Cleaning Editor] Removing $(EDITOR_DLL) and editor objects.
	@rm -f "$(EDITOR_DLL)" $(EDITOR_OBJS)

clean_game:
	@echo [Cleaning Game] Removing $(GAME_DLL) and game objects.
	@rm -f "$(GAME_DLL)" $(GAME_OBJS)

clean_all: clean clean_prefs
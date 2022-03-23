# Written by Alberto González Palomo <https://sentido-labs.com>
# Created: 2022-03-13 07:33, MIT license

# nanovg needs: premake4
# glew and glfw need: cmake, libgl-dev, glu-devel
# --location is to follow redirects which GitHub uses.
NANOVG_FETCH=curl --location 'https://github.com/memononen/nanovg/archive/5f65b43f7abf044a6afa60504382f52bc4325b92.tar.gz' | tar zxf -
NANOVG_DIR=lib/nanovg-5f65b43f7abf044a6afa60504382f52bc4325b92
GLEW_FETCH=curl --location 'https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.tgz' | tar zxf -
GLEW_DIR=lib/glew-2.2.0
GLFW_FETCH=curl --location 'https://github.com/glfw/glfw/archive/refs/tags/3.3.6.tar.gz' | tar zxf -
GLFW_DIR=lib/glfw-3.3.6

# If you prefer to fetch the current development version of e.g. nanovg:
#NANOVG_FECTH=git clone https://github.com/memononen/nanovg.git
#NANOVG_DIR=lib/nanovg

build/assets.o: src/assets.c $(CEDROCC)
	mkdir -p $$(dirname $@)
	$(CEDROCC) -c -o $@ $< $(CFLAGS) -O

LIBRARIES:=build/assets.o $(LIBRARIES) $(NANOVG_DIR)/build/libnanovg.a $(GLEW_DIR)/build/lib/libGLEW.a -ldl -lX11 -lpthread $(GLFW_DIR)/build/src/libglfw3.a -lGL -lGLU -lm
# “C99 compliance / user side #306” https://github.com/nanovg/nanovg/issues/306
CFLAGS:=$(CFLAGS) -I$(NANOVG_DIR)/src -DNANOVG_GLEW -I$(GLEW_DIR)/include -I$(GLFW_DIR)/include

$(NANOVG_DIR)/build/libnanovg.a: $(NANOVG_DIR)/build $(NANOVG_DIR)/src/*
	$(MAKE) -C $(NANOVG_DIR)/build config=release nanovg
$(GLEW_DIR)/build/lib/libGLEW.a: $(GLEW_DIR)/build $(GLEW_DIR)/src/*
	$(MAKE) -C $(GLEW_DIR)/build
$(GLFW_DIR)/build/src/libglfw3.a: $(GLFW_DIR)/build $(GLFW_DIR)/src/*
	$(MAKE) -C $(GLFW_DIR)/build

$(NANOVG_DIR)/src/*: $(NANOVG_DIR)
$(NANOVG_DIR)/build: $(NANOVG_DIR)
	@echo '============== Generating nanovg makefile =============='
	mkdir -p "$@" && cd $(NANOVG_DIR) && premake4 gmake

$(GLEW_DIR)/src/*: $(GLEW_DIR)
$(GLEW_DIR)/build: $(GLEW_DIR)
	@echo '============== Generating glew Makefile =============='
	mkdir -p "$@" && cd "$@" && cmake cmake

$(GLFW_DIR)/src/*: $(GLFW_DIR)
$(GLFW_DIR)/build: $(GLFW_DIR)
	@echo '============== Generating glfw Makefile =============='
	mkdir -p "$@" && cd "$@" && cmake ..

$(NANOVG_DIR):
	@echo '--------------------- Downloading nanovg ---------------------'
	cd "$@"/.. && $(NANOVG_FETCH)

$(GLEW_DIR):
	@echo '--------------------- Downloading GLEW ---------------------'
	cd "$@"/.. && $(GLEW_FETCH)

$(GLFW_DIR):
	@echo '--------------------- Downloading GLFW ---------------------'
	cd "$@"/.. && $(GLFW_FETCH)

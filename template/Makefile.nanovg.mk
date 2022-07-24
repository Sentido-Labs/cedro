# Written by Alberto González Palomo <https://sentido-labs.com>
# Created: 2022-03-13 07:33, MIT license

# nanovg needs: premake4
# glew and glfw need: cmake, libgl-dev, glu-devel
NANOVG_VERSION=616f18942c3e5864b2f3d615d56d901fcfed56e7
NANOVG_ARCHIVE=$(NANOVG_VERSION).tar.gz
NANOVG_CHECKSUM=8c3061dd419ea7e7eea8b1510dfb53caf35339cbf8a4b0b235b6e058b220e34d
NANOVG_FETCH=curl --location --output $(NANOVG_ARCHIVE) "https://github.com/memononen/nanovg/archive/$(NANOVG_ARCHIVE)" && echo "$(NANOVG_CHECKSUM)  $(NANOVG_ARCHIVE)" | sha256sum --check && tar zxf $(NANOVG_ARCHIVE)
NANOVG_DIR=lib/nanovg-$(NANOVG_VERSION)
GLEW_VERSION=2.2.0
GLEW_ARCHIVE=glew-$(GLEW_VERSION).tgz
GLEW_CHECKSUM=d4fc82893cfb00109578d0a1a2337fb8ca335b3ceccf97b97e5cc7f08e4353e1
GLEW_FETCH=curl --location --output $(GLEW_ARCHIVE) "https://github.com/nigels-com/glew/releases/download/glew-$(GLEW_VERSION)/$(GLEW_ARCHIVE)" && echo "$(GLEW_CHECKSUM)  $(GLEW_ARCHIVE)" | sha256sum --check && tar zxf $(GLEW_ARCHIVE)
GLEW_DIR=lib/glew-$(GLEW_VERSION)
GLFW_VERSION=3.3.6
GLFW_ARCHIVE=glfw-$(GLFW_VERSION).tar.gz
GLFW_CHECKSUM=ed07b90e334dcd39903e6288d90fa1ae0cf2d2119fec516cf743a0a404527c02
GLFW_FETCH=curl --location --output $(GLFW_ARCHIVE) "https://github.com/glfw/glfw/archive/refs/tags/$(GLFW_VERSION).tar.gz" && echo "$(GLFW_CHECKSUM)  $(GLFW_ARCHIVE)" | sha256sum --check && tar zxf $(GLFW_ARCHIVE)
GLFW_DIR=lib/glfw-$(GLFW_VERSION)

# If you prefer to fetch the current development version of e.g. nanovg:
#NANOVG_FECTH=git clone https://github.com/memononen/nanovg.git
#NANOVG_DIR=lib/nanovg

build/assets.o: src/assets.c logo.png $(CEDROCC)
	mkdir -p $$(dirname $@)
	$(CEDROCC) -c -o $@ $< $(CFLAGS) -O

LIBRARIES:=build/assets.o $(LIBRARIES) $(NANOVG_DIR)/build/libnanovg.a $(GLEW_DIR)/build/lib/libGLEW.a -ldl -lX11 -lpthread $(GLFW_DIR)/build/src/libglfw3.a -lGL -lGLU -lm
# “C99 compliance / user side #306” https://github.com/nanovg/nanovg/issues/306
CFLAGS:=$(CFLAGS) -I$(NANOVG_DIR)/src -DNANOVG_GLEW -I$(GLEW_DIR)/include -I$(GLFW_DIR)/include -std=c11
LDFLAGS:=-L$(GLEW_DIR)/build -L$(GLFW_DIR)/build

$(NANOVG_DIR)/build/libnanovg.a: $(NANOVG_DIR)/build $(NANOVG_DIR)/src/* $(GLEW_DIR)/build/lib/libGLEW.a $(GLFW_DIR)/build/src/libglfw3.a
	$(MAKE) -C $(NANOVG_DIR)/build config=release nanovg
$(GLEW_DIR)/build/lib/libGLEW.a: $(GLEW_DIR)/build $(GLEW_DIR)/src/*
	$(MAKE) -C $(GLEW_DIR)/build
$(GLFW_DIR)/build/src/libglfw3.a: $(GLFW_DIR)/build $(GLFW_DIR)/src/*
	$(MAKE) -C $(GLFW_DIR)/build

$(NANOVG_DIR)/src/*: $(NANOVG_DIR)
$(NANOVG_DIR)/build: $(NANOVG_DIR) $(GLEW_DIR)/build $(GLFW_DIR)/build
	@echo '============== Generating nanovg makefile =============='
# Replace pkg-config so that we do not depend on having glfw-devel installed.
	mkdir -p "$@" && cd $(NANOVG_DIR) && sed -i 's/"`pkg-config --libs glfw3`"/"-lglfw3"/' premake4.lua && premake4 gmake

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

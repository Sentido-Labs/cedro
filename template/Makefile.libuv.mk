# Written by Alberto González Palomo <https://sentido-labs.com>
# Created: 2022-03-12 07:21, MIT license

# --location is to follow redirects, because GitHub redirects this URL to
# https://codeload.github.com/libuv/libuv/tar.gz/refs/tags/v1.44.1
LIBUV_FETCH=curl --location 'https://github.com/libuv/libuv/archive/refs/tags/v1.44.1.tar.gz' | tar zxf -
LIBUV_DIR=lib/libuv-1.44.1

# If you prefer to fetch the current development version:
#LIBUV_FECTH=git clone https://github.com/libuv/libuv.git
#LIBUV_DIR=lib/libuv

LIBRARIES:=$(LIBRARIES) $(LIBUV_DIR)/.libs/libuv.a -lpthread -ldl
# “C99 compliance / user side #306” https://github.com/libuv/libuv/issues/306
CFLAGS:=$(CFLAGS) -I$(LIBUV_DIR)/include -D_XOPEN_SOURCE=600

$(LIBUV_DIR)/.libs/libuv.a: $(LIBUV_DIR)/.libs $(LIBUV_DIR)/src/*
	cd $(LIBUV_DIR); $(MAKE) $(MAKEFLAGS)

$(LIBUV_DIR)/src/*: $(LIBUV_DIR)
$(LIBUV_DIR)/.libs: $(LIBUV_DIR)
	cd "$@"/.. && sh ./autogen.sh && ./configure

$(LIBUV_DIR):
	@echo '--------------------- Downloading libuv ---------------------'
	cd "$@"/.. && $(LIBUV_FETCH)

# Written by Alberto González Palomo <https://sentido-labs.com>
# Created: 2022-03-12 07:21, MIT license

# --location is to follow redirects, because GitHub redirects this URL to
# https://codeload.github.com/libuv/libuv/tar.gz/refs/tags/v1.44.1
LIBUV_FETCH=curl --location 'https://github.com/libuv/libuv/archive/refs/tags/v1.44.1.tar.gz' | tar zxf -
LIBUV_DIR=lib/libuv-1.44.1

# If you prefer to fetch the current development version:
#LIBUV_FECTH=git clone https://github.com/libuv/libuv.git
#LIBUV_DIR=lib/libuv

# Example adapted from:
# http://docs.libuv.org/en/v1.x/guide/basics.html
#
# /* “pthread_rwlock_t undefined on unix platform”
#  * https://github.com/libuv/libuv/issues/1160
#  */
# #include <pthread.h>
# #include <uv.h>
#
# #pragma Cedro 1.0
#
# int
# main(int argc, char** argv)
# {
#     uv_loop_t *loop = uv_default_loop();
#     auto uv_loop_close(loop);
# 
#     uv_run(loop, UV_RUN_DEFAULT);
# 
#     return 0;
# }

LIBRARIES:=$(LIBRARIES) $(LIBUV_DIR)/.libs/libuv.a -lpthread -ldl
# “C99 compliance / user side #306” https://github.com/libuv/libuv/issues/306
CFLAGS:=$(CFLAGS) -I $(LIBUV_DIR)/include -D_XOPEN_SOURCE=600

$(LIBUV_DIR)/.libs/libuv.a: $(LIBUV_DIR)/src/*
	cd $(LIBUV_DIR); sh ./autogen.sh; ./configure; $(MAKE) $(MAKEFLAGS)

$(LIBUV_DIR)/src/*:
	@echo '--------------------- Downloading libuv ---------------------'
	cd lib; $(LIBUV_FETCH)

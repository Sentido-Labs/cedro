# Written by Alberto González Palomo <https://sentido-labs.com>
# Created: 2022-03-12 07:21, MIT license

LIBUV_VERSION=1.44.1
LIBUV_ARCHIVE=libuv-$(LIBUV_VERSION).tar.gz
LIBUV_CHECKSUM=e91614e6dc2dd0bfdd140ceace49438882206b7a6fb00b8750914e67a9ed6d6b
# --location is to follow redirects, because GitHub redirects this URL to
# https://codeload.github.com/libuv/libuv/tar.gz/refs/tags/v1.44.1
LIBUV_FETCH=curl --location --output $(LIBUV_ARCHIVE) "https://github.com/libuv/libuv/archive/refs/tags/v$(LIBUV_VERSION).tar.gz" && echo "$(LIBUV_CHECKSUM)  $(LIBUV_ARCHIVE)" | sha256sum --check && tar zxf $(LIBUV_ARCHIVE)
LIBUV_DIR=lib/libuv-$(LIBUV_VERSION)

# If you prefer to fetch the current development version:
#LIBUV_FECTH=git clone https://github.com/libuv/libuv.git
#LIBUV_DIR=lib/libuv

LIBRARIES:=$(LIBRARIES) $(LIBUV_DIR)/.libs/libuv.a -lpthread -ldl
# “C99 compliance / user side #306” https://github.com/libuv/libuv/issues/306
CFLAGS:=$(CFLAGS) -I$(LIBUV_DIR)/include -D_XOPEN_SOURCE=600
LDFLAGS:=$(LDFLAGS)

$(LIBUV_DIR)/.libs/libuv.a: $(LIBUV_DIR)/.libs $(LIBUV_DIR)/src/*
	cd $(LIBUV_DIR); $(MAKE) $(MAKEFLAGS)

$(LIBUV_DIR)/src/*: $(LIBUV_DIR)
$(LIBUV_DIR)/.libs: $(LIBUV_DIR)
	cd "$@"/.. && sh ./autogen.sh && ./configure

$(LIBUV_DIR):
	@echo '--------------------- Downloading libuv ---------------------'
	cd "$@"/.. && $(LIBUV_FETCH)

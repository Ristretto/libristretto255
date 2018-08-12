# Copyright (c) 2014-2018 Cryptography Research, Inc.
# Released under the MIT License.  See LICENSE.txt for license information.

UNAME := $(shell uname)
MACHINE := $(shell uname -m)

# Subdirectories for objects etc.
BUILD_OBJ  = build/obj
BUILD_LIB  = build/lib
BUILD_IBIN = build/obj/bin

# TODO: fix builds for non-x86_64 architectures
ARCHNAME = $(MACHINE)
ARCHDIR  = arch_$(ARCHNAME)

ifeq ($(UNAME),Darwin)
CC ?= clang
else
CC ?= gcc
endif
LD = $(CC)
ASM ?= $(CC)

WARNFLAGS = -pedantic -Wall -Wextra -Werror -Wunreachable-code \
	 -Wmissing-declarations -Wunused-function -Wno-overlength-strings $(EXWARN)

INCFLAGS  = -Iinclude -Iinclude/$(ARCHDIR)
LANGFLAGS = -std=c99 -fno-strict-aliasing
GENFLAGS  = -ffunction-sections -fdata-sections -fvisibility=hidden -fomit-frame-pointer -fPIC
OFLAGS   ?= -O2

MACOSX_VERSION_MIN ?= 10.9
ifeq ($(UNAME),Darwin)
GENFLAGS += -mmacosx-version-min=$(MACOSX_VERSION_MIN)
endif

TODAY = $(shell date "+%Y-%m-%d")

ARCHFLAGS ?= -march=native

ifeq ($(CC),clang)
WARNFLAGS_C += -Wgcc-compat
endif

ARCHFLAGS += $(XARCHFLAGS)
CFLAGS     = $(LANGFLAGS) $(WARNFLAGS) $(WARNFLAGS_C) $(INCFLAGS) $(OFLAGS) $(ARCHFLAGS) $(GENFLAGS) $(XCFLAGS)
LDFLAGS    = $(XLDFLAGS)
ASFLAGS    = $(ARCHFLAGS) $(XASFLAGS)

.PHONY: clean all lib
.PRECIOUS: src/%.c src/*/%.c include/%.h include/*/%.h $(BUILD_IBIN)/%

HEADERS= Makefile $(BUILD_OBJ)/timestamp

# components needed by all targets
COMPONENTS = $(BUILD_OBJ)/f_impl.o \
             $(BUILD_OBJ)/f_arithmetic.o \
             $(BUILD_OBJ)/f_generic.o \
             $(BUILD_OBJ)/ristretto.o \
             $(BUILD_OBJ)/scalar.o \
             $(BUILD_OBJ)/utils.o

# components needed by libristretto255.so
LIBCOMPONENTS = $(COMPONENTS) $(BUILD_OBJ)/elligator.o $(BUILD_OBJ)/ristretto_tables.o

# components needed by the ristretto_gen_tables binary
GENCOMPONENTS = $(COMPONENTS) $(BUILD_OBJ)/ristretto_gen_tables.o

all: lib

# Create all the build subdirectories
$(BUILD_OBJ)/timestamp:
	mkdir -p $(BUILD_OBJ) $(BUILD_LIB) $(BUILD_IBIN)
	touch $@

$(BUILD_OBJ)/f_impl.o: src/$(ARCHDIR)/f_impl.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_IBIN)/ristretto_gen_tables: $(GENCOMPONENTS)
	$(LD) $(LDFLAGS) -o $@ $^

src/ristretto_tables.c: $(BUILD_IBIN)/ristretto_gen_tables
	./$< > $@ || (rm $@; exit 1)

# The libristretto255 library
lib: $(BUILD_LIB)/libristretto255.so

$(BUILD_LIB)/libristretto255.so: $(BUILD_LIB)/libristretto255.so.1
	ln -sf `basename $^` $@

$(BUILD_LIB)/libristretto255.so.1: $(LIBCOMPONENTS)
	rm -f $@
ifeq ($(UNAME),Darwin)
	libtool -macosx_version_min $(MACOSX_VERSION_MIN) -dynamic -dead_strip -lc -x -o $@ \
		  $(LIBCOMPONENTS)
else ifeq ($(UNAME),SunOS)
	$(LD) $(LDFLAGS) -shared -Wl,-soname,`basename $@` -o $@ $(LIBCOMPONENTS)
	strip --discard-all $@
else
	$(LD) $(LDFLAGS) -shared -Wl,-soname,`basename $@` -Wl,--gc-sections -o $@ $(LIBCOMPONENTS)
	strip --discard-all $@
endif

$(BUILD_OBJ)/%.o: src/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -fr build

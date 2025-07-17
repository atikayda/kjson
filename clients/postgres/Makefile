EXTENSION = kjson
DATA = sql/kjson--1.0.0.sql sql/kjson--1.0.1.sql sql/kjson--1.0.0--1.0.1.sql
MODULE_big = kjson

# Source files - using native implementation
OBJS = src/kjson_io_native.o \
       src/kjson_parser.o \
       src/kjson_stringify.o \
       src/kjson_binary.o \
       src/kjson_ops.o \
       src/kjson_casts.o \
       src/kjson_utils.o \
       src/kjson_builders.o \
       src/kjson_path_ops.o \
       src/kjson_containment.o \
       src/kjson_cmp.o \
       src/kjson_gin.o \
       src/kjson_temporal.o \
       src/kduration.o \
       src/kinstant.o \
       src/kinstant_kduration_ops.o

# Compiler flags
PG_CPPFLAGS = -I$(srcdir)/src -I$(srcdir)/../c
SHLIB_LINK = -lm

# Test files
REGRESS = kjson_basic kjson_extended kjson_casts kjson_ops

# PostgreSQL build infrastructure
PG_CONFIG ?= /opt/homebrew/bin/pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
SRC := $(dir $(lastword $(MAKEFILE_LIST))) # directory where this Makefile is placed
vpath %.c $(SRC)
vpath Makefile $(SRC)

CFLAGS+=-O3 -std=gnu99
LDFLAGS+=-lsensors -lcpufreq

all: frekmand

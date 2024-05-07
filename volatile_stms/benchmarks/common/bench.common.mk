include ../../../common/common.inc.mk

ALGS:=norec tml
ROOT_DIR:=$(abspath ..)
STM_DIR:=$(abspath ../..)
ALGS_DIR:=$(STM_DIR)/algs

vpath %.c $(ROOT_DIR)/common

# LIBS+=-L$(STM_DIR)/lib
# LIBS+=-lrpmalloc

INCS+=-I$(STM_DIR)/include
INCS+=-I$(ROOT_DIR)/common
INCS+=$(addprefix -I$(ALGS_DIR)/,$(ALGS))
INCS+=-I$(ALGS_DIR)/common
LIBS+=-lpthread

.PHONY: build-algs
build-algs:
	$(MAKE) -C $(ALGS_DIR)
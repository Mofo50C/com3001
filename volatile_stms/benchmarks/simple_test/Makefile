include ../common/bench.common.mk

SRCS+=main.c bench_utils.c
include ../common/Makefile.common

build: build-algs

prepare-%:
	$(eval CFLAGS+=-D$(call toupper,$*))

.SECONDEXPANSION:
DEPS:=$$(wildcard $(ALGS_DIR)/$$*/out/*.o) $$(wildcard $(ROOT_DIR)/common/out/*.o)

$(addprefix test-,$(ALGS)): test-%: prepare-% $(DEPS) $(OBJS)
	$(CC) -o $@ $(filter-out prepare-$*,$^) $(LIBS)

clean::
	rm -f test-*
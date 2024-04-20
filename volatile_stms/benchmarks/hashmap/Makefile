include ../common/bench.common.mk

SRCS+=test.c tm_hashmap.c
include ../common/Makefile.common

build: build-algs
	$(MAKE) -C $(COMMON_DIR)/lib

prepare-%:
	$(eval CFLAGS+=-D$(call toupper,$*))

.SECONDEXPANSION:
$(addprefix test-,$(ALGS)): test-%: prepare-% $$(wildcard $(ALGS_DIR)/$$*/out/*.o) $$(wildcard $(COMMON_DIR)/lib/out/*.o) $(OBJS)
	$(CC) -o $@ $(filter-out prepare-$*,$^) $(LIBS)


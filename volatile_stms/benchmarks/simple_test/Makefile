include ../common/bench.common.mk

SRCS+=test.c
include ../common/Makefile.common

build: build-algs

prepare-%:
	$(eval CFLAGS+=-D$(call toupper,$*))

.SECONDEXPANSION:
$(addprefix test-,$(ALGS)): test-%: prepare-% $$(wildcard $(ALGS_DIR)/$$*/out/*.o) $(OBJS)
	$(CC) -o $@ $(filter-out prepare-$*,$^) $(LIBS)

clean::
	rm -f test-*
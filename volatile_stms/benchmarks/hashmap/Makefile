include ../common/bench.common.mk

INCS+=-I$(abspath ..)/hashmap

SRCS+=map.c tm_hashmap.c
include ../common/Makefile.common


build: build-algs build-bench-utils
	$(MAKE) -C $(COMMON_DIR)/lib

prepare-%:
	$(eval CFLAGS+=-D$(call toupper,$*))

PROGS=test bench transfer

.SECONDEXPANSION:
DEPS:=$$(wildcard $(ALGS_DIR)/$$*/out/*.o) $$(wildcard $(COMMON_DIR)/lib/out/*.o)

define make-prog-target
$$(addprefix $1-,$$(ALGS)): $1-%: prepare-% $$(DEPS) $$(OBJS) $$(OUT_DIR)/$1.o
	$$(CC) -o $$@ $$(filter-out prepare-$$*,$$^) $$(LIBS)

clean::
	rm -f $1-*
endef

$(foreach element,$(PROGS),$(eval $(call make-prog-target,$(element))))

clean::
	rm -f log.txt
CC:=gcc -std=c17
CFLAGS=-c -Wall

ROOT_DIR:=$(abspath ../..)
BENCH_DIR:=$(abspath ..)
ALGS_DIR:=$(ROOT_DIR)/algs
vpath %.c $(ROOT_DIR)/lib

INCS=-I$(ROOT_DIR)/lib
INCS+=-I$(BENCH_DIR)/common
INCS+=-I$(ALGS_DIR)/common
LIBS:=-lpmemobj -lpthread -lpmem
OUT_DIR:=./out

_OBJS:=test.o tx_hashmap.o xxhash.o
OBJS=$(addprefix $(OUT_DIR)/,$(_OBJS))
ALGS:=norec tml txpmdk

UC=$(shell echo '$1' | tr '[:lower:]' '[:upper:]')

build: $(ALGS)

$(ALGS):
	$(MAKE) -C $(ALGS_DIR)/$@

prepare-%:
	$(eval STM_ALG:=$(call UC,$*))
	$(eval CFLAGS+=-D$(STM_ALG))
	$(eval INCS+=-I$(ALGS_DIR)/$*)

.SECONDEXPANSION:
test-%: prepare-% $$(wildcard $(ALGS_DIR)/$$*/out/*.o) $(OBJS)
	$(CC) -o $@ $(filter-out prepare-$*,$^) $(LIBS)

$(OUT_DIR)/%.o: %.c | $(OUT_DIR)
	$(CC) $(CFLAGS) $(LIBS) $(INCS) $< -o $@

$(OUT_DIR):
	mkdir $(OUT_DIR)

.PHONY: clean clean-build
clean-build:
	rm -rf $(addsuffix /out,$(addprefix $(ALGS_DIR)/,$(ALGS)))

clean:
	rm -rf $(OUT_DIR)
	rm -f test-*
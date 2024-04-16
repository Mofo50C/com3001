CC:=gcc -std=c17
CFLAGS:=-c -Wall

ROOT_DIR:=$(abspath ../..)
ALGS_DIR:=$(abspath ..)

DEPS=-I$(ROOT_DIR)/lib
DEPS+=-I$(ALGS_DIR)/common
DEPS+=-I$(CURDIR)
LIBS:=-lpmemobj -lpmem
OUT_DIR:=./out

_OBJS=norec.o norec_util.o
OBJS=$(addprefix $(OUT_DIR)/,$(_OBJS))

.PHONY: all
all: $(OBJS)

$(OUT_DIR)/%.o: %.c | $(OUT_DIR)
	$(CC) $(CFLAGS) $(LIBS) $(DEPS) $< -o $@

$(OUT_DIR):
	mkdir $(OUT_DIR)

.PHONY: clean
clean:
	rm -rf $(OUT_DIR)
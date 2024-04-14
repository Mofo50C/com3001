vpath %.c ../../lib

CC=gcc -std=c17
CFLAGS=-c
DEPS=-I../common -I../../common -I../../lib
LIBS=-lpmemobj -lpthread
OUT_DIR=./out/

_OBJS=norec.o hashmap.o vector.o fnv.o
OBJS=$(addprefix $(OUT_DIR),$(_OBJS))
TEST_OBJS=$(addprefix $(OUT_DIR),norec_test.o)

test: $(OBJS) $(OUT_DIR)$
	$(CC) -o $(OUT_DIR)$@ $^ $(DEPS) $(LIBS)

$(OUT_DIR)%.o: %.c | $(OUT_DIR)
	$(CC) $(CFLAGS) $(LIBS) $(DEPS) $< -o $@

$(OUT_DIR):
	mkdir $(OUT_DIR)

.PHONY: clean
clean:
	rm -r $(OUT_DIR)
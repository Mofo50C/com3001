# CC=gcc -std=gnu17 -fsanitize=address -static-libasan
# CFLAGS=-c -Wall -fsanitize=address
CC=gcc -std=c17
CFLAGS=-c -Wall -ggdb3 -O0

COMMON_DIR:=$(abspath $(dir $(lastword $(MAKEFILE_LIST))))
INCS+=-I$(COMMON_DIR)/include
INCS+=-I$(COMMON_DIR)/lib

OUT_DIR:=./out

toupper=$(shell echo '$1' | tr '[:lower:]' '[:upper:]')
tolower=$(shell echo '$1' | tr '[:upper:]' '[:lower:]')

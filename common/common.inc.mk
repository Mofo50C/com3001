CC=gcc -std=c17
CFLAGS=-c -Wall -Wno-unused-label -ggdb3 -O0

COMMON_DIR:=$(abspath $(dir $(lastword $(MAKEFILE_LIST))))
INCS+=-I$(COMMON_DIR)/include
INCS+=-I$(COMMON_DIR)/lib

OUT_DIR:=./out

toupper=$(shell echo '$1' | tr '[:lower:]' '[:upper:]')
tolower=$(shell echo '$1' | tr '[:upper:]' '[:lower:]')

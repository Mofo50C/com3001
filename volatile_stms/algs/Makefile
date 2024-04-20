include ../../common/common.inc.mk

ALGS:=norec tml
ROOT_DIR:=$(abspath ..)
ALGS_DIR:=$(ROOT_DIR)/algs

.PHONY: all
all: $(ALGS)

$(ALGS): %: build-%

build-%:
	$(MAKE) -C $(ALGS_DIR)/$*


.PHONY: clean
clean:
	rm -rf $(addsuffix /out,$(addprefix $(ALGS_DIR)/,$(ALGS)))

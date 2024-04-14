#ifndef NOREC_TYPES_H
#define NOREC_TYPES_H 1

#include <stdlib.h>

enum tx_action_type {
	ACTION_WRITE,
	ACTION_FREE
};

struct tx_act {
	size_t size;
	void *pdirect;
	void *pval;
	enum tx_action_type type;
};

enum tx_set_entry_type {
	VALUE_PTR,
	VALUE_ACTS
};

#endif
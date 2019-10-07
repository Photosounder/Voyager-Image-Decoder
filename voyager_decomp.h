#include <rouziclib/rouziclib.h>

#define RECORD_BYTES        836

typedef struct leaf
{
	struct leaf *right;
	short int dn;
	struct leaf *left;
} voy_node_t;

typedef struct
{
	voy_node_t *tree;
	buffer_t inbuf;
	size_t inpos;
} voy_image_t;

extern uint8_t *voyager_decompress_buffer_to_array(buffer_t inbuf);

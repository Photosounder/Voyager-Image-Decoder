#include "voyager_decomp.h"
#include <rouziclib/rouziclib.c>

_Thread_local alloc_list_t voy_alloc_list={0};

int voy_read_var(voy_image_t *s, char *ibuf)	// read variable length records from input file
{
	int length, lim_len;

	length = read_LE16(&s->inbuf.buf[s->inpos], &s->inpos);			// read the length of the record

	lim_len = MINN(MINN(length, RECORD_BYTES), s->inbuf.len-s->inpos);
	memcpy(ibuf, &s->inbuf.buf[s->inpos], lim_len);				// copy the record data
	ibuf[lim_len] = '\0';
	s->inpos += length + (length & 1);					// advance the reading position

	return lim_len;
}

void voy_skip_labels(voy_image_t *s)		// skip label records
{
	int length, i=0;
	char ibuf[2048];

	do
	{
		length = voy_read_var(s, ibuf);				// read each record

		//fprintf_rl(stdout, "Label #%d: %s\n", i, ibuf);
		i++;

		if (strcmp_len2(ibuf, "END")==0 && length == 3)		// look for the END record
			break;
	}
	while (length > 0);
}

void voy_dcmprs(char *ibuf, char *obuf, int *nin, int *nout, voy_node_t *root)	// Decompress a record
{
	voy_node_t *ptr = root;		// pointer to position in s->tree
	unsigned char test;		// test byte for bit set
	unsigned char idn;		// input compressed byte
	char odn;			// last dn value decompressed
	char *ilim = ibuf + *nin;	// end of compressed bytes
	char *olim = obuf + *nout;	// end of output buffer

	/**************************************************************************
	  Check for valid input values for nin, nout and make initial assignments.
	 ***************************************************************************/

	if (ilim > ibuf && olim > obuf)
		odn = *obuf++ = *ibuf++;
	else
	{
		fprintf_rl(stderr, "Invalid byte count in voy_dcmprs()\n");
		return;
	}

	/**************************************************************************
	  Decompress the input buffer.  Assign the first byte to the working
	  variable, idn.  An arithmetic and (&) is performed using the variable
	  'test' that is bit shifted to the right.  If the result is 0, then
	  go to right else go to left.
	 ***************************************************************************/

	for (idn=(*ibuf) ; ibuf < ilim  ; idn =(*++ibuf))
	{
		for (test=0x80 ; test ; test >>= 1)
		{
			ptr = (test & idn) ? ptr->left : ptr->right;

			if (ptr->dn != -1)
			{
				if (obuf >= olim) return;
				odn -= ptr->dn + 256;
				*obuf++ = odn;
				ptr = root;
			}
		}
	}
}

voy_node_t *voy_new_node(short int value)
{
	voy_node_t *temp;         // Pointer to the memory block

	// Allocate the memory and intialize the fields.
	temp = malloc_list(sizeof(voy_node_t), &voy_alloc_list);

	temp->right = NULL;
	temp->dn = value;
	temp->left = NULL;

	return temp;
}

void voy_sort_freq(int *freq_list, voy_node_t **node_list, int num_freq)
{
	// Local Variables
	int *i;			// primary pointer into freq_list
	int *j;			// secondary pointer into freq_list

	voy_node_t **k;		// primary pointer to node_list
	voy_node_t **l;		// secondary pointer into node_list

	int temp1;		// temporary storage for freq_list
	voy_node_t *temp2;	// temporary storage for node_list

	int cnt;		// count of list elements

	/************************************************************************
	  Save the current element - starting with the second - in temporary
	  storage.  Compare with all elements in first part of list moving
	  each up one element until the element is larger.  Insert current
	  element at this point in list.
	 *************************************************************************/

	if (num_freq <= 0)
		return;		// If no elements or invalid, return

	for (i=freq_list, k=node_list, cnt=num_freq ; --cnt ; *j=temp1, *l=temp2)
	{
		temp1 = *(++i);
		temp2 = *(++k);

		for (j = i, l = k ;  *(j-1) > temp1 ; )
		{
			*j = *(j-1);
			*l = *(l-1);
			j--;
			l--;
			if ( j <= freq_list)
				break;
		}

	}
}

voy_node_t *voy_huff_tree(int *hist)
{
	// Local variables used
	int freq_list[512];		// Histogram frequency list
	voy_node_t **node_list;		// DN pointer array list

	int *fp;			// Frequency list pointer
	voy_node_t **np;		// Node list pointer

	int num_freq;			// Number non-zero frequencies in histogram
	int sum;			// Sum of all frequencies

	short int num_nodes;		// Counter for DN initialization
	short int cnt;			// Miscellaneous counter

	short int znull = -1;		// Null node value

	voy_node_t *temp;		// Temporary node pointer

	/***************************************************************************
	  Allocate the array of nodes from memory and initialize these with numbers
	  corresponding with the frequency list.  There are only 511 possible
	  permutations of first difference histograms.  There are 512 allocated
	  here to adhere to the FORTRAN version.
	 ****************************************************************************/

	fp = freq_list;
	node_list = malloc_list(512*sizeof(temp), &voy_alloc_list);
	np = node_list;

	for (num_nodes=1, cnt=512 ; cnt-- ; num_nodes++)
	{
		/**************************************************************************
		  The following code has been added to standardize the VAX byte order
		  for the "int" type.  This code is intended to make the routine
		  as machine independant as possible.
		 ***************************************************************************/
		unsigned char *cp = (unsigned char *) hist++;
		unsigned int j=0;
		short int i;
		for (i=4 ; --i >= 0 ; j = (j << 8) | *(cp+i));

		// Now make the assignment
		*fp++ = j;
		temp = voy_new_node(num_nodes);
		*np++ = temp;
	}

	(*--fp) = 0;		// Ensure the last element is zeroed out.

	/***************************************************************************
	  Now, sort the frequency list and eliminate all frequencies of zero.
	 ****************************************************************************/

	num_freq = 512;
	voy_sort_freq(freq_list, node_list, num_freq);

	fp = freq_list;
	np = node_list;

	for (num_freq=512 ; (*fp) == 0 && (num_freq) ; fp++, np++, num_freq--);


	/***************************************************************************
	  Now create the s->tree.  Note that if there is only one difference value,
	  it is returned as the root.  On each interation, a new node is created
	  and the least frequently occurring difference is assigned to the right
	  pointer and the next least frequency to the left pointer.  The node
	  assigned to the left pointer now becomes the combination of the two
	  nodes and it's frequency is the sum of the two combining nodes.
	 ****************************************************************************/

	for (temp=(*np) ; (num_freq--) > 1 ; )
	{
		temp = voy_new_node(znull);
		temp->right = (*np++);
		temp->left = (*np);
		*np = temp;
		*(fp+1) = *(fp+1) + *fp;
		*fp++ = 0;
		voy_sort_freq(fp, np, num_freq);
	}

	return temp;
}

uint8_t *voyager_decompress_buffer_to_array(buffer_t inbuf)
{
	voy_image_t ss={0}, *s;
	int hist[RECORD_BYTES*4];
	int out_bytes = RECORD_BYTES;
	char ibuf[2048];
	uint8_t *data = calloc(800*800+RECORD_BYTES, sizeof(uint8_t));

	s = &ss;
	s->inbuf = inbuf;

	// Skip labels
	voy_skip_labels(s);

	// Load image histogram
	voy_read_var(s, hist);
	voy_read_var(s, (char *)hist+RECORD_BYTES);

	// Load encoding histogram
	voy_read_var(s, hist);
	voy_read_var(s, (char *)hist+RECORD_BYTES);
	voy_read_var(s, (char *)hist+RECORD_BYTES*2);

	// Load engineering summary
	voy_read_var(s, ibuf);

	// Initialise the decompression
	s->tree = voy_huff_tree(hist);

	// Decompress the image
	int length, line=0;
	do
	{
		length = voy_read_var(s, ibuf);						// read one record
		if (length <= 0)
			break;

		voy_dcmprs(ibuf, &data[line*800], &length, &out_bytes, s->tree);	// decompress one record into one line of pixels
		line++;
	}
	while (line < 800);

	free_alloc_list(&voy_alloc_list);	// free the tree

	return data;
}

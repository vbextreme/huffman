#include <notstd/opt.h>
#include <notstd/str.h>
#include <notstd/phq.h>
#include <fcntl.h>
#include <inutility.h>

#include <limits.h>
#include <huffman.h>

#define HUFFMAN_FORMAT 0x0150

typedef struct node{
	struct node* child[2];
	unsigned     repeat;
	unsigned     index;
	uint8_t      value;
}node_s;

typedef struct bitmap{
	uint8_t bits[32];
	uint8_t count;
	uint8_t numbyte;
}bitmap_s;

__private void repetition(const uint8_t* data, const unsigned len, unsigned repeat[256]){
	memset(repeat, 0, 256*sizeof(unsigned));
	for( unsigned i = 0; i < len; ++i ){
		++repeat[data[i]];
	}
}

__private int pcmp(const void* a, const void* b){
	return ((const node_s*)a)->repeat > ((const node_s*)b)->repeat;
}

__private unsigned iget(void* a){
	return ((node_s*)a)->index;
}

__private void iset(void* a, unsigned i){
	((node_s*)a)->index = i;
}

__private node_s* htree(node_s nodes[1024], unsigned repeat[256]){
	unsigned inode = 0;
	//creating priority heap queue
	phq_s phq;
	phq_ctor(&phq, 512, pcmp, iget, iset);
	for( unsigned i = 0; i < 256; ++i ){
		if( repeat[i] ){
			node_s* node = &nodes[inode++];
			node->child[0] = NULL;
			node->child[1] = NULL;
			node->index    = 0;
			node->repeat   = repeat[i];
			node->value    = i;
			phq_push(&phq, node);
		}
	}
	//There can be only one.
	node_s* root;
	node_s* b;
	while( (root=phq_pop(&phq)) && (b=phq_pop(&phq)) ){
		iassert(inode<1024);
		node_s* n = &nodes[inode++];
		n->value      = 0;
		n->index      = 0;
		n->repeat     = root->repeat + b->repeat;
		n->child[0]   = root;
		n->child[1]   = b;
		phq_push(&phq, n);
	}
	phq_dtor(&phq);
	return root;
}

__private void htable(node_s* node, bitmap_s prefix[256], bitmap_s bm){
	if( !node ) return;
	if( !node->child[0] ){
		for( unsigned i = 0; i < bm.count; ++i ){
			prefix[node->value].bits[i]  = bm.bits[i];
		}
		prefix[node->value].count = bm.count;
		prefix[node->value].numbyte = ROUND_UP(bm.count, 8) / 8;
	}
	else{
		++bm.count;
		htable(node->child[0], prefix, bm);
		m_bit_set(bm.bits, bm.count-1, 1);
		htable(node->child[1], prefix, bm);
	}
}

__private unsigned long w16(uint8_t* out, unsigned long pos, unsigned value){
	out[pos++] = (value >> 8) & 0xFF;
	out[pos++] = value & 0xFF;
	return pos;
}

__private unsigned long w32(uint8_t* out, unsigned long pos, unsigned value){
	out[pos++] = (value >> 24) & 0xFF;
	out[pos++] = (value >> 16) & 0xFF;
	out[pos++] = (value >> 8) & 0xFF;
	out[pos++] = value & 0xFF;
	return pos;
}

void* huffman_compress(const void* data, const unsigned len){
	const uint8_t* d = data;
	const unsigned maxoutput = len+512;
	unsigned repeat[256];
	node_s   nodes[1024];
	bitmap_s table[256];
	uint8_t* out = MANY(uint8_t, maxoutput);
	m_zero(out);
	
	repetition(data, len, repeat);
	node_s* tree = htree(nodes, repeat);
	htable(tree, table, (bitmap_s){{0},0,0});
	
	unsigned long pos = 0;
	pos = w16(out, pos, HUFFMAN_FORMAT);
	pos = w32(out, pos, len);
	const unsigned linkoutsize = pos;
	pos += 4;
	const unsigned linkused = pos++;
	uint8_t used = 0;
	for( unsigned i = 0; i < 256; ++i ){
		if( repeat[i] ){
			++used;
			out[pos++] = i;
			pos = w32(out, pos, repeat[i]);
		}
	}
	out[linkused] = used;
	
	pos <<= 3;
	for( unsigned p = 0; p < len; ++p ){
		const uint8_t   inch      = d[p];
		const unsigned count      = table[inch].count;
		const uint8_t* const bits = table[inch].bits;
		iassert(table[inch].count);
		unsigned kp = pos >> 3;
		if( kp >= maxoutput ){
			errno = ENOBUFS;
			m_free(out);
			return NULL;
		}
		if( !count ){
			errno = ESRCH;
			m_free(out);
			return NULL;;
		}
		if( pos & 0x7 ){
			for( unsigned i = 0; i < count; ++i ){
				m_bit_set0(out, pos++, m_bit_get(bits, i));
			}
		}
		else{
			const unsigned np = table[inch].numbyte;
			for( unsigned i = 0; i < np; ++i ){
				out[kp++] = bits[i];
			}
			pos += count;
		}
	}
	
	w32(out, linkoutsize, pos);
	m_header(out)->len = ROUND_UP(pos,8)>>3;
	m_fit(out);
	return out;
}

__private uint16_t r16(const uint8_t* inp, unsigned long* pos){
	uint16_t ret = inp[(*pos)++];
	ret <<= 8;
	ret |=  inp[(*pos)++];
	return ret;
}

__private uint32_t r32(const uint8_t* inp, unsigned long* pos){
	uint32_t ret = inp[(*pos)++];
	ret <<= 8;
	ret |=  inp[(*pos)++];
	ret <<= 8;
	ret |=  inp[(*pos)++];
	ret <<= 8;
	ret |=  inp[(*pos)++];
	return ret;
}

void* huffman_decompress(const void* data, unsigned len){
	uint8_t* out = NULL;
	const uint8_t* inp = data;
	unsigned long pos = 0;
	if( len < 17 ) goto ERR_ISQ;
	if( r16(inp, &pos) != HUFFMAN_FORMAT ){
		errno = ENOEXEC;
		return NULL;
	}
	const unsigned orgBytes = r32(inp, &pos);
	const unsigned bitSize  = r32(inp, &pos);

	unsigned repeat[256] = {0};
	const uint8_t used = inp[pos++];
	
	if( pos+used*5 >= len ) goto ERR_ISQ;
	for( unsigned i = 0; i < used; ++i ){
		const uint8_t index = inp[pos++];
		repeat[index] = r32(inp, &pos);
	}
	
	node_s nodes[1024];
	node_s* tree = htree(nodes, repeat);
	
	pos <<= 3;
	out = MANY(uint8_t, orgBytes);
	unsigned safeinc = 0;
	while( pos < bitSize ){
		node_s* node = tree;
		while( node->child[0] && pos < bitSize ){
			const uint8_t b = m_bit_get(inp, pos++);
			node = node->child[b];
		}
		if( node->child[0] ) goto ERR_ISQ;
		if( safeinc >= orgBytes ) goto ERR_ISQ; 
		out[safeinc++] = node->value;
	}
	
	m_header(out)->len = safeinc;
	return out;
ERR_ISQ:
	errno = EILSEQ;
	m_free(out);
	return NULL;
}



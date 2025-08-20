#include <notstd/str.h>
#include <notstd/phq.h>
#include <fcntl.h>
#include <inutility.h>

#include <limits.h>
#include <huffman.h>


#define ANCHOR_MAX 4096
#define MULTI_MAX  4

typedef struct node{
	struct node* child[2];
	size_t       repeat;
	unsigned     index;
	uint8_t      value;
}node_s;

typedef struct bitmap{
	uint8_t bits[32];
	uint8_t count;
	uint8_t numbyte;
}bitmap_s;

#undef __AVX__
#ifdef __AVX__
#include <immintrin.h>
#define AVX_BYTE 32
__private void repetition(const uint8_t* data, const size_t len, size_t repeat[256]){
	memset(repeat, 0, 256*sizeof(unsigned));
	size_t i = 0;
	for(; i + AVX_BYTE < len; i += AVX_BYTE ){
		__m256i v = _mm256_loadu_si256((const __m256i*)&data[i]);
		alignas(AVX_BYTE) uint8_t tmp[AVX_BYTE];
		_mm256_store_si256((__m256i*)tmp, v);
		for( unsigned j = 0; j < AVX_BYTE; ++j ){
			++repeat[tmp[j]];
		}
	}
	
	for(; i < len; ++i ){
		++repeat[data[i]];
	}
}
#else
__private void repetition(const uint8_t* data, const size_t len, size_t repeat[256]){
	memset(repeat, 0, 256*sizeof(unsigned));
	for( size_t i = 0; i < len; ++i ){
		iassert( repeat[data[i]] < UINT32_MAX );
		++repeat[data[i]];
	}
}
#endif

__private int pcmp(const void* a, const void* b){
	return ((const node_s*)a)->repeat > ((const node_s*)b)->repeat;
}

__private unsigned iget(void* a){
	return ((node_s*)a)->index;
}

__private void iset(void* a, unsigned i){
	((node_s*)a)->index = i;
}

__private node_s* htree(node_s nodes[1024], size_t repeat[256]){
	unsigned inode = 0;
	//creating priority heap queue
	phq_s phq;
	phq_ctor(&phq, 1024, pcmp, iget, iset);
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
		//dbg_info("r:%u b:%u n:%u", root->repeat, b->repeat, root->repeat+b->repeat);
		node_s* n = &nodes[inode++];
		n->value      = 0;
		n->index      = 0;
		n->repeat     = root->repeat + b->repeat;
		n->child[0]   = root;
		n->child[1]   = b;
		phq_push(&phq, n);
	}
	phq_dtor(&phq);
	iassert(root);
	return root;
}

__private void htable(node_s* node, bitmap_s prefix[256], bitmap_s bm){
	if( !node ) return;
	if( !node->child[0] ){
		for( unsigned i = 0; i < bm.count; ++i ){
			prefix[node->value].bits[i]  = bm.bits[i];
		}
		prefix[node->value].count = bm.count;
		prefix[node->value].numbyte = ROUND_UP(bm.count, 8) >> 3;
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

__private unsigned long w64(uint8_t* out, unsigned long pos, uint64_t value){
	out[pos++] = (value >> 56) & 0xFF;
	out[pos++] = (value >> 48) & 0xFF;
	out[pos++] = (value >> 40) & 0xFF;
	out[pos++] = (value >> 32) & 0xFF;
	out[pos++] = (value >> 24) & 0xFF;
	out[pos++] = (value >> 16) & 0xFF;
	out[pos++] = (value >> 8) & 0xFF;
	out[pos++] = value & 0xFF;
	return pos;
}

void* huffman_compress(const void* data, const size_t len){
	const uint8_t* d = data;
	const size_t maxoutput = len*2+512;
	size_t   repeat[256];
	node_s   nodes[1024];
	bitmap_s table[256];
	
	uint8_t* out = MANY(uint8_t, maxoutput);
	m_zero(out);
	
	dbg_info("inp:%lu maxout: %lu", len, maxoutput);
	
	repetition(data, len, repeat);
	node_s* tree = htree(nodes, repeat);
	htable(tree, table, (bitmap_s){{0},0,0});
	
	unsigned long pos = 0;
	pos = w16(out, pos, HUFFMAN_FORMAT);
	pos = w64(out, pos, len);
	dbg_info("org:%lu", len);
	const unsigned linkoutsize = pos;
	pos += sizeof len;
	const unsigned linkused = pos;
	uint16_t used = 0;
	pos += sizeof used;
	for( unsigned i = 0; i < 256; ++i ){
		if( repeat[i] ){
			dbg_info("[%u] %lu", i, repeat[i]);
			++used;
			out[pos++] = i;
			pos = w64(out, pos, repeat[i]);
		}
	}
	
	w16(out, linkused, used);
	dbg_info("used: %u", used);
	if( !used ){
		dbg_error("no used elements");
		errno = EILSEQ;
		m_free(out);
		return NULL;
	}
	
	pos <<= 3;
	//unsigned kp = pos >> 3;
	for( size_t p = 0; p < len; ++p ){
		//dbg_info("inch: [%u](%u)", p,d[p]);
		const uint8_t   inch      = d[p];
		const unsigned  count     = table[inch].count;
		const unsigned  np        = table[inch].numbyte;
		const uint8_t* const bits = table[inch].bits;
		size_t kp = pos >> 3;
		if( kp >= maxoutput ){
			dbg_error("required more size");
			errno = ENOBUFS;
			m_free(out);
			return NULL;
		}
		if( !count ){
			errno = ESRCH;
			m_free(out);
			return NULL;;
		}
		
		const uint8_t shift = pos & 0x7;
		if( shift ){
			const uint8_t ishift = 8-shift;
			for( unsigned i = 0; i < np; ++i ){
				out[kp++] |= bits[i] >> shift;
				out[kp]   |= bits[i] << ishift;
			}
		}
		else{
			for( unsigned i = 0; i < np; ++i ){
				out[kp++] = bits[i];
			}
		}
		pos += count;
/*
		if( pos & 0x7 ){
			for( unsigned i = 0; i < count; ++i ){
				m_bit_set0(out, pos++, m_bit_get(bits, i));
			}
		}
		else{
			//dbg_info("achor %u", kp);
			const unsigned np = table[inch].numbyte;
			for( unsigned i = 0; i < np; ++i ){
				out[kp++] = bits[i];
			}
			pos += count;
		}
*/
	}
	w64(out, linkoutsize, pos);
	dbg_info("bits: %lu", pos);
	
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

__private uint64_t r64(const uint8_t* inp, unsigned long* pos){
	uint64_t ret = inp[(*pos)++];
	ret <<= 8;
	ret |=  inp[(*pos)++];
	ret <<= 8;
	ret |=  inp[(*pos)++];
	ret <<= 8;
	ret |=  inp[(*pos)++];
	ret <<= 8;
	ret |=  inp[(*pos)++];
	ret <<= 8;
	ret |=  inp[(*pos)++];
	ret <<= 8;
	ret |=  inp[(*pos)++];
	ret <<= 8;
	ret |=  inp[(*pos)++];
	return ret;
}

void* huffman_decompress(const void* data, const size_t len){
	dbg_info("");
	uint8_t* out = NULL;
	const uint8_t* inp = data;
	size_t pos = 0;
	
	if( len < 17 ) goto ERR_ISQ;
	uint16_t format = r16(inp, &pos);
	if( format != HUFFMAN_FORMAT ){
		errno = ENOEXEC;
		return NULL;
	}
	const size_t orgBytes = r64(inp, &pos);
	const size_t bitSize  = r64(inp, &pos);
	dbg_info("org: %lu bit: %lu", orgBytes, bitSize);
	
	size_t repeat[256] = {0};
	const uint16_t used = r16(inp, &pos);
	dbg_info("used: %u", used);
	if( !used ) goto ERR_ISQ;
	
	if( pos+used*sizeof pos >= len ) goto ERR_ISQ;
	for( unsigned i = 0; i < used; ++i ){
		const uint8_t index = inp[pos++];
		repeat[index] = r64(inp, &pos);
	}
	
	node_s nodes[1024];
	node_s* tree = htree(nodes, repeat);
	
	out = MANY(uint8_t, orgBytes);
	size_t safeinc = 0;
	pos <<= 3;
	dbg_info("start");
	while( pos < bitSize ){
		//dbg_info("%lu/%u", pos, bitSize);
		node_s* node = tree;
		while( node->child[0] && pos < bitSize ){
			//dbg_info("bitget");
			iassert(node->child[1]);
			const uint8_t b = m_bit_get(inp, pos++);
			iassert( b <= 1 );
			//dbg_info("enter node");
			node = node->child[b];
		}
		if( node->child[0] ) goto ERR_ISQ;
		if( safeinc >= orgBytes ) goto ERR_ISQ; 
		//dbg_info("store");
		out[safeinc++] = node->value;
	}
	dbg_info("end");
	iassert(orgBytes == safeinc);
	m_header(out)->len = safeinc;
	return out;
ERR_ISQ:
	errno = EILSEQ;
	m_free(out);
	return NULL;
}



#include <huffman.h>
#include <bitdiet.h>

#define BLOCK_SIZE (KiB*32)

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

__private uint16_t r16(const uint8_t* inp, unsigned* pos){
	uint16_t ret = inp[(*pos)++];
	ret <<= 8;
	ret |=  inp[(*pos)++];
	return ret;
}

__private uint32_t r32(const uint8_t* inp, unsigned* pos){
	uint32_t ret = inp[(*pos)++];
	ret <<= 8;
	ret |=  inp[(*pos)++];
	ret <<= 8;
	ret |=  inp[(*pos)++];
	ret <<= 8;
	ret |=  inp[(*pos)++];
	return ret;
}

typedef struct blkmap{
	unsigned ipos;
	unsigned opos;
	unsigned size;
	void* out;
}blkmap_s;

void* bitdiet_compress(const void* data, unsigned size){
	const uint8_t* idata = data;
	const unsigned blockmax = (size/BLOCK_SIZE)+1;
	__free blkmap_s* map = MANY(blkmap_s, blockmax);
	unsigned blkcount = 0;
	unsigned ipos = 0;
	while( ipos < size ){
		iassert(blkcount < blockmax);
		map[blkcount].out  = NULL;
		map[blkcount].opos = 0;
		map[blkcount].ipos = ipos;
		map[blkcount].size = size-ipos < BLOCK_SIZE ? size-ipos: BLOCK_SIZE;
		ipos += map[blkcount].size;
		++blkcount;
	}
	
	__parallef
	for( unsigned i = 0; i < blkcount; ++i ){
		dbg_info("compress pos %u size %u", map[i].ipos, map[i].size);
		map[i].out = huffman_compress(&idata[map[i].ipos], map[i].size);
		if( map[i].out ) map[i].size = m_header(map[i].out)->len;
	}
	if( !map[0].out ) goto ERR_HUFFMAN;
	unsigned totalsize = 2+4+4*blkcount;
	map[0].opos = totalsize;
	totalsize += map[0].size;
	for( unsigned i = 1; i < blkcount; ++i ){
		if( !map[i].out ) goto ERR_HUFFMAN;
		map[i].opos = map[i-1].opos + map[i-1].size;
		totalsize += map[i].size;
	}
	
	uint8_t* bdet = MANY(uint8_t, totalsize);
	unsigned long bpos = 0;	
	bpos = w16(bdet, bpos, BITDIET_FORMAT);
	bpos = w32(bdet, bpos, blkcount);
	dbg_info("block count: %u", blkcount);
	unsigned long offarray = bpos;
	
	__parallef
	for( unsigned i = 0; i < blkcount; ++i ){
		memcpy(&bdet[map[i].opos], map[i].out, map[i].size);
		m_free(map[i].out);
		w32(bdet, offarray+i*4, map[i].opos);
		dbg_info("[%lu] %u (size %u)", offarray+i*4, map[i].opos, map[i].size);
	}
	
	m_header(bdet)->len = totalsize;
	return bdet;
ERR_HUFFMAN:
	for( unsigned i = 0; i < blkcount; ++i ){
		m_free(map[i].out);
	}
	return NULL;
}

void* bitdiet_decompress(const void* data, unsigned size){
	dbg_info("");
	const uint8_t* idata = data;
	unsigned ipos = 0;
	const uint16_t format = r16(data, &ipos);
	if( format == HUFFMAN_FORMAT ) return huffman_decompress(data, size);
	if( format != BITDIET_FORMAT ){
		errno = ENOEXEC;
		return NULL;
	}
	const unsigned blkcount = r32(data, &ipos);
	dbg_info("block count: %u", blkcount);
	__free blkmap_s* map = MANY(blkmap_s, blkcount);
	
	map[0].out  = NULL;
	map[0].opos = 0;
	map[0].ipos = r32(data, &ipos);
	map[0].size = 0;
	for( unsigned i = 1; i < blkcount; ++i ){
		map[i].out  = NULL;
		map[i].opos = 0;
		map[i].ipos = r32(data, &ipos);
		map[i-1].size = map[i].ipos - map[i-1].ipos;

	}
	map[blkcount-1].size = size - map[blkcount-1].ipos;
	
	for( unsigned i = 0; i < blkcount; ++i ){
		dbg_info("[%u] %u (size %u)", i, map[i].ipos, map[i].size);
	}
	
	__parallef
	for( unsigned i = 0; i < blkcount; ++i ){
		dbg_info("decompress %u size %u", map[i].ipos, map[i].size);
		map[i].out = huffman_decompress(&idata[map[i].ipos], map[i].size);
		if( map[i].out ) map[i].size = m_header(map[i].out)->len;
	}
	
	if( !map[0].out ) goto ERR_HUFFMAN;
	unsigned totalsize = map[0].size;
	for( unsigned i = 1; i < blkcount; ++i ){
		if( !map[i].out ) goto ERR_HUFFMAN;
		map[i].opos = map[i-1].opos + map[i-1].size;
		totalsize += map[i].size;
	}
	
	uint8_t* bdet = MANY(uint8_t, totalsize);
	m_header(bdet)->len = totalsize;
	__parallef
	for( unsigned i = 0; i < blkcount; ++i ){
		memcpy(&bdet[map[i].opos], map[i].out, map[i].size);
		dbg_info("write [%u] %u size %u", i, map[i].opos, map[i].size);
		m_free(map[i].out);
	}
	return bdet;
ERR_HUFFMAN:
	for( unsigned i = 0; i < blkcount; ++i ){
		m_free(map[i].out);
	}
	return NULL;
}











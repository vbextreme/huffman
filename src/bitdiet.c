#include <huffman.h>
#include <bitdiet.h>

#define BLOCK_SIZE (KiB*32)

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

typedef struct blkmap{
	size_t ipos;
	size_t opos;
	size_t size;
	void* out;
}blkmap_s;

void* bitdiet_compress(const void* data, const size_t size){
	const uint8_t* idata = data;
	const size_t blockmax = (size/BLOCK_SIZE)+1;
	__free blkmap_s* map = MANY(blkmap_s, blockmax);
	size_t blkcount = 0;
	size_t ipos = 0;
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
	for( size_t i = 0; i < blkcount; ++i ){
		dbg_info("compress pos %lu size %lu", map[i].ipos, map[i].size);
		map[i].out = huffman_compress(&idata[map[i].ipos], map[i].size);
		if( map[i].out ) map[i].size = m_header(map[i].out)->len;
	}
	if( !map[0].out ) goto ERR_HUFFMAN;
	size_t totalsize = 2+8+8*blkcount;
	map[0].opos = totalsize;
	totalsize += map[0].size;
	for( size_t i = 1; i < blkcount; ++i ){
		if( !map[i].out ) goto ERR_HUFFMAN;
		map[i].opos = map[i-1].opos + map[i-1].size;
		totalsize += map[i].size;
	}
	
	uint8_t* bdet = MANY(uint8_t, totalsize);
	unsigned long bpos = 0;	
	bpos = w16(bdet, bpos, BITDIET_FORMAT);
	bpos = w64(bdet, bpos, blkcount);
	dbg_info("block count: %lu", blkcount);
	unsigned long offarray = bpos;
	
	__parallef
	for( size_t i = 0; i < blkcount; ++i ){
		memcpy(&bdet[map[i].opos], map[i].out, map[i].size);
		m_free(map[i].out);
		w64(bdet, offarray+i*8, map[i].opos);
		dbg_info("[%lu] %lu (size %lu)", offarray+i*4, map[i].opos, map[i].size);
	}
	
	m_header(bdet)->len = totalsize;
	return bdet;
ERR_HUFFMAN:
	for( size_t i = 0; i < blkcount; ++i ){
		m_free(map[i].out);
	}
	return NULL;
}

void* bitdiet_decompress(const void* data, const size_t size){
	dbg_info("");
	const uint8_t* idata = data;
	size_t ipos = 0;
	const uint16_t format = r16(data, &ipos);
	if( format == HUFFMAN_FORMAT ) return huffman_decompress(data, size);
	if( format != BITDIET_FORMAT ){
		errno = ENOEXEC;
		return NULL;
	}
	const size_t blkcount = r64(data, &ipos);
	dbg_info("block count: %lu", blkcount);
	__free blkmap_s* map = MANY(blkmap_s, blkcount);
	
	map[0].out  = NULL;
	map[0].opos = 0;
	map[0].ipos = r64(data, &ipos);
	map[0].size = 0;
	for( size_t i = 1; i < blkcount; ++i ){
		map[i].out  = NULL;
		map[i].opos = 0;
		map[i].ipos = r64(data, &ipos);
		map[i-1].size = map[i].ipos - map[i-1].ipos;

	}
	map[blkcount-1].size = size - map[blkcount-1].ipos;
	
	for( size_t i = 0; i < blkcount; ++i ){
		dbg_info("[%lu] %lu (size %lu)", i, map[i].ipos, map[i].size);
	}
	
	__parallef
	for( size_t i = 0; i < blkcount; ++i ){
		dbg_info("decompress %lu size %lu", map[i].ipos, map[i].size);
		map[i].out = huffman_decompress(&idata[map[i].ipos], map[i].size);
		if( map[i].out ) map[i].size = m_header(map[i].out)->len;
	}
	
	if( !map[0].out ) goto ERR_HUFFMAN;
	size_t totalsize = map[0].size;
	for( size_t i = 1; i < blkcount; ++i ){
		if( !map[i].out ) goto ERR_HUFFMAN;
		map[i].opos = map[i-1].opos + map[i-1].size;
		totalsize += map[i].size;
	}
	
	uint8_t* bdet = MANY(uint8_t, totalsize);
	m_header(bdet)->len = totalsize;
	__parallef
	for( size_t i = 0; i < blkcount; ++i ){
		memcpy(&bdet[map[i].opos], map[i].out, map[i].size);
		dbg_info("write [%lu] %lu size %lu", i, map[i].opos, map[i].size);
		m_free(map[i].out);
	}
	return bdet;
ERR_HUFFMAN:
	for( size_t i = 0; i < blkcount; ++i ){
		m_free(map[i].out);
	}
	return NULL;
}











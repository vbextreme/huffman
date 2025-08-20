#ifndef __HUFFMAN_H__
#define __HUFFMAN_H__

#include <notstd/core.h>

//.hff format
//(size in bits)
//(16) FORMAT 0x0150
//(64) original size in bytes
//(64) blocksize in bits
//(16)  table count
//-[]- (8)  value
//-[]- (64) repeat
//(N)  compressed block

#define HUFFMAN_FORMAT  0x0150

void* huffman_compress(const void* data, const size_t len);
void* huffman_decompress(const void* data, const size_t len);


#endif

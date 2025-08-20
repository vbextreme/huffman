#ifndef __BITDIET_H__
#define __BITDIET_H__

#include <notstd/core.h>

//.bdet format
//(size in bits)
//(16) FORMAT 0x1005
//(32) number of block
//-[]- (64) offset
//(N)  huffman block

#define BITDIET_FORMAT 0x1005

void* bitdiet_compress(const void* data, unsigned size);
void* bitdiet_decompress(const void* data, unsigned size);

#endif

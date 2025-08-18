#ifndef __HUFFMAN_H__
#define __HUFFMAN_H__

#include <notstd/core.h>

//.hff format
//(size in bits)
//(16) FORMAT 0x0150
//(32) original size in bytes
//(64) blocksize in bits
//(16)  table count
//---  (8)  value
//---  (32) repeat
//[0] (32) aligned 1
//[1] (32) aligned 2
//[2] (32) aligned 3
//(N)  compressed block


void* huffman_compress(const void* data, const unsigned len, int multi);
void* huffman_decompress(const void* data, unsigned len);


#endif

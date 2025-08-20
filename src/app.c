#include <notstd/opt.h>
#include <huffman.h>
#include <bitdiet.h>

#define BUF_SIZE (4096*2)

typedef enum{
	OPT_c,
	OPT_d,
	OPT_i,
	OPT_b,
	OPT_h
}options_e;

option_s opt[] = {
	{ 'c', "--compress"  , "compress data in stdin"                                    , OPT_NOARG          , 0, NULL},
	{ 'd', "--decompress", "decompress data in stdin"                                  , OPT_NOARG          , 0, NULL},
	{ 'i', "--info"      , "in compression mode display in stderr the % of compression", OPT_NOARG          , 0, NULL},	
	{ 'b', "--bitdiet"   , "enable bitdiet compression"                                , OPT_NOARG          , 0, NULL},
	{ 'h', "--help"      , "display this"                                              , OPT_NOARG | OPT_END, 0, NULL},
};

__private uint8_t* fd_slurp(int fd){
	uint8_t* inp = MANY(uint8_t, BUF_SIZE);
	ssize_t rd;
	while( (rd=read(fd, &inp[m_header(inp)->len], m_available(inp))) > 0 ){
		m_header(inp)->len += rd;
		inp = m_grow(inp, BUF_SIZE);
	}
	if( rd < 0 ) die("error on read stdin: %m");
	return inp;
}

__private void fd_splash(int fd, const void* data, size_t size){
	char* d = (char*)data;
	size_t out = 0;
	while( out < size ){
		const size_t block = size-out < BUF_SIZE ? size-out : BUF_SIZE;
		if( write(fd, &d[out], block) < (ssize_t)block ) die("error on writing: %m");
		out += block;
	}
}

int main(int argc, char** argv){
	argv_parse(opt, argc, argv);
	if( opt[OPT_h].set ){
		argv_usage(opt, argv[0]);
		return 0;
	}
	
	__free uint8_t* inp = fd_slurp(0);
	__free uint8_t* out = NULL;
	errno = 0;
	if( opt[OPT_c].set ){
		if( opt[OPT_b].set ){
			out = bitdiet_compress(inp, m_header(inp)->len);
		}
		else{
			out = huffman_compress(inp, m_header(inp)->len);
		}
		if( opt[OPT_i].set ){
			fprintf(stderr, "compress %lu -> %lu %%%f", m_header(inp)->len, m_header(out)->len, 100.0-(100.0*m_header(out)->len/m_header(inp)->len));
		}
	}
	if( opt[OPT_d].set ){
		if( opt[OPT_b].set ){
			out = bitdiet_decompress(inp, m_header(inp)->len);
		}
		else{
			out = huffman_decompress(inp, m_header(inp)->len);
		}
	}
	if( !out ){
		if( !errno ) die("usage -c or -d, try with -h");
		else die("huffman error: %m");
	}
	
	fd_splash(1, out, m_header(out)->len);
	return 0;
}

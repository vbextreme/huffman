#include <notstd/opt.h>
#include <huffman.h>

#define BUF_SIZE (4096*2)

typedef enum{
	OPT_c,
	OPT_d,
	OPT_p,
	OPT_i,
	OPT_h
}options_e;

option_s opt[] = {
	{ 'c', "--compress"  , "compress data in stdin"                                    , OPT_NOARG          , 0, NULL},
	{ 'd', "--decompress", "decompress data in stdin"                                  , OPT_NOARG          , 0, NULL},
	{ 'p', "--parallel"  , "enable parallel decompression"                             , OPT_NOARG          , 0, NULL},	
	{ 'i', "--info"      , "in compression mode display in stderr the % of compression", OPT_NOARG          , 0, NULL},	
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
		out = huffman_compress(inp, m_header(inp)->len, opt[OPT_p].set);
		if( opt[OPT_i].set ){
			fprintf(stderr, "compress %u -> %u %%%f", m_header(inp)->len, m_header(out)->len, 100.0-(100.0*m_header(out)->len/m_header(inp)->len));
		}
	}
	if( opt[OPT_d].set ){
		out = huffman_decompress(inp, m_header(inp)->len);
	}
	if( !out ){
		if( !errno ) die("usage -c or -d, try with -h");
		else die("huffman error: %m");
	}
	
	write(1, out, m_header(out)->len);
	
	return 0;
}

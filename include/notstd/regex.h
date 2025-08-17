#ifndef __NOTSTD_REGEX_H__
#define __NOTSTD_REGEX_H__

#include <notstd/core.h>
typedef struct regex{
	void*        rx;
	void*        match;
	size_t*      offv;
	size_t       offc;
	char*        regex;
	const char*  text;
	size_t       lent;
	unsigned int newline;
	unsigned     rexflags;
	int          utf8;
	int          crlf;
	int          clearregex;
	int          err;
	size_t       erroff;
}regex_s;

regex_s* regex_ctor(regex_s* rx);
regex_s* regex_text(regex_s* rx, const char* txt, size_t len);
regex_s* regex_match_delete(regex_s* rx);
regex_s* regex_dtor(regex_s* rx);
const char* regex_err_str(regex_s* rx);
int regex_build(regex_s* rx, const char* regex);
void regex_match_always_begin(regex_s* rx);
int regex_match(regex_s* rx);
size_t regex_match_count(regex_s* rx);
const char* regex_match_get(regex_s* rx, unsigned index, unsigned* lenout);
char** str_regex(const char* regex, const char* str, int global);


#endif

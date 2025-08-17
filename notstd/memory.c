#undef DBG_ENABLE

#include <notstd/futex.h>
#include <notstd/debug.h>
#include <notstd/mathmacro.h>
#include <notstd/error.h>
#include <notstd/mth.h>

#include <stdlib.h>
#include <string.h>

#define MEMORY_IMPLEMENTATION
#include <notstd/memory.h>

unsigned PAGE_SIZE;

#ifdef OS_LINUX
#include <unistd.h>
#define os_page_size() sysconf(_SC_PAGESIZE);
#else
#define os_page_size() 512
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
// memory manager

void mem_begin(void){
	PAGE_SIZE  = os_page_size();
}

__malloc void* m_alloc(unsigned sof, size_t count, mcleanup_f dtor){
	iassert(sof);
	iassert(count);
	const size_t size = sof * count + sizeof(hmem_s);
	hmem_s* hm  = malloc(size);
	if( !hm ) die("malloc: %m");
	hm->refs    = 1;
	hm->flags   = HMEM_FLAG_CHECK;
	hm->count   = count;
	hm->cleanup = dtor;
	hm->len     = 0;
	hm->sof     = sof;
	void* ret = hm_toaddr(hm);
	iassert( ADDR(ret) % sizeof(uintptr_t) == 0 );
	dbg_info("mem addr: %p header: %p", ret, hm);
	return ret;
}

void* m_realloc(void* mem, size_t count){
	hmem_s* hm = m_header(mem);
	if( hm->count == count ) return mem;
	if( hm->refs > 1 ) die("unsafe realloc shared ptr");
	const size_t size = sizeof(hmem_s) + count * hm->sof;
	hm = realloc(hm, size);
	if( !hm ) die("realloc: %m");
	hm->count = count;
	void* ret = hm_toaddr(hm);
	iassert( ADDR(ret) % sizeof(uintptr_t) == 0 );
	return ret;
}

void m_free(void* addr){
	dbg_info("free addr: %p", addr);
	if( !addr ) return;
	hmem_s* hm = m_header(addr);
	if( !hm->refs ) die("double free");
	if( --hm->refs ) return;
	if( hm->cleanup ) hm->cleanup(addr);
	free(hm);
}

void* m_borrowed(void* mem){
	if( !mem ) return mem;
	++m_header(mem)->refs;
	return mem;
}

void* m_delete(void* mem, unsigned index, unsigned count){
	hmem_s* hm = m_header(mem);
	if( !count || index >= hm->len){
		errno = EINVAL;
		return mem;
	}
	if( index + count >= hm->len ){
		hm->len = index;
		return mem;
	}	
	void*  dst  = (void*)ADDRTO(mem, hm->sof, index);
	void*  src  = (void*)ADDRTO(mem, hm->sof, (index+count));
	size_t size = (hm->len - (index+count)) * hm->sof;
	memmove(dst, src, size);
	hm->len -= count;
	return mem;
}

void* m_widen(void* mem, size_t index, size_t count){
	if( !count ){
		errno = EINVAL;
		return mem;
	}
	mem = m_grow(mem, count);
	hmem_s* hm = m_header(mem);
	if( index > hm->len ){
		errno = EINVAL;
		return mem;
	}
	if( index == hm->len ){
		hm->len = index + count;
		return mem;
	}
	void*  src  = (void*)ADDRTO(mem, hm->sof, index);
	void*  dst  = (void*)ADDRTO(mem, hm->sof, (index+count));
	size_t size = (hm->len - (index+count)) * hm->sof;
	memmove(dst, src, size);
	hm->len += count;
	return mem;
}

void* m_insert(void* restrict dst, size_t index, void* restrict src, size_t count){
	errno = 0;
	dst = m_widen(dst, index, count);
	if( errno ) return dst;	
	const unsigned sof = m_header(dst)->sof;
	void* draw = (void*)ADDRTO(dst, sof, index);
	memcpy(draw, src, count * sof);
	return dst;
}

void* m_shuffle(void* mem, size_t begin, size_t end){
	hmem_s* hm = m_header(mem);
	if( end == 0 && hm->len ) end = hm->len-1;
	if( end == 0 ) return mem;
	const size_t count = (end - begin) + 1;
	for( size_t i = begin; i <= end; ++i ){
		size_t j = begin + mth_random(count);
		if( j != i ){
			void* a = (void*)ADDRTO(mem, hm->sof, i);
			void* b = (void*)ADDRTO(mem, hm->sof, j);
			memswap(a , hm->sof, b, hm->sof);
		}
	}
	return mem;
}


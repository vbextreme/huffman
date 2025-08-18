#ifndef __NOTSTD_MEMORY_H__
#define __NOTSTD_MEMORY_H__

#include <stddef.h>
#include <stdlib.h>
#include <notstd/type.h>
#include <notstd/compiler.h>
#include <notstd/xmacro.h>
#include <notstd/debug.h>

//validation value of memory canary
#define HMEM_FLAG_CHECK    0x0000F1CA

//realign to next valid address of pointers, prevent strict aliasing
//int v[32];
//int* pv = v[1];
//char* ch = (char*)pv;
//++ch;
//pv = (int*)ch; //strict aliasing
//aliasing_next(pv); //now pv points to correct address, v[2]
#define aliasing_next(PTR) do{ PTR = (void*)(ROUND_UP(ADDR(PTR), sizeof(PTR[0]))) } while(0)

//realign to previus valid address of pointers, prevent strict aliasing
//aliasing_prev(pv); //now pv points to correct address, v[1]
#define aliasing_prev(PTR) do{ PTR = (void*)(ROUND_DOWN(ADDR(PTR), sizeof(PTR[0]))) } while(0)

//return 0 if not violate strict aliasing, otherwise you can't cast without creash your software
#define aliasing_check(PTR) (ADDR(PTR)%sizeof(PTR[0]))

/************/
/* memory.c */
/************/

//raii attribute for cleanup and free memory allocated with mem_alloc
#define __free __cleanup(mem_free_raii)
#define MANY_0(T,C,arg...) (T*)m_alloc(sizeof(T), (C), NULL)
#define MANY_1(T,C,arg...) (T*)m_alloc(sizeof(T), (C), ##arg)
#define MANY(T,C,arg...)   CONCAT_EXPAND(MANY_, VA_COUNT(arg))(T,C, ##arg)
#define NEW(T,arg...)      MANY(T,1, ##arg)
#define OBJ(N,arg...)      (T*)N##_ctor(m_alloc(sizeof(N), 1, N##_dtor), ##arg)
#define DELETE(M)          do{ m_free(M); (M)=NULL; }while(0)
#define RESIZE(T,M,C)      (T*)m_realloc((M), (C))

//callback type for cleanup object
typedef void (*mcleanup_f)(void*);

#ifdef MEMORY_IMPLEMENTATION
#include <notstd/field.h>
#else
extern unsigned PAGE_SIZE;
#endif

typedef struct hmem{
	__rdwr mcleanup_f cleanup;
	__rdwr unsigned   len;
	__rdon unsigned   sof;
	__rdon unsigned   refs;
	__rdon unsigned   count; // max numbers of objects
	__prv8 uint32_t   flags;
}hmem_s;

typedef struct mslice{
	void*  base;
	size_t begin;
	size_t end;
}mslice_s;

//is called in notstd_begin(), need call only one time
void mem_begin(void);

//allocate memory: sof sizeof, count numbers of object, dtor cleanup function
__malloc void* m_alloc(unsigned sof, size_t count, mcleanup_f dtor);

//reallocate memory, mem address previus allocate with m_alloc, count numbers of objects
void* m_realloc(void* mem, size_t count);

//release memory
void m_free(void* addr);

//increase reference count
void* m_borrowed(void* mem);

//delete elements, index < hm->len, count > 0, decrease len and move memory if required
void* m_delete(void* mem, unsigned index, unsigned count);

//increase space and increment len, index <= hm->len, count > 0
void* m_widen(void* mem, size_t index, size_t count);

//widen dst to index and copy src(not required hmem) with count objects
void* m_insert(void* restrict dst, size_t index, void* restrict src, size_t count);

//randomize elements in array, from begin and end included
void* m_shuffle(void* mem, size_t begin, size_t end);




//memset(0)
void mem_zero(void* addr);

int mem_isheap(void* addr);

__unsafe_begin;
__unsafe_unused_fn;
__unsafe_deprecated;

//return 0 success otherwise error
__private int hm_check(hmem_s* hm){
	return (hm->flags & 0x0000FFFF) ^ HMEM_FLAG_CHECK;
}

__private void* hm_toaddr(hmem_s* hm){
	return (void*)(ADDR(hm)+sizeof(hmem_s));
}

//return memory header
__private hmem_s* m_header(void* addr){
	iassert(addr);
	hmem_s* hm = ((hmem_s*)(ADDR(addr)-sizeof(hmem_s)));
	iassert(!hm_check(hm));
	return hm;
}

//return all size in byte without header
__private unsigned m_size(void* addr){
	hmem_s* hm = m_header(addr);
	return hm->count * hm->sof - sizeof(hmem_s);
}

//return count memory can be used
__private unsigned m_available(void* addr){
	hmem_s* hm = m_header(addr);
	return hm->count - hm->len;
}

//return pointer to address, &ptr[index]
__private void* m_addressing(void* addr, unsigned index){
	hmem_s* hm = m_header(addr);
	return (void*)ADDRTO(addr, hm->sof, index);
}

//this function is called from __free
__private void mem_free_raii(void* addr){
	m_free(*(void**)addr);
}

//increase space in memory, add count to vector
__private void* m_grow(void* mem, size_t count){
	hmem_s* hm = m_header(mem);
	if( hm->len + count > hm->count ){
		mem = m_realloc(mem, (hm->len + count) * 2);
	}
	return mem;
}

//increase space in memory setted to 0, add count to vector
__private void* m_grow_zero(void* mem, size_t count){
	hmem_s* hm = m_header(mem);
	if( hm->len + count > hm->count ){
		mem = m_realloc(mem, (hm->len + count) * 2);
		hm = m_header(mem);
		memset((void*)ADDRTO(mem, hm->sof, hm->len), 0, (hm->count - hm->len)*hm->sof);
	}
	return mem;
}

//decrease memory space
__private void* m_shrink(void* mem){
	hmem_s* hm = m_header(mem);
	if( hm->len < hm->count / 4 ){
		mem = m_realloc(mem, hm->len * 2);
	}
	return mem;
}

//remove unused space
__private void* m_fit(void* mem){
	return m_realloc(mem, m_header(mem)->len);
}

//WARNING in this function need pass &mem and not mem because return index of pushed element and not new memory addrresssss
__private unsigned m_ipush(void* dst){
	*(void**)dst = m_grow(*(void**)dst, 1);
	return m_header(*(void**)dst)->len++;
}

__private void* m_push(void* arr, void* src){
	unsigned index = m_ipush(&arr);
	void* dst = m_addressing(arr, index);
	memcpy(dst, src, m_header(arr)->sof);
	return arr;
}

//WARNING return address memory out of len or NULL
__private void* m_pop(void* restrict mem){
	hmem_s* hm = m_header(mem);
	if( !hm->len ) return NULL;
	return (void*)ADDRTO(mem, hm->sof, --hm->len);
}

//WARNING return index memory out of len or NULL
__private long m_ipop(void* restrict mem){
	hmem_s* hm = m_header(mem);
	if( !hm->len ) return -1;
	return --hm->len;
}

//classic qsort
__private void* m_qsort(void* mem, cmp_f cmp){
	hmem_s* hm = m_header(mem);
	qsort(mem, hm->len, hm->sof, cmp);
	return mem;
}

//classic bsearch
__private void* m_bsearch(void* mem, void* search, cmp_f cmp){
	hmem_s* hm = m_header(mem);
	return bsearch(search, mem, hm->len, hm->sof, cmp);
}

//same bsearch but return index
__private long m_ibsearch(void* mem, void* search, cmp_f cmp){
	hmem_s* hm = m_header(mem);
	void* r = bsearch(search, mem, hm->len, hm->sof, cmp);
	if( !r ) return -1;
	return (r - mem) / hm->sof;
}

//position by index: -1 == last element, < 0 index by right, >=0 index by left, if index > len index = len-1
__private unsigned m_index(void* mem, long index){
	hmem_s* hm = m_header(mem);
	if( !hm->len ) return 0;
	if( labs(index) > (long)hm->len ){
		if( index < 0 ) return 0;
		else            return hm->len-1;
	}
	return index < 0 ? hm->len - index: index;
}

//same index but return address
__private void* m_indexing(void* mem, long index){
	index = m_index(mem, index);
	hmem_s* hm = m_header(mem);
	return (void*)ADDRTO(mem, hm->sof, index);
}

//clear elements
__private void* m_clear(void* mem){
	m_header(mem)->len = 0;
	return mem;
}

//memset 0, set all to 0
__private void* m_zero(void* mem){
	hmem_s* hm = m_header(mem);
	memset(mem, 0, hm->sof*hm->count);
	return mem;
}

//set \0 at end of memory region
__private void* m_nullterm(void* mem){
	mem = m_grow(mem, 1);
	hmem_s* hm = m_header(mem);
	char* dst = (void*)ADDRTO(mem, hm->sof, hm->len);
	*dst = 0;
	return mem;
}

//copy if count = 0 copy all src in dst, all dst is reset, src and dst need same type
__private void* m_copy(void* dst, void* src, unsigned count){
	m_clear(dst);
	const hmem_s* hsrc = m_header(src);
	if( !count ){
		count = hsrc->len;
		if( !count ) return dst;
	}
	if( count > hsrc->len ) count = hsrc->len;
	dst = m_grow(dst, count);
	memcpy(dst, src, count * hsrc->sof);
	m_header(dst)->len = count;
	return dst;
}

__private void m_bit_set0(void* dst, const unsigned long pos, const uint8_t value){	
	const unsigned long i = pos >> 3;
	const unsigned long o = 7 - (pos & 7);
	char* d = dst;
	d[i] |= value << o;
}

__private void m_bit_set(void* dst, const unsigned long pos, const uint8_t value){	
	const unsigned long i = pos >> 3;
	const unsigned long o = 7 - (pos & 7);
	char* d = dst;
	if( value ){
		d[i] |= 1 << o;
	}
	else{
		d[i] &= ~(1 << o);
	}
}

__private uint8_t m_bit_get(const void* const inp, const unsigned long pos){
	const unsigned long i = pos >> 3;
	const unsigned long o = 7 - (pos & 7);
	const char* const in = inp;
	return (in[i] >> o) & 1;
}

__unsafe_end;

#define mforeach(M,IT) for(unsigned IT = 0; IT < m_header(M)->len; ++IT)


/************/
/* extras.c */
/************/

//classic way for swap any value
#define swap(A,B) ({ typeof(A) tmp = A; (A) = (B); (B) = tmp; })

//swap memory region, size is how many memory you need to swap, warning size is not checked, -1 if !a||!b||!sizeA||!sizeB
int memswap(void* restrict a, size_t sizeA, void* restrict b, size_t sizeB);

//call memswap but check size and realloc if need
//mem_swap(&a, 5, &b, 7)
int mem_swap(void* restrict a, size_t sza, void* restrict b, size_t szb);


#endif

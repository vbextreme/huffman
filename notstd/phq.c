/* fork from Volkan Yazıcı priority heap queue */
#include <notstd/memory.h>
#define PHQ_IMPLEMENTATION
#include <notstd/phq.h>

#define left(i)   ((i) << 1)
#define right(i)  (((i) << 1) + 1)
#define parent(i) ((i) >> 1)

phq_s* phq_ctor(phq_s* p, size_t size, cmp_f cmp, phqIndexGet_f iget, phqIndexSet_f iset){
	++size;
	p->cmp   = cmp;
	p->iget  = iget;
	p->iset  = iset;
	p->queue = MANY(void*, size);
	m_header(p->queue)->len = 1;
	m_zero(p->queue);
    return p;
}

void phq_dtor(void* ph){
	phq_s* p = ph;
	m_free(p->queue);
}

unsigned phq_size(phq_s* p){
    return m_header(p->queue)->count-1;
}

__private void bubble_up(phq_s* p, size_t i){
	size_t parentnode;
	void* movingnode = p->queue[i];
	for( parentnode = parent(i); ((i > 1) && p->cmp(p->queue[parentnode], movingnode)); i = parentnode, parentnode = parent(i) ){
		p->queue[i] = p->queue[parentnode];
		p->iset(p->queue[i], i);
    }
	p->queue[i] = movingnode;
	p->iset(p->queue[i], i);
}

__private size_t maxchild(phq_s* p, size_t i){
    size_t childnode = left(i);
	const unsigned count = m_header(p->queue)->len-1;
    if( childnode > count ) return 0;
    if( (childnode+1) <= count && p->cmp(p->queue[childnode], p->queue[childnode+1]) ) childnode++;
    return childnode;
}

__private void percolate_down(phq_s* p, size_t i){
	size_t childnode;
	void* movingnode = p->queue[i];
	while( (childnode = maxchild(p, i)) && p->cmp(movingnode, p->queue[childnode]) ){
		p->queue[i] = p->queue[childnode];
		p->iset(p->queue[i], i);
		i = childnode;
	}
	p->queue[i] = movingnode;
	p->iset(p->queue[i], i);
}

void phq_push(phq_s* p, void* data){
	p->queue = m_grow_zero(p->queue, 1);
	hmem_s* hm = m_header(p->queue);
    const unsigned i = hm->len++;
    p->queue[i] = data;
	bubble_up(p, i);
}

void phq_change_priority(phq_s *p, void* data, unsigned cmpPrio){
	const unsigned posn = p->iget(data);
    if( cmpPrio ) bubble_up(p, posn);
    else percolate_down(p, posn);
}

void phq_remove(phq_s* p, void* data){
	const unsigned posn = p->iget(data);
	hmem_s* hm = m_header(p->queue);
	const unsigned last = hm->len - 1;
	if( hm->len <= 2 ){
		p->queue[1] = 0;
		--hm->len;
		return;
	}
	if( last == posn ){
		p->queue[hm->len-1] = 0;
		--hm->len;
		return;
	}
	
	void* moving = p->queue[last];
	p->queue[posn] = moving;
	p->queue[last] = 0;
	--hm->len;
	p->iset(moving, 1);
	if (posn > 1 && p->cmp(p->queue[parent(posn)], p->queue[posn])){
		bubble_up(p, posn);
	}
	else{
		percolate_down(p, posn);
	}
	
	p->iset(data, 0);
}

void* phq_pop(phq_s* p){
	hmem_s* hm = m_header(p->queue);
	if( hm->len < 2 ) return NULL;
	void* ret = p->queue[1];
	p->queue[1] = p->queue[--hm->len];
	if (hm->len >= 1) p->iset(p->queue[1], 1);
	percolate_down(p, 1);
	p->queue[hm->len] = 0;
	return ret;
}

void* phq_peek(phq_s* p){
	return m_header(p->queue)->len > 1 ? p->queue[1]: NULL;
}



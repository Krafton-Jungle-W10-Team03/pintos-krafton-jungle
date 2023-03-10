#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"

#include <hash.h> // need to hash

enum vm_type {
	/* page not initialized */
	/* uninit.c */
	VM_UNINIT = 0,
	/* page not related to the file, aka anonymous page */
	/* anon.c */
	VM_ANON = 1,
	/* page that realated to the file */
	/* file.c */
	VM_FILE = 2,
	/* page that hold the page cache, for project 4 */
	/* 프로젝트 4에서 다룰 것 */
	VM_PAGE_CACHE = 3,

	/* Bit flags to store state */

	/* Auxillary bit flag marker for store information. You can add more
	 * markers, until the value is fit in the int. */
	VM_MARKER_0 = (1 << 3),
	VM_MARKER_1 = (1 << 4),

	/* DO NOT EXCEED THIS VALUE. */
	VM_MARKER_END = (1 << 31),
};

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;

#define VM_TYPE(type) ((type) & 7)

/* The representation of "page".
 * This is kind of "parent class", which has four "child class"es, which are
 * uninit_page, file_page, anon_page, and page cache (project4).
 * DO NOT REMOVE/MODIFY PREDEFINED MEMBER OF THIS STRUCTURE. */
/* "페이지"를 나타낸다.
 * uninit_page, file_page, anon_page, page cache의 4개의 자녀 클래스를 가지고 있다. */
struct page {
	const struct page_operations *operations;
	void *va;              /* Address in terms of user space */
	struct frame *frame;   /* Back reference for frame */

	/* Your implementation */
	/*-------------------------[P3]hash table---------------------------------*/
	struct hash_elem hash_elem; // 해쉬 테이블 element(page와 해쉬테이블 연결해주는 element)
	// key : page->va, value : struct page
	bool writable; // 페이지의 R/W 여부 (ref. pml4_set_page)
	/*-------------------------[P3]hash table---------------------------------*/

	/* Per-type data are binded into the union.
	 * Each function automatically detects the current union */
	// page type에 맞는 구조체 형식을 가질 수 있도록 정의
	union {
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
#ifdef EFILESYS
		struct page_cache page_cache;
#endif
	};
};

/* The representation of "frame" */
struct frame {
	void *kva;
	struct page *page;
	/*-------------------------[P3]frame table---------------------------------*/
	struct list_elem frame_elem; // frame을 리스트 형태로 구현했기 때문에 list_elem을 추가한다.
	/*-------------------------[P3]frame table---------------------------------*/

};





/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. */
/*
 * 함수 포인터들과 type 멤버로 이루어져 있음
 * swap in,swap out,destroy를 사용할 때 해당 page의 타입에 따라 구조체로 정의된 함수를 사용하도록 구성
 * page type에 따라 구조체, 사용함수를 구분해 놓는 목적
*/
struct page_operations {
	bool (*swap_in) (struct page *, void *);
	bool (*swap_out) (struct page *);
	void (*destroy) (struct page *);
	enum vm_type type;
};

#define swap_in(page, v) (page)->operations->swap_in ((page), v)
#define swap_out(page) (page)->operations->swap_out (page)
#define destroy(page) \
	if ((page)->operations->destroy) (page)->operations->destroy (page)

/* Representation of current process's memory space.
 * We don't want to force you to obey any specific design for this struct.
 * All designs up to you for this. */
/* 보조 페이지 테이블에 대해 구현한다. */
struct supplemental_page_table {
	/*-------------------------[P3]hash table---------------------------------*/
	struct hash spt_hash;
	/*-------------------------[P3]hash table---------------------------------*/
};

#include "threads/thread.h"
void supplemental_page_table_init (struct supplemental_page_table *spt);
bool supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src);
void supplemental_page_table_kill (struct supplemental_page_table *spt);
struct page *spt_find_page (struct supplemental_page_table *spt,
		void *va);
bool spt_insert_page (struct supplemental_page_table *spt, struct page *page);
void spt_remove_page (struct supplemental_page_table *spt, struct page *page);

void vm_init (void);
bool vm_try_handle_fault (struct intr_frame *f, void *addr, bool user,
		bool write, bool not_present);

#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer ((type), (upage), (writable), NULL, NULL)
bool vm_alloc_page_with_initializer (enum vm_type type, void *upage,
		bool writable, vm_initializer *init, void *aux);
void vm_dealloc_page (struct page *page);
bool vm_claim_page (void *va);
enum vm_type page_get_type (struct page *page);

#endif  /* VM_VM_H */

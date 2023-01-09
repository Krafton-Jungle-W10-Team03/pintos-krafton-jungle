/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

#include "threads/vaddr.h"

/*------------------------- [P3] Memory Management --------------------------*/
static struct list frame_table;
// ? static struct list_elem *start; // frame_table을 순회하기 위한 첫 번째 원소

static unsigned hash_func (const struct hash_elem *e, void *aux UNUSED); // Implement hash_hash_func
static unsigned less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux); // Implement hash_less_func
static bool insert_page(struct hash *h, struct page *p);
static bool delete_page(struct hash *h, struct page *p);

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
/* 아래의 초기화 코드를 호출하여 가상 메모리 하위 시스템을 초기화한다. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	list_init(&frame_table); // frame_table에 대한 초기화
    // ? start = list_begin(&frame_table); // frame_table 탐색을 위한 첫 번째 원소
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
/* Initializer를 사용하여 보류 중인 페이지 개체를 만든다. 
 * 페이지를 생성하려면 직접 생성하지 말고 이 함수 또는 'vm_alloc_page'를 통해 생성한다. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT) // vm_type은 VM_ANON과 VM_FILE만 가능하다.

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* TODO: Insert the page into the spt. */
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	/*
	* va를 통해 page를 찾아야하는데, hash_find의 인자는 hash_elem이므로 이에 해당하는 hash_elem을 만들어준다.
	* 
	* 1. dummy page 생성(hash_elem 포함)
	* 2. va 매핑
	* 3. 해당 페이지와 같은 해시 값을 갖는 hash_elem을 찾는다.
	*/

	struct page *page = (struct page *)malloc(sizeof(struct page)); 
	// ↳ page를 새로 만들어주는 이유? : 해당 가상 주소에 대응하는 해시 값 도출을 위함
	//   page 생성 시, page_elem도 생성된다.
	struct hash_elem *e;
	
	page->va = pg_round_down(va); // 페이지를 만들고 가상 주소를 매핑해주는 작업 수행

	/* e와 같은 해시값을 갖는 page를 spt에서 찾은 다음 해당 hash_elem을 리턴 */
	e = hash_find(&spt->spt_hash, &page->hash_elem);
	free(page);
	
	if (e == NULL)
		return NULL;
	else
		return hash_entry(e, struct page, hash_elem); // e에 해당하는 page 리턴
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	return insert_page(&spt -> spt_hash, page);
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */
    struct thread *curr = thread_current();
    struct list_elem *frame_e;
    struct list_elem *e = start;

	// ? start가 없어도 되지 않나...?
	// ? start 없어도 되는 거 확인되면 struct frame에서도 지우기!
    // for (start = e; start != list_end(&frame_table); start = list_next(start)) {
    //     victim = list_entry(start, struct frame, frame_elem);
    //     if (pml4_is_accessed(curr->pml4, victim->page->va)) // access bit가 1이라면 true
    //         pml4_set_accessed (curr->pml4, victim->page->va, 0); // access bit를 초기화 해준다.
    //     else
    //         return victim;
    // }

    // for (start = list_begin(&frame_table); start != e; start = list_next(start)) {
    //     victim = list_entry(start, struct frame, frame_elem);
    //     if (pml4_is_accessed(curr->pml4, victim->page->va))
    //         pml4_set_accessed (curr->pml4, victim->page->va, 0);
    //     else
    //         return victim;
    // }

	// LRU 방식
	// frame table의 처음과 끝을 순회하면서 access bit가 0인 프레임을 찾는다.
	for (frame_e = list_begin(&frame_table); frame_e != list_end(&frame_table); frame_e = list_next(frame_e)) {
        victim = list_entry(frame_e, struct frame, frame_elem);
        if (pml4_is_accessed(curr->pml4, victim->page->va)) // access bit가 1이라면 true
            pml4_set_accessed (curr->pml4, victim->page->va, 0); // access bit를 초기화 해준다.
        else
            return victim;
    }
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	swap_out(victim->page);
	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
/* palloc()을 하고 frame을 가져온다. 사용 가능한 페이지가 없는 경우, 페이지를 내쫓고 해당 페이지를 반환한다.
 * 항상 유효한 주소를 반환한다. 즉, 유저 풀 메모리가 가득 차면 해당 함수는 사용 가능한 메모리 공간을 얻기 
 * 위해 프레임을 제거한다.*/
static struct frame *
vm_get_frame (void) {
	/* TODO: Fill this function. */
	struct frame *frame = (struct frame*)malloc(sizeof(struct frame)); 
	// frame 구조체를 위한 공간 할당한다.(작으므로 malloc으로 _Gitbook Memory Allocation 참조)

	frame->kva = palloc_get_page(PAL_USER); 
	// 유저 풀(실제 메모리)로부터 페이지 하나를 가져온다. 
	// ↳ 사용 가능한 페이지가 없을 경우, NULL 리턴
    if(frame->kva == NULL) { // 사용 가능한 페이지가 없는 경우
        frame = vm_evict_frame(); // swap out 수행 (frame을 내쫓고 해당 공간을 가져온다.)
        frame->page = NULL;

        return frame; // 무조건 유효한 주소만 리턴한다는 말이 통하는 이유 : swap out을 통해 공간 확보후, 리턴하기 떄문
    }
    list_push_back (&frame_table, &frame->frame_elem); // 새로 frame을 생성한 경우
    frame->page = NULL;

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
    struct thread *curr = thread_current();
	/* TODO: Fill this function */
	struct page *page = spt_find_page(&curr -> spt, va); // va에 페이지를 할당한다.
	if (page == NULL)
		return false;

	return vm_do_claim_page (page); // 해당 페이지에 프레임을 할당한다.
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame (); // 프레임 하나를 얻는다.

	/* Set links */
	frame->page = page; // 프레임의 페이지(가상)로 얻은 페이지를 연결해준다.
	page->frame = frame; // 페이지의 물리적 주소로 얻은 프레임을 연결해준다.

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
    struct thread *curr = thread_current();
	bool writable = page -> writable; // 해당 페이지의 R/W 여부
	pml4_set_page(curr->pml4, page->va, frame->kva, writable); // 현재 페이지 테이블에 가상 주소에 따른 frame 매핑

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->spt_hash, hash_func, less_func, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}

/*------------------------- [P3] Memory Management 추가 함수 --------------------------*/
/* [KAIST 35p.] vm_hash_func 
 * spt에 넣을 인덱스를 해쉬 함수를 돌려서 도출한다.
 * hash.c - 'hash_hash_func' 의 구현 형태
 * hash_bytes 설명 : Returns a hash of the SIZE bytes in BUF(hash_elem).
*/
static unsigned 
hash_func (const struct hash_elem *e, void *aux UNUSED) {
	const struct page *p = hash_entry(e, struct page, hash_elem); // hash 테이블이 hash_elem을 원소로 가지고 있으므로 페이지 자체에 대한 정보를 가져온다.
	return hash_bytes(&p->va, sizeof(p->va)); // 인덱스를 리턴해야하므로 hash_bytes를 리턴한다. 
}

/* [KAIST 35p.] vm_less_func 
 * 체이닝 방식의 spt를 구현하기 위한 함수
 * 해시 테이블 버킷 내의 두 페이지의 주소값 비교
 * hash.c - 'hash_less_func' 의 구현 형태
*/
static unsigned 
less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux) {
	const struct page *a_p = hash_entry(a, struct page, hash_elem);
	const struct page *b_p = hash_entry(b, struct page, hash_elem);
	return a_p->va < b_p->va;
}

/* [KAIST 34p.] insert_vme
 * spt 해시 테이블에 페이지를 삽입한다.
*/
static bool 
insert_page(struct hash *h, struct page *p) {
    if(!hash_insert(h, &p->hash_elem))
		return true;
	else
		return false;
}

/* [KAIST 34p.] delete_vme
 * spt 해시 테이블에서 페이지를 삭제한다.
*/
// ? 이거 왜 구현하는 거지...?
static bool 
delete_page(struct hash *h, struct page *p) {
	if(!hash_delete(h, &p->hash_elem))
		return true;
	else
		return false;
}
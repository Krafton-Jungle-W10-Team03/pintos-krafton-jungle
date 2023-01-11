/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

#include "threads/vaddr.h"


/*-------------------------[P3]frame table---------------------------------*/
static struct list frame_table;
/*-------------------------[P3]frame table---------------------------------*/

static unsigned hash_func (const struct hash_elem *e, void *aux UNUSED); // Implement hash_hash_func
static unsigned less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux); // Implement hash_less_func
static bool insert_page(struct hash *h, struct page *p);
static bool delete_page(struct hash *h, struct page *p);

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
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

/*
1. 커널이 새 page를 요청하면 vm_alloc_page_with_initializer 호출
	initializer는 페이지 구조를 할당하고 페이지 type에 따라 적절한 initalizer를 할당하고 
	SPT 페이지에 추가 후 userprogram으로 반환함
2. 해당 페이지에 access 호출이 오면 내용이 없는 페이지 임으로 page fault가 발생
	uninit_initialize 를 호출하고 이전에 설정한 initializer를 호출한다.
	page를 frame가 연결하고 lazy_load_segment 를 호출하여 필요한 데이터를 물리 메모리에 올린다.
*/

bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
	// ↳ upage라는 가상 메모리에 매핑되는 페이지 존재 x -> 새로 만들어야함
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		struct page* page = (struct page*)malloc(sizeof(struct page));
		
		// 페이지 타입에 따라 initializer가 될 초기화 함수를 매칭해준다.
        typedef bool (*initializer_by_type)(struct page *, enum vm_type, void *);
        initializer_by_type initializer = NULL;

		// switch-case 문 : VM_TYPE 이 enum이므로
        switch(VM_TYPE(type)) { // 페이지 타입에 따라 initiailizer를 설정한다.
            case VM_ANON:
                initializer = anon_initializer;
                break;
            case VM_FILE:
                initializer = file_backed_initializer;
                break;
		}

        uninit_new(page, upage, init, type, aux, initializer); // UNINIT 페이지 생성
        page->writable = writable; // page의 w/r 여부

		/* TODO: Insert the page into the spt. */
		return spt_insert_page(spt, page); // 새로 만든 페이지를 spt에 삽입한다.
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */
	/*
	* va를 통해 page를 찾아야하는데, hash_find의 인자는 hash_elem이므로 이에 해당하는 hash_elem을 만들어준다.
	* 
	* 1. dummy page 생성(hash_elem 포함)
	* 2. va 매핑
	* 3. 해당 페이지와 같은 해시 값을 갖는 hash_elem을 찾는다.
	*/
	/*-------------------------[P3]hash table---------------------------------*/
	// 인자로 받은 va에 해당하는 vm_entry를 검색 후 반환
	// 가상 메모리 주소에 해당하는 페이지 번호 추출 (pg_round_down())
	// hash_find() 함수를 이용하여 vm_entry 검색 후 반환
	
	struct page *page = (struct page*)malloc(sizeof(struct page));	// 임의의 페이지 만들어주기
	// ↳ page를 새로 만들어주는 이유? : 해당 가상 주소에 대응하는 해시 값 도출을 위함
	//   page 생성 시, hash_elem도 생성된다.
	struct hash_elem *e;
	
	// 인자로 받은 spt내에서 va를 키로 전달해 이를 갖는 page 리턴
	// hash_find로 부터 해당 page를 찾을 수 있음 (p->value : key, struct page: value)
	// spt의 hash 테이블 구조체를 인자로 넣어야 하는데 va만 인자로 받아왔기 때문에,
	// dummy 페이지를 만들고 해당 페이지의 가상주소를 va로 만듦
	// va가 속해있는 페이지 시작 주소를 갖는 page를 만듦(pg_round_down)
	page->va = pg_round_down(va);
	/* e와 같은 해시값을 갖는 page를 spt에서 찾은 다음 해당 hash_elem을 리턴 */
	e = hash_find(&spt->spt_hash, &page->hash_elem);
	free(page);

	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */

	return succ;
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
	/*-------------------------[P3]frame table---------------------------------*/
	// victim = list_entry(list_pop_front(&frame_table), struct frame, frame_elem);

	struct thread *curr = thread_current();
    struct list_elem *frame_e;

	// LRU 방식
	// frame table의 처음과 끝을 순회하면서 access bit가 0인 프레임을 찾는다.
	for (frame_e = list_begin(&frame_table); frame_e != list_end(&frame_table); frame_e = list_next(frame_e)) {
        victim = list_entry(frame_e, struct frame, frame_elem);
        if (pml4_is_accessed(curr->pml4, victim->page->va)) // access bit가 1이라면 true
            pml4_set_accessed (curr->pml4, victim->page->va, 0); // access bit를 초기화 해준다.
        else
            return victim;
    }
	/*-------------------------[P3]frame table---------------------------------*/

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	/*-------------------------[P3]frame table---------------------------------*/
	if(victim->page != NULL){
		swap_out(victim -> page);
		return victim;
	}
	/*-------------------------[P3]frame table---------------------------------*/

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */

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
	// struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	
	/* page_fault로 부터 넘어온 인자
	 * not_present : 페이지 존재 x (bogus fault)
	 * user : 유저에 의한 접근(true), 커널에 의한 접근(false)
	 * write : 쓰기 목적 접근(true), 읽기 목적 접근(false)
	*/
	// ! Stack Growth 에서 다시 보기
	// page fault 주소에 대한 유효성 검증
	// 커널 가상 주소 공간에 대한 폴트 처리 불가, 사용자 요청에 대한 폴트 처리 불가
	if (is_kernel_vaddr (addr) && user) // real fault
		return false;

    void *rsp_stack = is_kernel_vaddr(f->rsp) ? thread_current()->rsp_stack : f->rsp;
    if (not_present){
        if (!vm_claim_page(addr)) return false;
        else return true;
    }
	
	// return vm_do_claim_page (page);
	return false;
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
	struct page *page = NULL;
	/* TODO: Fill this function */
	struct page *page = spt_find_page(&curr -> spt, va); // va에 해당하는 페이지가 존재하는지 확인한다.
	if (page == NULL)
		return false;

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
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

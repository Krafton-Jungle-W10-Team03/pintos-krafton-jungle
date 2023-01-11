/* uninit.c: Implementation of uninitialized page.
 *
 * All of the pages are born as uninit page. When the first page fault occurs,
 * the handler chain calls uninit_initialize (page->operations.swap_in).
 * The uninit_initialize function transmutes the page into the specific page
 * object (anon, file, page_cache), by initializing the page object,and calls
 * initialization callback that passed from vm_alloc_page_with_initializer
 * function.
 * */
/* uninit.c: 초기화되지 않은 페이지에 대한 구현이다.
 * 모든 페이지가 uninit page로 생성된다.
 * 첫 번째 페이지 폴트가 났을 때, 핸들러 체인은 uninit_initiailize(page->operations.swap_in)를 호출한다.
 * uninit_initialize 함수는 페이지 개체를 특정 페이지 개체(anon, file, page_cache)로 변환하고 
 * vm_alloc_page_with_initializer 함수로 부터 전달된 초기화 콜백을 호출한다.
*/

#include "vm/vm.h"
#include "vm/uninit.h"

static bool uninit_initialize (struct page *page, void *kva);
static void uninit_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations uninit_ops = {
	.swap_in = uninit_initialize,
	.swap_out = NULL,
	.destroy = uninit_destroy,
	.type = VM_UNINIT,
};

/* DO NOT MODIFY this function */
void
uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *)) {
	ASSERT (page != NULL);

	*page = (struct page) {
		.operations = &uninit_ops,
		.va = va,
		.frame = NULL, /* no frame for now */
		.uninit = (struct uninit_page) {
			.init = init,
			.type = type,
			.aux = aux,
			.page_initializer = initializer,
		}
	};
}

/* Initalize the page on first fault */
static bool
uninit_initialize (struct page *page, void *kva) {
	struct uninit_page *uninit = &page->uninit; // page 구조체 내의 union 내의 uninit_page struct(union 구조체내의 값은 1개만 사용 가능)

	/* Fetch first, page_initialize may overwrite the values */
	// 가져오기 먼저하고, page_initialize 값을 덮어쓸 수 있음
	vm_initializer *init = uninit->init;
	void *aux = uninit->aux;

	/* TODO: You may need to fix this function. */
	return uninit->page_initializer (page, uninit->type, kva) &&
		(init ? init (page, aux) : true);
}

/* Free the resources hold by uninit_page. Although most of pages are transmuted
 * to other page objects, it is possible to have uninit pages when the process
 * exit, which are never referenced during the execution.
 * PAGE will be freed by the caller. */
/* uninit_page가 보유한 리소스를 확보, 대부분의 페이지가 다른 페이지 개체로 전송되지만
 * 프로세스가 종료될 때 실행중에 참조되지 않는 uninit_page가 있을 수 있음
 * 호출자가 페이지를 해제함 */
static void
uninit_destroy (struct page *page) {
	struct uninit_page *uninit UNUSED = &page->uninit;
	/* TODO: Fill this function.
	 * TODO: If you don't have anything to do, just return. */
	if(page->uninit.aux != NULL)
		free(page->uninit.aux);
	return;
}

#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "filesys/off_t.h"

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

/*-------------------------[P3]Anonoymous page---------------------------------*/
struct segment_aux {
    struct file *file;
    off_t offset;
    size_t page_read_bytes;
};

bool setup_stack (struct intr_frame *if_);
bool lazy_load_segment (struct page *page, void *aux);
/*-------------------------[P3]Anonoymous page---------------------------------*/

#endif /* userprog/process.h */

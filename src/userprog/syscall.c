#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
	/* HUANG Implementation */
#include <list.h>
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "threads/init.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "devices/input.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "devices/shutdown.h"
	/* == HUANG Implementation */

static void syscall_handler (struct intr_frame *);

	/* HUANG Implementation */
//process id: a one-to-one mapping to tid_t, so that the same values in both identify the same process
typedef int pid_t;

struct openFile {
	tid_t f_tid;	//the thread who owns the file
	int fd;		//file's fd
	struct list_elem elem;	//to be linked in open_file_list
	struct file *file;	//the open file
};

static struct list open_file_list;	//keep track of all open files
static struct lock f_lock;		//for synchronization

// system call functions
static void halt (void);
void exit (int status);
//static void exit (int status);
static pid_t exec (const char *cmd_line);
static int wait (pid_t pid);
static bool create (const char*file, unsigned initial_size);
static bool remove (const char *file);
static int open (const char *file);
static int filesize (int fd);
static int read (int fd, void *buffer, unsigned size);
static int write (int fd, const void *buffer, unsigned size);
static void seek (int fd, unsigned position);
static unsigned tell (int fd);
static void close (int fd);
static void close_all_file_by_thread(tid_t cur);

//my own function
//static bool is_save_mem_access (const void *ptr);
bool is_save_mem_access (const void *ptr);
static struct openFile *my_get_file(int fd);	//given fd, return pointer to openFile struct

static int total_fd = 1;	//keep track of the total fd, should start from 2
	/* == HUANG Implementation */

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  
	/* HUANG Implementation */
  list_init(&open_file_list);
  lock_init(&f_lock);
	/* == HUANG Implementation */
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
	/* Original code
  printf ("system call!\n");	*/

	/* HUANG Implementation */

  uint32_t *local_esp;		//local variable to keep track of f->esp. maybe want to change it to globle later
  local_esp = f->esp;

  //check if esp if valid or not. 
  if (*local_esp < SYS_HALT || *local_esp > SYS_INUMBER) {
	goto THE_END;
  }

  if (!is_save_mem_access(local_esp) || !is_save_mem_access(local_esp + 1) ||
	!is_save_mem_access(local_esp + 2) || !is_save_mem_access(local_esp + 3)) {
	exit(-1);	
  }

  
  switch (*local_esp) {
    	case SYS_HALT:
    	  	halt ();
    	  	break;
    	case SYS_EXIT:
    	  	exit (*(local_esp + 1));
    	  	break;
    	case SYS_EXEC:
    	  	f->eax = exec ((char *) *(local_esp + 1));
    	  	break;
    	case SYS_WAIT:
    	  	f->eax = wait (*(local_esp + 1));
    	  	break;
    	case SYS_CREATE:
    	  	f->eax = create ((char *) *(local_esp + 1), *(local_esp + 2));
    	  	break;
    	case SYS_REMOVE:
    	  	f->eax = remove ((char *) *(local_esp + 1));
    	  	break;
    	case SYS_OPEN:
    	  	f->eax = open ((char *) *(local_esp + 1));
    	  	break;
    	case SYS_FILESIZE:
    	  	f->eax = filesize (*(local_esp + 1));
    	  	break;
    	case SYS_READ:
    	  	f->eax = read (*(local_esp + 1), (void *) *(local_esp + 2), *(local_esp + 3));
    	  	break;
    	case SYS_WRITE:
    	  	f->eax = write (*(local_esp + 1), (void *) *(local_esp + 2), *(local_esp + 3));
    	  	break;
    	case SYS_SEEK:
    	  	seek (*(local_esp + 1), *(local_esp + 2));
    	  	break;
    	case SYS_TELL:
    	  	f->eax = tell (*(local_esp + 1));
    	  	break;
    	case SYS_CLOSE:
    	  	close (*(local_esp + 1));
    	  	break;
    	default:
    	  	break;
  }

	/* == HUANG Implementation */


	/* Original code
  thread_exit ();	*/

	/* HUANG Implementation */
  return;

THE_END:
  thread_exit ();	
	/* == HUANG Implementation */
}


/********** Start Implementing System Call Functions From here *****************/

	/* HUANG Implementation */	//Begin implement system call functions
	
/*
 * Terminates Pintos by calling shutdown_power_off() (declared in devices/shutdown.h)
 */
static void halt (void) {
	shutdown_power_off();
}

/*
 * Terminates the current user program, returning status to the kernel.
 * If the process's parent waits for it, this is the status that will be returned
 * Conventionally, a status of 0 indicates success and nonzero values indicate errors
 */
//static void exit (int status) {
void exit (int status) {
        /* XIN Implementation */
	struct thread *cur = thread_current();
	/* Kai Implementation */
        close_all_file_by_thread(cur->tid);
	
	if (cur->parent != NULL) {
        	lock_acquire(&cur->parent->l);
		struct list children = cur->parent->children;
      		struct list_elem *e;
      		for (e = list_begin (&children); e != list_end (&children); e = list_next (e)) {
			struct child *child = list_entry (e, struct child, childelem);
			if (cur->tid == child->pid) {
				child->isKilled = 0;
				child->exit_status = status;
				break;
			}
        	}
                lock_release(&cur->parent->l);
        }

	printf("%s: exit(%d)\n", cur->name, status);
	
        /* == XIN Implementation */
	thread_exit ();	
}


/*
 * Runs the executable whose name is given in cmd_line, passing any given arguments, 
 * and returns the new process's program id (pid). 
 * Must return pid -1, which otherwise should not be a valid pid, if the program cannot load or run for any reason. 
 * Thus, the parent process cannot return from the exec until it 
 * knows whether the child process successfully loaded its executable. 
 * You must use appropriate synchronization to ensure this.
 */
static pid_t exec (const char *cmd_line) {
	/* XIN Implementation */

	struct thread *parent = thread_current();

	//printf("\nparent name: %s\n", parent->name);
	pid_t pid = process_execute(cmd_line);
	if (pid == TID_ERROR) return -1;

	struct list_elem *e;
      	struct child *child = NULL;
	for (e = list_begin (&parent->children); e != list_end (&parent->children); e = list_next (e)) {
      		child = list_entry (e, struct child, childelem);
      		if (child->pid == pid) break;
    	}

	if (child == NULL) return -1;
	//printf("exec parent name %s child pid %d load %d \n", parent->name, pid, child->loadSuccess);

	while (child->loadSuccess == -1) {
		//printf("\nbefore sema_donw\n");
		sema_down(&parent->sema_load);
		//printf("\nafter sema_down\n");
	}

	lock_acquire(&parent->l);
	int return_val =  child->loadSuccess ? pid : -1;
	lock_release(&parent->l);
	
	//printf("\n%s exec finished\n", parent->name);
	return return_val;

	/* == XIN Implementation */
}

/*
 * Waits for a child process pid and retrieves the child's exit status.
 * We suggest that you implement process_wait() according to the comment at the top 
 * of the function and then implement the wait system call in terms of process_wait()
 * Implementing this system call requires considerably more work than any of the rest.
 */
static int wait (pid_t pid) {
	return process_wait(pid);
}

/*
 * Creates a new file called file initially initial_size bytes in size. 
 * Returns true if successful, false otherwise. 
 * Creating a new file does not open it: opening the new file is a 
 * separate operation which would require a open system call.
 */
static bool create (const char*file, unsigned initial_size) {
	bool successful;

	if (file == NULL) {
		exit(-1);
	}

	//has to check if char *file is a valid pointer. If not, exit(-1) 
	if (!is_save_mem_access(file)) {
		exit(-1);
	}

	lock_acquire(&f_lock);
	successful = filesys_create(file, initial_size);	//refer to "src/filesys/filesys.c"
	lock_release(&f_lock);

	return successful;
}

/*
 * Deletes the file called file. Returns true if successful, false otherwise.
 * A file may be removed regardless of whether it is open or closed, and removing an open file does not close it
 */
static bool remove (const char *file){
	bool successful;

	if (!file) {
		return false;
	}

	//has to check if char *file is a valid pointer. If not, exit(-1) 
	if (!is_save_mem_access(file)) {
		exit(-1);
	}

	lock_acquire(&f_lock);
	successful = filesys_remove(file);	//refer to "src/filesys/filesys.c"
	lock_release(&f_lock);

	return successful;
}

/*
 * Opens the file called file. Returns a nonnegative integer handle 
 * called a "file descriptor" (fd), or -1 if the file could not be opened.
 * File descriptors numbered 0 and 1 are reserved for the console: 
 * fd 0 (STDIN_FILENO) is standard input, fd 1 (STDOUT_FILENO) is standard output.
 * The open system call will never return either of these file descriptors ...
 */
static int open (const char *file){
	struct openFile *open_file;
	struct file *local_file;
	
	int fd = -1;

	//has to check if char *file is a valid pointer. If not, exit(-1) 
	if (!is_save_mem_access(file)) {
		exit(-1);
	}

	lock_acquire(&f_lock);
	
	local_file = filesys_open(file);
	if (file == NULL || local_file == NULL) {
		fd = -1;
		goto THE_END;
	}

	open_file = malloc(sizeof(struct openFile));	
	total_fd++;
	open_file->f_tid = thread_current()->tid;
	open_file->fd = total_fd; 
	open_file->file = local_file;
	fd = open_file->fd;
	list_push_back(&open_file_list, &open_file->elem);

THE_END:
	lock_release(&f_lock);
	return fd;
}

/*
 * Returns the size, in bytes, of the file open as fd
 * if no such fd, return -1 instead
 */
static int filesize (int fd){
	struct openFile *open_file;
	int f_size = -1;
	
	lock_acquire(&f_lock);

	open_file = my_get_file(fd);

	if (open_file == NULL) {
		goto THE_END;
	}	

	f_size = file_length(open_file->file);

THE_END:
	lock_release(&f_lock);
	return f_size;
}

/*
 * Reads size bytes from the file open as fd into buffer. Returns the number of bytes actually read
 * (0 at end of file), or -1 if the file could not be read (due to a condition other than end of file). 
 * Fd 0 reads from the keyboard using input_getc().
 */
static int read (int fd, void *buffer, unsigned size){
	struct openFile *open_file;
	int read_size = -1;
	unsigned int i;

	lock_acquire(&f_lock);

	if (fd == STDIN_FILENO) {
		for (i = 0; i< size; i++) {
			*(uint8_t *)(buffer + i) = input_getc();
		}
		read_size = size;
		goto THE_END;
	} else if (fd == STDOUT_FILENO) {
		goto THE_END;
	} else if (is_save_mem_access(buffer) && is_save_mem_access(buffer + size)) {
		open_file = my_get_file(fd);
		if (open_file == NULL) {
			goto THE_END;
		}
		read_size = file_read(open_file->file, buffer, size);
	} else {
		lock_release(&f_lock);
		exit(-1);
	}

THE_END:
	lock_release(&f_lock);
	return read_size;
}

/*
 * Writes size bytes from buffer to the open file fd. 
 * Returns the number of bytes actually written, which may be 
 * less than size if some bytes could not be written. ...
 */
static int write (int fd, const void *buffer, unsigned size) {
	struct openFile *open_file;
	int write_size = -1;
	
	lock_acquire(&f_lock);
	//write the console (for "msg")
	if (!is_save_mem_access(buffer) || !is_save_mem_access(buffer + size)) {
		exit(-1);
	}

	if (fd == STDOUT_FILENO) {	//write to console
		putbuf(buffer, size);
		write_size = size;
	} else if (fd == STDIN_FILENO){
		goto THE_END;
	} else if (is_save_mem_access(buffer) && is_save_mem_access(buffer + size)) {
		open_file = my_get_file(fd);
		if (open_file == NULL) {
			goto THE_END;
		}
		write_size = file_write(open_file->file, buffer, size);
	} else {
		lock_release(&f_lock);
		exit(-1);
	}

THE_END:
	lock_release(&f_lock);
	return write_size;
}

/*
 * Changes the next byte to be read or written in open file fd to position, 
 * expressed in bytes from the beginning of the file. 
 * (Thus, a position of 0 is the file's start.) ... 
 */
static void seek (int fd, unsigned position) {
	struct openFile *open_file;
	lock_acquire(&f_lock);

	open_file = my_get_file(fd);
	if (open_file != NULL) {
		file_seek(open_file->file, position);
	}
	lock_release(&f_lock);

	return;
}

/*
 * Returns the position of the next byte to be read or written in open file fd, 
 * expressed in bytes from the beginning of the file
 */
static unsigned tell (int fd) {
	off_t pos;
	struct openFile *open_file;
	lock_acquire(&f_lock);

	open_file = my_get_file(fd);
	if (open_file != NULL) {
		pos = file_tell(open_file->file);
	}

	lock_release(&f_lock);
	return pos;	
}

/*
 * Closes file descriptor fd. Exiting or terminating a process implicitly 
 * closes all its open file descriptors, as if by calling this function for each one
 */
static void close (int fd ) {
	struct openFile *open_file;
	struct openFile *open_file_toClose;
	lock_acquire(&f_lock);	

	open_file = my_get_file(fd);

	if (open_file != NULL && open_file->f_tid == thread_current()->tid) {
		
		struct list_elem *e;
		for (e = list_begin (&open_file_list); e != list_end (&open_file_list); e = list_next (e)){ 
		  	open_file_toClose = list_entry (e, struct openFile, elem);
			if (open_file_toClose->fd == fd) {
				list_remove(e);
				file_close(open_file_toClose->file);
				free(open_file_toClose);
				break;
			}
		}
	}
	
	lock_release(&f_lock);
	return;
}

	/* == HUANG Implementation */	//End implementing system call functions

	/* KAI Implementation */ // cur_t = current thread t_id
static void close_all_file_by_thread(tid_t cur_t){
        struct list_elem *e, *next_e;
	struct openFile *open_file;
	
	for (e=list_begin(&open_file_list); e!=list_end(&open_file_list);){
		next_e=list_next(e);
		open_file = list_entry(e, struct openFile, elem);
		if(open_file->f_tid == cur_t){
			list_remove(e);
			file_close(open_file->file);
			free(open_file);
		}
		e = next_e;
	}

	
	return;
}
	/* == KAI Implementation */


/********** End Implementing System Call Functions *****************/
/********** Start Implementing my own Functions From here *****************/

	/* HUANG Implementation */	//Begin implementing my own functions
/*
 * check if user mem_access is save or not
 */
bool is_save_mem_access (const void *u_ptr) {
	bool flag = false;

	struct thread *cur = thread_current();	//get the current thread

	if (u_ptr != NULL && is_user_vaddr(u_ptr)) {	//check if it's a ptr pointing at user's range. refer to "src/threads/vaddr.h" 
		flag = pagedir_get_page (cur->pagedir, u_ptr) != NULL;	//refer to "src/userprog/pagedir.c" 
	}

	return flag;
}


/*
 * given fd, return pointer to openFile struct 
 * if no such fd open, return NULL
 */
static struct openFile *my_get_file(int fd) {
	struct list_elem *e;
	struct openFile *open_file;	

	for (e = list_begin (&open_file_list); e != list_end (&open_file_list);
	     e = list_next (e))
	  { 
	  	open_file = list_entry (e, struct openFile, elem);
		if (open_file->fd == fd) {
			return open_file;
		}
	  }
	return NULL;
}	



	/* == HUANG Implementation */	//End implementing my own functions

#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "filesys/filesys.h"

/* HUANG proj4 */
#include "threads/palloc.h"
#include <stdio.h>
#include "devices/block.h"
#include <string.h>

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 64
#endif

// buffer_info tells us what info is saved on each buffer
struct buffer_info{
    //int buffer_no;
    struct inode * buffer_inode; // maybe we can delete this
    block_sector_t sector_num; // sector of data original saved on disk (unique)
		   // put to -1 as initial value, which means this has not been occupied
    bool dirty;    // dirty == 1: cache sector and disk sector are not consistant; vice versa
    	//recentlyUsed: whenever sector is read/write from buffer cache, set it to true;
	//in lookup_evict(), recentlyUsed is set to false by clock algorithm
    bool recentlyUsed;
    int priority; // choose some buffer to evict, need priority scheduling
};



void buffer_cache_init (void); 
void inode_free_all(void);
int lookup_sector(block_sector_t sector_num); 
int lookup_empty_buffer(void);
int lookup_evict(void);
int buffer_evict(int evict_indx);
uint8_t *get_buffer_vaddr(void);
struct buffer_info *get_buffer_info_array(void);

void write_via_cache(struct inode *inode, const uint8_t *buffer, 
		block_sector_t sector_idx, int sector_ofs, int chunk_size);

void read_via_cache(struct inode *inode, uint8_t *buffer, 
		block_sector_t sector_idx, int sector_ofs, int chunk_size);


#endif

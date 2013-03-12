#include "cache.h"

	//buffer_vaddr is the pointer at buffer cache (start addr of 8 continuous pages)
static uint8_t *buffer_vaddr = NULL;
	//keep track of all buffer info
static struct buffer_info buffer_info_array[BUFFER_SIZE]; // can be malloc() as well


void buffer_cache_init (void) {
  buffer_vaddr = palloc_get_multiple(PAL_ZERO | PAL_USER, 8);
  if(buffer_vaddr == NULL)
	PANIC("BUFFER PALLOC FAILED!");
  int i;
  for (i=0; i<BUFFER_SIZE; i++){
	buffer_info_array[i].sector_num = -2; 
	buffer_info_array[i].dirty = false;
	buffer_info_array[i].recentlyUsed = false;
  }
}


// ------------------------------------------------
void inode_free_all(void){

                //go through buffer cache, write back the sectors belong to inode
        int i = 0;
        for (; i < BUFFER_SIZE; i++) {
		if (buffer_info_array[i].dirty) {
                	if (buffer_info_array[i].sector_num != -2 || buffer_info_array[i].buffer_inode != NULL) {
                	        int sector_idx = buffer_info_array[i].sector_num;
                	        void *buffer_cache_start_addr = buffer_vaddr + i * BLOCK_SECTOR_SIZE;
                	        block_write (fs_device, sector_idx, buffer_cache_start_addr);

                	        buffer_info_array[i].sector_num = -2;
                	        buffer_info_array[i].buffer_inode = NULL;
                	        buffer_info_array[i].dirty = false;
                	        buffer_info_array[i].recentlyUsed = false;
                	}
		}
        }

	palloc_free_multiple(buffer_vaddr,8);
}
// ------------------------------------------------
	//check if sector_num exists in buffer cache
	//return buffer_info_array[] indx
	//if none exists, return -1
int lookup_sector(block_sector_t sector_num) { // check to see if this sector has been read before
	int i;
	for (i=0; i<BUFFER_SIZE; i++)
		if (buffer_info_array[i].sector_num == sector_num){
			/*
			buffer_info_array[i].dirty = true;
			printf("FOUND cache_dirty %d buffer_info.[sector] %d\n", 
				i, buffer_info_array[i].sector_num);
			*/
			return i;
		}	
	return  -1;
}
// ------------------------------------------------
	//look for an empty buffer cache sector
	//return buffer_info_array[] indx
	//if none exists, return -1
int lookup_empty_buffer(void) {
	int i;
	for (i=0; i<BUFFER_SIZE; i++) { 
		if (buffer_info_array[i].sector_num == -2) 
			return i;
	}
	return -1; // no empty buffer, get eviction and begin to work, 
}
// ------------------------------------------------
	//1. check if is recentlyUsed, if it is , set to to false,
	//	until find a buffer cache sector that is not recentlyUsed
	//2. after get the one not recentlyUsed, check if it is dirty
	//3. if not dirty, return the indx,
	//4. else if dirty, write it back to disk,
	//5. jump to 1
	//so, the return result should always be a valid buffer cache indx
int lookup_evict(void){
//just for test
/*
	int sector_idx = buffer_info_array[0].sector_num;
	int buffer_cache_start_addr = buffer_vaddr + 0 * BLOCK_SECTOR_SIZE;
        block_write (fs_device, sector_idx, buffer_cache_start_addr);
	return 0;
*/
// == just for test

	int i = 0;
/*
	for (i=0;i<BUFFER_SIZE;i++) {
		if(buffer_info_array[i].dirty == false)
			return i;
	}
*/
		//1. check if is recentlyUsed, if it is , set to to false,
	while (buffer_info_array[i].recentlyUsed == true) {
		//mark it as not recenlty used 
		buffer_info_array[i].recentlyUsed = false;

	THE_MIDDLE:
		if (i == 63) {
			i = 0;
		} else {
			i++;
		}
	}

	if (buffer_info_array[i].dirty == false) {
		return i;
	} else {
		//4. else if dirty, write it back to disk,
		//write the cache sector back to disk 
		int sector_idx = buffer_info_array[i].sector_num;
		void *buffer_cache_start_addr = buffer_vaddr + i * BLOCK_SECTOR_SIZE;
          	block_write (fs_device, sector_idx, buffer_cache_start_addr);
			//mark buffer_info_arr.dirty as clean since just wrote back
		buffer_info_array[i].dirty = false;
	
		goto THE_MIDDLE;
	}
}
// ------------------------------------------------
	//real eviction happens upstairs 
int buffer_evict(int evict_indx){

	buffer_info_array[evict_indx].sector_num = -2;

	return evict_indx;
} 



uint8_t *get_buffer_vaddr(void) {
	return buffer_vaddr;
}

struct buffer_info *get_buffer_info_array(void) {
	return buffer_info_array;
}

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
                	//if (buffer_info_array[i].sector_num != -2 || buffer_info_array[i].buffer_inode != NULL) {
                	if (buffer_info_array[i].sector_num != -2) {
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


/*
 * buffer is the start address in memory to be written from
 * if used in inode_write_at, buffer = 'buffer' + bytes_written
 * sector_ofs is the start point to actually write the buffer cache
 * chunk_size is the actual size to be written to buffer cache
 */
void write_via_cache(struct inode *inode, const uint8_t *buffer, 
		block_sector_t sector_idx, int sector_ofs, int chunk_size) {

	//lookup buffer and check if sector_indx exits in buffer cache
      int buffer_arr_indx = lookup_sector(sector_idx);	//lookup result indx for a valid sector in cache
	if (buffer_arr_indx > BUFFER_SIZE) {
		PANIC("lookup_sector wrong\n");
	}
      if (buffer_arr_indx >= 0){ 
		//if yes, just write buffer cache
	void *buffer_cache_start_addr = buffer_vaddr + buffer_arr_indx * BLOCK_SECTOR_SIZE + sector_ofs;
        memcpy (buffer_cache_start_addr, (void *)buffer, chunk_size);
      } else {			//the sector_idx is not in buffer cache
	//check if empty sector exists in buffer cache
	int buffer_indx = lookup_empty_buffer();
	if (buffer_indx == -1) {
		//if no, evict sector from buffer cache, and get its indx 
		//choose a sector to evict
		//lookup_evict contains 'clock algorithm'
		//lookup_evict() takes care of case when buffer_evict_indx == -1
		int buffer_evict_indx = lookup_evict(); 
			//buffer_evict_indx should be a valid indx
		buffer_evict_indx = buffer_evict(buffer_evict_indx); 
		buffer_indx = buffer_evict_indx;
	}
		//buffer_cache_start_addr is the start addr of buffer cache sector
	void *buffer_cache_start_addr = buffer_vaddr + buffer_indx * BLOCK_SECTOR_SIZE;
		//get sector from disk to buffer cache
		//copy the sector from disk to buffer cache

      	if (sector_ofs != 0 || chunk_size != BLOCK_SECTOR_SIZE) {
         		// Write partial sector cache, need to fetch from disk first. 
        	block_read (fs_device, sector_idx, buffer_cache_start_addr);
	}
		//change buffer_cache_start_addr to the offset of buffer cache sector
	buffer_cache_start_addr += sector_ofs;
		//write sector to buffer cache 
		//copy the sector from buffer cache to buffer_read
        memcpy (buffer_cache_start_addr, (void *)buffer, chunk_size);

		//fill the info into buffer_info_array
	buffer_info_array[buffer_indx].sector_num = sector_idx;
	buffer_info_array[buffer_indx].buffer_inode = inode;
	buffer_info_array[buffer_indx].dirty = true;
	buffer_info_array[buffer_indx].recentlyUsed = true;

      }
}

/*
 * buffer is the start address in memory to be read from
 * if used in inode_read_at, buffer = 'buffer' + bytes_read
 * sector_ofs is the start point to actually read from the buffer cache
 * chunk_size is the actual size to be read to buffer cache
 */
void read_via_cache(struct inode *inode, uint8_t *buffer, 
		block_sector_t sector_idx, int sector_ofs, int chunk_size) {

	//printf("sector_idx %d, size %d\n", sector_idx, size);
      int buffer_arr_indx = lookup_sector(sector_idx);	//lookup result indx for a valid sector in cache
	if (buffer_arr_indx > BUFFER_SIZE) {
		PANIC("lookup_sector wrong\n");
	}
//	printf("sector_idx %d\n", sector_idx);
      if (buffer_arr_indx >= 0){ 
//	printf("indx %d, sector_idx %d size %d\n", buffer_arr_indx, sector_idx, size);
	
	void *buffer_cache_start_addr = buffer_vaddr + buffer_arr_indx * BLOCK_SECTOR_SIZE + sector_ofs;
	memcpy((void *)buffer, buffer_cache_start_addr, chunk_size);
      } else {			//the sector_idx is not in buffer cache
	//printf("new start\n");
	int buffer_indx = lookup_empty_buffer();
	if (buffer_indx == -1) {
		//choose a sector to evict
		//lookup_evict contains 'clock algorithm'
		//lookup_evict() takes care of case when buffer_evict_indx == -1
		int buffer_evict_indx = lookup_evict(); 
			//buffer_evict_indx should be a valid indx
		buffer_evict_indx = buffer_evict(buffer_evict_indx); 
		buffer_indx = buffer_evict_indx;
		//printf("evicting!\n");
	}
		//buffer_cache_start_addr is the start addr of buffer cache sector
	void *buffer_cache_start_addr = buffer_vaddr + buffer_indx * BLOCK_SECTOR_SIZE;
		//copy the sector from disk to buffer cache
        block_read (fs_device, sector_idx, buffer_cache_start_addr);
		//change buffer_cache_start_addr to the offset of buffer cache sector
	buffer_cache_start_addr += sector_ofs;
		//copy the sector from buffer cache to buffer_read
        memcpy ((void *)buffer, buffer_cache_start_addr, chunk_size);

		//fill the info into buffer_info_array
	buffer_info_array[buffer_indx].sector_num = sector_idx;
	buffer_info_array[buffer_indx].buffer_inode = inode;
	buffer_info_array[buffer_indx].dirty = false;
	buffer_info_array[buffer_indx].recentlyUsed = true;
      }

}

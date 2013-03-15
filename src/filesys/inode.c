#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
/* KAI proj4 */
#include "cache.h"
/* == KAI proj4 */

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define BUFFER_SIZE 64
#define IDISK_SIZE 126

/* KAI IMPLEMENTATION */
allocate_disk_space_for_inode_write(struct inode * inode, off_t offset, off_t size);
/* == KAI IMPLEMENTATION */

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
// inode_disk -- level 0 (DIRECT)
// extedned (second last element) -- level 1 (INDIRECT)
// indirect-extended (last element) -- level 2 (level 2 points to level 1) (DOUBLY INDIRECT)
struct inode_disk
  {
    /* our proj4 */

    // If right now at level 0 (DIRECT)
    // each array element saves disk sector number where save the file 
    // the second last array element saves sector number of level 1 struct 'inode_disk'
    // the last array element saves sector number of level 2 struct 'inode_disk'

    // If at level 1 (INDIRECT)
    // each array element saves disk sector number where save the file 
   
    // If at level 2 (DOUBLY-INDIRECT)
    // each  array element saves sector number of level 1 struct 'inode_disk'
     
    uint32_t file_sector_index[IDISK_SIZE];               // each array element saves disk sector number where save the file //
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
   
    /* == our project4 */

    /* original code
    block_sector_t start;               // First data sector. //
    off_t length;                       // File size in bytes. /
    unsigned magic;                     // Magic number. /
    uint32_t unused[125];               // Not used. /
    */
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    /* original code
    struct inode_disk data;             // Inode content. /
    */

    /* our proj4 */
    off_t length; 			/* the total length of the file */
    /* == our proj4 */   
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
// TODO: check file sector from our new structure
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);

  /* our proj4 */
  
  /* psuedo-code
   *  ARRAY means file_sector_index[IDISK_SIZE] in inode_disk 
   *
   * 1. find which array element to lookup in LEVEL 0 inode_disk structure,  determine by 'pos/BLOCK_SECTOR_SIZE'
   * 2. if array element index is smaller than (IDISK_SIZE-2), return array element value (sector index)
   * 3. if array element index is between [IDISK_SIZE-2, 2*IDISK_SIZE-3], go to LEVEL1 inode_disk_structure and return array element value (sector index)
   * 4. if array element index is larger than (2*IDISK_SIZE-3), go to doubly indirect LEVEL2 inode_disk_structure, and then go to its indirct LEVEL1 inode_
        disk_structure
   * In detail for LEVEL 2 structure
   * [2*IDISK_SIZE-2 + 0*IDISK_SIZE, 2*IDISK_SIZE-2 + 1*IDISK_SIZE-1]  
   * [2*IDISK_SIZE-2 + 1*IDISK_SIZE, 2*IDISK_SIZE-2 + 2*IDISK_SIZE-1]  
   * [2*IDISK_SIZE-2 + 2*IDISK_SIZE, 2*IDISK_SIZE-2 + 3*IDISK_SIZE-1]  
   * ....
   * [2*IDISK_SIZE-2 + (IDISK_SIZE-1)*IDISK_SIZE, 2*IDISK_SIZE-2 + IDISK_SIZE*IDISK_SIZE-1]  
   * 
   */
  if (pos < inode->length){
    struct inode_disk level0_inode_disk;
    read_via_cache(NULL, (void *)&level0_inode_disk, inode->sector, 0, BLOCK_SECTOR_SIZE);
    // step 1. get array element index
    int total_index_of_sector_num = pos/BLOCK_SECTOR_SIZE;

    if (total_index_of_sector_num < IDISK_SIZE-2){
	// step 2. check in LEVEL 0 structure
	block_sector_t result = level0_inode_disk.file_sector_index[total_index_of_sector_num];
	return result;
    }
    else if(total_index_of_sector_num <= (2*IDISK_SIZE-3) ){
	// step 3. check in LEVEL 1 structure
	int level1_index = total_index_of_sector_num - (IDISK_SIZE-2);

	struct inode_disk level1_inode_disk;
	block_sector_t level0_second_last_val = level0_inode_disk.file_sector_index[IDISK_SIZE-2];
        read_via_cache(NULL, (void *)&level1_inode_disk, level0_second_last_val, 0, BLOCK_SECTOR_SIZE);
	block_sector_t result = level1_inode_disk.file_sector_index[level1_index];
	return result; 	
    } else if(total_index_of_sector_num <= ((IDISK_SIZE+2)*IDISK_SIZE-3) ){
	//step 4. check in LEVEL2 -> LEVEL1 structure
	int total_index_left = total_index_of_sector_num - (2*IDISK_SIZE-2);
	int level2_index = total_index_left/IDISK_SIZE;
	int level1_index = total_index_left%IDISK_SIZE;

	block_sector_t level0_last_val = level0_inode_disk.file_sector_index[IDISK_SIZE-1];	
	struct inode_disk level2_inode_disk;
        read_via_cache(NULL, (void *)&level2_inode_disk, level0_last_val, 0, BLOCK_SECTOR_SIZE);
	
	block_sector_t level2_val = level2_inode_disk.file_sector_index[level2_index];
	struct inode_disk level1_inode_disk;
        read_via_cache(NULL, (void *)&level1_inode_disk, level2_val, 0, BLOCK_SECTOR_SIZE);
	
	// result is actually level1_val
	block_sector_t result = level1_inode_disk.file_sector_index[level1_index];
	return result;	

    } else {
	PANIC("position too large!\n");
    }
  }
  else
    return -1;

  /* ==  our proj4 */



  /* original code
  if (pos < inode->data.length)
    return inode->data.start + pos / BLOCK_SECTOR_SIZE;
  else
    return -1;
  */
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
  buffer_cache_init (); 
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      /* original code 
      if (free_map_allocate (sectors, &disk_inode->start)) 
	{
          block_write (fs_device, sector, disk_inode);
          if (sectors > 0) 
            {
              static char zeros[BLOCK_SECTOR_SIZE];
              size_t i;
              
              for (i = 0; i < sectors; i++) 
                block_write (fs_device, disk_inode->start + i, zeros);
            }
          success = true; 
        }
      */
      
      /* our proj4 */
      // allocate originally true, use a for loop to try allocate, use an array to 
      // to keep track what disk sectors has been assigned
      // if any failed, then free all disk sector according to the array
      // if success, then insert disk_inode according to the array
      bool allocate_success = true;
      block_sector_t allocated_disk_sector_index[sectors];
      int num_allocated = 0;
      while (num_allocated < sectors){
	allocate_success = free_map_allocate(1, &allocated_disk_sector_index[num_allocated]);	
	if(!allocate_success) break;
	num_allocated++;
      }

	sectors = (length - 1) / BLOCK_SECTOR_SIZE;

      if(allocate_success)
      {
		// insert disk_inode according to array
		// given index in array => determine disk_inode index => copy sector_num in
		int level_max = 0;
		if(sectors < IDISK_SIZE -2){
		    level_max=0;
		}
		else if (sectors <= 2*IDISK_SIZE-3){
		    level_max=1;
		} else if (sectors <= ((IDISK_SIZE+2)*IDISK_SIZE-3) ){ 
		    level_max=2;
		} else{
		    PANIC("too many sectors in inode_create\n");
		}
			
  		struct inode_disk *disk_inode_level1 = NULL;	//for level 1 disk inode
  		struct inode_disk *disk_inode_level2 = NULL;	//for level 2 disk inode

		if (level_max == 1) {
  			disk_inode_level1 = calloc (1, sizeof *disk_inode_level1);
		}
		if (level_max == 2) {
  			disk_inode_level2 = calloc (1, sizeof *disk_inode_level2);
		}
		
		// START INSERT INODE_DISK HERE

		// insert LEVEL 0 inode_disk
							//0313	//means modified on this day
		int level0_index_max = (level_max==0) ? sectors : (IDISK_SIZE-3);
		int i=0;
		for ( ; i<=level0_index_max ; i++){
			disk_inode->file_sector_index[i] = allocated_disk_sector_index[i];
		}

		// insert LEVEL 1 inode_disk
		if(level_max == 1 || level_max == 2){
			block_sector_t level1_sector_index = 0;
			free_map_allocate(1, &level1_sector_index); // TODO: if free_map_allocate is NOT successful, free_map_release()
			disk_inode->file_sector_index[IDISK_SIZE-2] = level1_sector_index; // ADD level1 sector num to 'second last' of LEVEL 0
						//0313	//means modified on this day
			int level1_index_max = (level_max == 1) ? 
						sectors - (IDISK_SIZE-2) :
						IDISK_SIZE - 1;
			int i = 0;
			for ( ; i <= level1_index_max; i++){
				disk_inode_level1->file_sector_index[i] = allocated_disk_sector_index[i + (IDISK_SIZE-2)];
			}	
		}

		// insert LEVEL 2 inode_disk
		if(level_max == 2){
			block_sector_t level2_sector_index = 0;
			free_map_allocate(1, &level2_sector_index); // TODO: if free_map_allocate is NOT successful, free_map_release()
			disk_inode->file_sector_index[IDISK_SIZE-1] = level2_sector_index; // ADD level2 sector num to 'last' of LEVEL 0
			
						//0313	//means modified on this day
			int total_index_left = sectors - (2*IDISK_SIZE-2);
			int level2_entry_num = total_index_left/IDISK_SIZE + 1;

			int i = 0;
			for ( ; i < level2_entry_num; i++){
				block_sector_t level1_sector_index = 0;
				free_map_allocate(1, &level1_sector_index); // TODO: if free_map_allocate is NOT successful, free_map_release()
				disk_inode_level2->file_sector_index[i] = level1_sector_index; // ADD level1 sector num to 'i' of LEVEL2

				int level1_index_max = IDISK_SIZE - 1;
				if(i == level2_entry_num - 1) {
					level1_index_max = total_index_left%IDISK_SIZE;
				}
				int j = 0;
				for ( ; j <= level1_index_max; j++){
					int loc_level1_index = 2*IDISK_SIZE-2 + i*IDISK_SIZE + j;
					disk_inode_level1->file_sector_index[j] = allocated_disk_sector_index[loc_level1_index];
				}				
				write_via_cache(NULL, (void *)disk_inode_level1, level1_sector_index, 0, BLOCK_SECTOR_SIZE);
			}	

		}

		// after all insertion, write_via_cache()
		// END INSERT INODE_DISK HERE 
		
		write_via_cache(NULL, (void *)disk_inode, sector, 0, BLOCK_SECTOR_SIZE);

		if (level_max == 1) {
			block_sector_t level1_sector_index = disk_inode->file_sector_index[IDISK_SIZE-2]; 
			write_via_cache(NULL, (void *)disk_inode_level1, level1_sector_index, 0, BLOCK_SECTOR_SIZE);	
  			free(disk_inode_level1);
		}
		if (level_max == 2) { // write LEVEL2 here, LEVEL 1 has been written in if(level_max == 2)
			block_sector_t level2_sector_index = disk_inode->file_sector_index[IDISK_SIZE-1]; 
			write_via_cache(NULL, (void *)disk_inode_level2, level2_sector_index, 0, BLOCK_SECTOR_SIZE);		
  			free(disk_inode_level2);
		}
        	success = true; 
       } else{
		num_allocated--;
	      while(num_allocated >= 0){
		free_map_release(allocated_disk_sector_index[num_allocated], 1);
	      }
       }
	/* == our proj4 */ 
	
        free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   /Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  /* original code
  block_read (fs_device, inode->sector, &inode->data);
  */
	/* our proj4 */
			//find sector for inode_disk, find length and set
			//     inode.length
  struct inode_disk  data; 
  read_via_cache(NULL, (void *)&data, sector, 0, BLOCK_SECTOR_SIZE);
  inode->length = data.length;
	/* == our proj4 */
  
  /* original code 
  read_via_cache(inode, (void *)&inode->data, sector, 0, BLOCK_SECTOR_SIZE);
  */
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
	/* our proj4*/

		//go through buffer cache, write back the sectors belong to inode
		//TODO need to "free" buffer_info_array of indirect & double indirect inode_disks
	struct buffer_info *buffer_info_array = get_buffer_info_array();
	int i = 0;
	for (; i < BUFFER_SIZE; i++) {
		if (buffer_info_array[i].buffer_inode == inode
		|| buffer_info_array[i].sector_num == inode->sector) {
			int sector_idx = buffer_info_array[i].sector_num;
			void *buffer_cache_start_addr = get_buffer_vaddr() + i * BLOCK_SECTOR_SIZE;
                	block_write (fs_device, sector_idx, buffer_cache_start_addr);

			buffer_info_array[i].sector_num = -2;
			buffer_info_array[i].buffer_inode = NULL;
			buffer_info_array[i].dirty = false;
			buffer_info_array[i].recentlyUsed = false;
		} 
	}

	/* == our proj4*/

      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
	  /* original code
          free_map_release (inode->sector, 1);
          free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length)); 
	  */


		/* our proj4  */
	  // free all disk sectors which are occupied by files
          // 	    according to disk_inode
		//figure out level_max
		//here sectors is actually index
		int sectors = (inode->length - 1) / BLOCK_SECTOR_SIZE;
		int level_max = 0;
		if(sectors < IDISK_SIZE -2){
		    level_max=0;
		}
		else if (sectors <= 2*IDISK_SIZE-3){
		    level_max=1;
		} else if (sectors <= ((IDISK_SIZE+2)*IDISK_SIZE-3) ){ 
		    level_max=2;
		} else{
		    PANIC("too many sectors in inode_close\n");
		}
		//free from the depest level: first free_map_release its entries, then it self

		struct inode_disk disk_inode_level0;
		struct inode_disk disk_inode_level1;
		struct inode_disk disk_inode_level2;

		//get level0 inode_disk
		read_via_cache(NULL, (void *)&disk_inode_level0, inode->sector, 0, BLOCK_SECTOR_SIZE);

		//free_map_release double indrect level2's elem and it self, and level2's sector_num
		if(level_max == 2){
			//get level2 inode_disk
			int level2_sector_index = disk_inode_level0.file_sector_index[BLOCK_SECTOR_SIZE - 1];
			read_via_cache(NULL, (void *)&disk_inode_level2, level2_sector_index, 0, BLOCK_SECTOR_SIZE);

			int total_index_left = sectors - (2*IDISK_SIZE-2);
			int level2_entry_num = total_index_left/IDISK_SIZE + 1;

			int i = 0;
			for ( ; i < level2_entry_num; i++){
				//get level1_sector_index from disk_inode_level2
				block_sector_t level1_sector_index = 0;
				level1_sector_index = disk_inode_level2.file_sector_index[i]; 
				//read level1 inode_disk from cache
				read_via_cache(NULL, (void *)&disk_inode_level1, level1_sector_index, 0, BLOCK_SECTOR_SIZE);

				int level1_index_max = IDISK_SIZE - 1;
				if(i == level2_entry_num - 1) {
					level1_index_max = total_index_left%IDISK_SIZE;
				}
				int j = 0;
				for ( ; j <= level1_index_max; j++){
					int loc_level1_index = disk_inode_level1.file_sector_index[j]; 
          				free_map_release (loc_level1_index, 1);
				}				

				//free disk_node_level2[i] (val as ith elem in disk_node_level2) as well
          			free_map_release (level1_sector_index, 1);
			}
			//free disk_node_level0[BLOCK_SECTOR_SIZE - 1] (disk_node_level_2's sector_num) as well
          		free_map_release (level2_sector_index, 1);
		}

		//free_map_release single indrect level1's elem and it self
		if(level_max == 1 || level_max == 2){
			//get level1 inode_disk
			int level1_sector_index = disk_inode_level0.file_sector_index[BLOCK_SECTOR_SIZE - 2];
			read_via_cache(NULL, (void *)&disk_inode_level1, level1_sector_index, 0, BLOCK_SECTOR_SIZE);

			int level1_index_max = (level_max == 1) ?
						sectors - (IDISK_SIZE-2) :
						IDISK_SIZE - 1;
			int i = 0;
			for ( ; i <= level1_index_max; i++){
				int level1_sector_index = disk_inode_level1.file_sector_index[i];
          			free_map_release (level1_sector_index, 1);
			}	
			//free disk_node_level0[BLOCK_SECTOR_SIZE - 2] (disk_node_level_1's sector_num) as well
          		free_map_release (level1_sector_index, 1);
		}


		//need to process level0 anyway
		int level0_index_max = (level_max==0) ? sectors : (IDISK_SIZE-3);
		int i=0;
		for ( ; i<=level0_index_max ; i++){
			int level0_sector_index = disk_inode_level0.file_sector_index[i];
          		free_map_release (level0_sector_index, 1);
		}

		//free_map_release level0 itself
          	free_map_release (inode->sector, 1);
		/* == our proj4  */

        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
	//printf("inode_read_at called, sector_idx %d, offset %d\n", byte_to_sector (inode, offset), offset);
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;

	/* original code
  uint8_t *bounce = NULL;	*/ 

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

	/* KAI proj4 */

	read_via_cache(inode, buffer + bytes_read, sector_idx, sector_ofs, chunk_size);

	/* == KAI proj4 */

	/* original code
	if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          // Read full sector directly into caller's buffer. //
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          // Read sector into bounce buffer, then partially copy
          //   into caller's buffer.
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }	
*/
	
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
//      printf("size %d offset %d bytes_read %d\n", size, offset, bytes_read);
    }
	/* original code
  free(bounce);		*/	
//	printf("songhuang size %d\n", bytes_read);
  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;

	/* original code
  uint8_t *bounce = NULL;
	*/

  if (inode->deny_write_cnt)
    return 0;

  /* KAI IMPLEMENTATION */
  // HERE we need to write [offset, offset+size] in file. So create a space if 
  //needed then  
  allocate_disk_space_for_inode_write(inode, offset, size);
  /* == KAI IMPLEMENTATION */

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

	/* our proj4*/

	write_via_cache(inode, buffer + bytes_written, sector_idx, sector_ofs, chunk_size);

	/* == our proj4*/

	/* original code
      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          // Write full sector directly to disk. 
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          // We need a bounce buffer. 
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          // If the sector contains data before or after the chunk
          //   we're writing, then we need to read in the sector
          //   first.  Otherwise we start with a sector of all zeros. 
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else {
		PANIC("lalla\n");
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
		}
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }
	*/

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }


	/* original code
  free (bounce);	
	*/

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->length;
  /* original code
  return inode->data.length;
  */
}



/* KAI IMPLEMENTATION */
// helper function: allocate disk space for inode_write_at
// function:
// 1. allocate disk space for [length, offset+size]
//	i) check the end of existing position
// 2. put data between [length, offset-1] to be 0
// 3. update inode and inode_disk length
void
allocate_disk_space_for_inode_write(struct inode* inode, off_t offset, off_t size){


// step 1. allocate disk_space for [length, offset+size]

      // allocate originally true, use a for loop to try allocate, use an array to 
      // to keep track what disk sectors has been assigned
      // if any failed, then free all disk sector according to the array
      // if success, then insert disk_inode according to the array

      off_t old_length = inode->length; //before alloc new disks 
      off_t new_length = offset+size+1; //new POSSIBLE length
      int existing_sector_num = bytes_to_sectors(old_length);
      int future_possible_total_sector_num  = bytes_to_sectors(offset+size);
      sectors_to_be_alloc= future_possible_total_sector_num - existing_sector_num;
	
      bool allocate_success = true;
   
      if (sectors_to_be_alloc > 0)
	      block_sector_t allocated_disk_sector_index[sectors_to_be_alloc];
	      int num_allocated = 0;
	      while (num_allocated < sectors_to_be_alloc){
		allocate_success = free_map_allocate(1, &allocated_disk_sector_index[num_allocated]);	
		if(!allocate_success) break;
		num_allocated++;
	      }

           	// assign disk sector num in inode_disk index range [start_sector_index, end_sector_index]
		block_sector_t end_sector_index = (new_length - 1) / BLOCK_SECTOR_SIZE;
		block_sector_t start_sector_index = (old_length -1 )/BLOCK_SECTOR_SIZE + 1;
		
	

	      if(allocate_success)
	      {
			// ----------------------
			// check start_sector_index
			int start_level = 0;
			if(start_sector_index < IDISK_SIZE -2){

			    start_level=0;
			}
			else if (start_sector_index <= 2*IDISK_SIZE-3){
			    start_level=1;
			} else if (start_sector_index <= ((IDISK_SIZE+2)*IDISK_SIZE-3) ){
			    start_level=2;
			} else{
			    PANIC("too many sectors in inode_create\n");
			}
			// ----------------------
			//check end_sector_index
			int end_level = 0;
			if(end_sector_index < IDISK_SIZE -2){

			    end_level=0;
			}
			else if (end_sector_index <= 2*IDISK_SIZE-3){
			    end_level=1;
		} else if (end_sector_index <= ((IDISK_SIZE+2)*IDISK_SIZE-3) ){
		    end_level=2;
		} else{
		    PANIC("too many sectors in inode_create\n");
		}

		struct inode_disk *disk_inode_level0 = NULL;	//for level 1 disk inode
		struct inode_disk *disk_inode_level1 = NULL;	//for level 1 disk inode
		struct inode_disk *disk_inode_level2 = NULL;	//for level 2 disk inode

		
		disk_inode_level0 = calloc (1, sizeof *disk_inode_level0);
//			if ( (end_level == 1 || end_level == 2) && 
//					start_level == 0 || start_level == 1) {
		disk_inode_level1 = calloc (1, sizeof *disk_inode_level1);
//			if (end_level == 2) {
		disk_inode_level2 = calloc (1, sizeof *disk_inode_level2);
	
		// START INSERT INODE_DISK HERE

		// insert LEVEL 0 inode_disk
		
		if (start_level == 0){
			int level0_index_start = (old_length == 0) ? 0 : start_sector_index;
			int level0_index_end = 0;
			if (end_level == 0) {
				level0_index_end = (new_length - 1) / BLOCK_SECTOR_SIZE;
			} else {
				level0_index_end = IDISK_SIZE - 3;	
			}

			int i= level0_index_start;
			for ( ; i<=level0_index_end; i++){
				disk_inode_level0->file_sector_index[i] = allocated_disk_sector_index[i-level0_index_start];
			}
		}

		// insert LEVEL 1 inode_disk

		if((end_level == 1 || end_level == 2)  &&
				(start_level ==0 || start_level == 1 )){

			// determine if we need to alloc inode_disk_level1
			int level1_index_start = ( start_sector_index < (IDISK_SIZE-2) ) ? 0 : (start_sector_index - (IDISK_SIZE-2));
			int level1_index_end = 0;
			if(end_level == 1){
				level1_index_end = end_sector_index - (IDISK_SIZE - 2); 
			} else {
				level1_index_end = IDISK_SIZE - 1;
			}
			
			block_sector_t level1_sector_index = 0;
			
			if( level1_index_start == 0){
				free_map_allocate(1, &level1_sector_index); // TODO: if free_map_allocate is NOT successful, free_map_release()
			}else{
				

		
			disk_inode->file_sector_index[IDISK_SIZE-2] = level1_sector_index; // ADD level1 sector num to 'second last' of LEVEL 0

				int level1_index_max = (level_max == 1) ? 
							sectors - (IDISK_SIZE-2) :
							IDISK_SIZE - 1;
				int i = 0;
				for ( ; i <= level1_index_max; i++){
					disk_inode_level1->file_sector_index[i] = allocated_disk_sector_index[i + (IDISK_SIZE-2)];
				}	
			}

			// insert LEVEL 2 inode_disk
			if(level_max == 2){
				block_sector_t level2_sector_index = 0;
				free_map_allocate(1, &level2_sector_index); // TODO: if free_map_allocate is NOT successful, free_map_release()
				disk_inode->file_sector_index[IDISK_SIZE-1] = level2_sector_index; // ADD level2 sector num to 'last' of LEVEL 0
			
							//0313	//means modified on this day
				int total_index_left = sectors - (2*IDISK_SIZE-2);
				int level2_entry_num = total_index_left/IDISK_SIZE + 1;

				int i = 0;
				for ( ; i < level2_entry_num; i++){
					block_sector_t level1_sector_index = 0;
					free_map_allocate(1, &level1_sector_index); // TODO: if free_map_allocate is NOT successful, free_map_release()
					disk_inode_level2->file_sector_index[i] = level1_sector_index; // ADD level1 sector num to 'i' of LEVEL2

					int level1_index_max = IDISK_SIZE - 1;
					if(i == level2_entry_num - 1) {
						level1_index_max = total_index_left%IDISK_SIZE;
					}
					int j = 0;
					for ( ; j <= level1_index_max; j++){
						int loc_level1_index = 2*IDISK_SIZE-2 + i*IDISK_SIZE + j;
						disk_inode_level1->file_sector_index[j] = allocated_disk_sector_index[loc_level1_index];
					}				
					write_via_cache(NULL, (void *)disk_inode_level1, level1_sector_index, 0, BLOCK_SECTOR_SIZE);
				}	

			}

			// after all insertion, write_via_cache()
			// END INSERT INODE_DISK HERE 
		
			write_via_cache(NULL, (void *)disk_inode, sector, 0, BLOCK_SECTOR_SIZE);

			if (level_max == 1) {
				block_sector_t level1_sector_index = disk_inode->file_sector_index[IDISK_SIZE-2]; 
				write_via_cache(NULL, (void *)disk_inode_level1, level1_sector_index, 0, BLOCK_SECTOR_SIZE);	
			}
			if (level_max == 2) { // write LEVEL2 here, LEVEL 1 has been written in if(level_max == 2)
				block_sector_t level2_sector_index = disk_inode->file_sector_index[IDISK_SIZE-1]; 
				write_via_cache(NULL, (void *)disk_inode_level2, level2_sector_index, 0, BLOCK_SECTOR_SIZE);		
			}

	  		free(disk_inode_level0);
	  		free(disk_inode_level1); 
	  		free(disk_inode_level2);
			
			success = true; 
	       } else{
			num_allocated--;
		      while(num_allocated >= 0){
			free_map_release(allocated_disk_sector_index[num_allocated], 1);
		      }
	       }
	
       /* == KAI IMPLEMENTATION */

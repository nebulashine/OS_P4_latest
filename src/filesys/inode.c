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


/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[125];               /* Not used. */
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
    struct inode_disk data;             /* Inode content. */
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
    return inode->data.start + pos / BLOCK_SECTOR_SIZE;
  else
    return -1;
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
      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
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
  block_read (fs_device, inode->sector, &inode->data);
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
	struct buffer_info *buffer_info_array = get_buffer_info_array();
	int i = 0;
	for (; i < BUFFER_SIZE; i++) {
		if (buffer_info_array[i].buffer_inode == inode) {
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
          free_map_release (inode->sector, 1);
          free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length)); 
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
	//printf("sector_idx %d, size %d\n", sector_idx, size);
      int buffer_arr_indx = lookup_sector(sector_idx);	//lookup result indx for a valid sector in cache
	if (buffer_arr_indx > BUFFER_SIZE) {
		PANIC("lookup_sector wrong\n");
	}
//	printf("sector_idx %d\n", sector_idx);
      if (buffer_arr_indx >= 0){ 
//	printf("indx %d, sector_idx %d size %d\n", buffer_arr_indx, sector_idx, size);
	
	void *buffer_cache_start_addr = get_buffer_vaddr() + buffer_arr_indx * BLOCK_SECTOR_SIZE + sector_ofs;
	memcpy(buffer+bytes_read, buffer_cache_start_addr, chunk_size);
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
	void *buffer_cache_start_addr = get_buffer_vaddr() + buffer_indx * BLOCK_SECTOR_SIZE;
		//copy the sector from disk to buffer cache
        block_read (fs_device, sector_idx, buffer_cache_start_addr);
		//change buffer_cache_start_addr to the offset of buffer cache sector
	buffer_cache_start_addr += sector_ofs;
		//copy the sector from buffer cache to buffer_read
        memcpy (buffer + bytes_read, buffer_cache_start_addr, chunk_size);

		//fill the info into buffer_info_array
	struct buffer_info *buffer_info_array = get_buffer_info_array();
	buffer_info_array[buffer_indx].sector_num = sector_idx;
	buffer_info_array[buffer_indx].buffer_inode = inode;
	buffer_info_array[buffer_indx].dirty = false;
	buffer_info_array[buffer_indx].recentlyUsed = true;
      }

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

	//lookup buffer and check if sector_indx exits in buffer cache
      int buffer_arr_indx = lookup_sector(sector_idx);	//lookup result indx for a valid sector in cache
	if (buffer_arr_indx > BUFFER_SIZE) {
		PANIC("lookup_sector wrong\n");
	}
      if (buffer_arr_indx >= 0){ 
		//if yes, just write buffer cache
	void *buffer_cache_start_addr = get_buffer_vaddr() + buffer_arr_indx * BLOCK_SECTOR_SIZE + sector_ofs;
        memcpy (buffer_cache_start_addr, buffer + bytes_written, chunk_size);
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
	void *buffer_cache_start_addr = get_buffer_vaddr() + buffer_indx * BLOCK_SECTOR_SIZE;
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
        memcpy (buffer_cache_start_addr, buffer + bytes_written, chunk_size);

		//fill the info into buffer_info_array
	struct buffer_info *buffer_info_array = get_buffer_info_array();
	buffer_info_array[buffer_indx].sector_num = sector_idx;
	buffer_info_array[buffer_indx].buffer_inode = inode;
	buffer_info_array[buffer_indx].dirty = true;
	buffer_info_array[buffer_indx].recentlyUsed = true;

      }
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
  return inode->data.length;
}

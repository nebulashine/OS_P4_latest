
Buffer Cache:
-----------------------------------
DONE:
	modify:
		inode_read_at()
		inode_write_at()
	in inode.c

inode.c
	change inode_init()
	change inode_read_at()


added cache.h
added cache.c


TODO: 
implement read-ahead (in back ground)
implement synchronization

Indexed and Extensible Files:
-----------------------------------
	add array/indirect arrray in inode struct in inode.c

	when create file, don't allocate freemap continously, but one by one instead: 
		free_map_allocate(); 
		inode_create() in inode.c

	when write to end of file, extend the file
		free_map_allocate();

DONE:
	modified struct disk_inode
	modified struct inode


TODO:
	modify inode_create()
	modify inode_close()
	modify inode_open()
	modify inode_length()
	modify byte_to_setor()
	

Subdirectories
-----------------------------------
	update open & close in syscall.c


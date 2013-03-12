
Buffer Cache:
-----------------------------------
	modify:
		inode_read_at()
		inode_write_at()
	in inode.c

inode.c
	change inode_init()
	change inode_read_at()


added cache.h
added cache.c

Indexed and Extensible Files:
-----------------------------------
	add array/indirect arrray in inode struct in inode.c

	when create file, don't allocate freemap continously, but one by one instead: 
		free_map_allocate(); 
		inode_create() in inode.c

	when write to end of file, extend the file
		free_map_allocate();


Subdirectories
-----------------------------------
	update open & close in syscall.c


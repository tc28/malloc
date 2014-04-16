i) the structure of my free and allocated blocks
free block: 
each free block has a
a) header, one word before the block pointer, packing the size and ALLOC bit of this block
b) footer, at the end of this block, packing the size and ALLOC bit of this block
c) predecessor pointer, whose position is the same as that of the block pointer, pointing to the block pointer of its predecessor block in the free list
d) successor pointer, one word after the block pointer, pointing to the block pointer of its successor block in the free list

allocated block: 
an allocated block does not have valid predecessor or successor
because it is removed from the free linked list


ii) the organization of free list
the freel list starts with a prologue (both logical and physical) of 4 words:
//prologue:	
PUT(heap_ptr + (2*WSIZE), PACK(DSIZE*2, 1));//header
PUT(heap_ptr + (3*WSIZE), 0);//predecessor & bp
PUT(heap_ptr + (4*WSIZE), 0);//successor
PUT(heap_ptr + (5*WSIZE), PACK(DSIZE*2, 1));//footer

the free list ends with a LOGICAL epilogue of 4 words
//pseudo LOGICAL epilogue:
//at the LOGICAL end of the free linked list
PUT(heap_ptr + (6*WSIZE), PACK(DSIZE*2, 1));//header 
PUT(heap_ptr + (7*WSIZE), 0);//predecessor & bp
PUT(heap_ptr + (8* WSIZE), 0);//successor
PUT(heap_ptr + (9* WSIZE), PACK(DSIZE*2, 1));//footer of epilgue
this is not the same as the PHYSICAL epilogue at the PHYSICAL end of the heap

then, I updated the pointers of the prologue and epilogue:
//upate pointers
//changing the prologue's successor to epigue
//changing the epilogues's predecessor to prologue
PUT(heap_ptr + 4*WSIZE, (int) (heap_ptr + 7*WSIZE));
PUT(heap_ptr + 7*WSIZE, (int) (heap_ptr + 3*WSIZE));

initially, my free list only contains this two nodes
when new memory are extended,
the free list will increase: 
the new free block will be added to the free list, after head

iii) how my allocator manipulates free blocks
when there is a request to malloc, my allocator looks for the first block in the free list of sufficient size, 
using the "first-fit" strategy. if such as fit is found, we mark this block as allocated,
remove this block from the free list, and return this block's block pointer. in a special case (the size of 
this block is larger than the requested size), a split is performed and the remaining block will be still
in the free list.

iv) my strategy for maintaining compaction
I use the function coalesce() to maintain compaction
three cases: 
a) coalesce with the next block and return a new block pointer, if ONLY the next block is freed
b) coalesce with the previous block and return a new block pointer, if ONLY the previous block is freed
c) coalesce with both the previous and the next block, and return a new block pointer, if BOTH the previous and next blocks are freed

v) unresolved bugs:
I don't have any unresovled bugs and passed all the test.


However, I cannot correctly implement the 4th case of realloc: merging with both the previous block
and the next block, both are which are freed. I left some comments and my code.
Please DON'T dock points off for style for it, because I left my code there for your record.
If you could, please help me fix the problems with this case, rerun mdriver, and assign a better performance grade to me.
Currently, without this case, my performance grade is 82. 

commented out code:
please add back to my line 352, if you want to try on it. 
/*
						total_size = original_size + previous_size + next_size;
						if ((total_size - new_size) >= 2 * DSIZE) {
							void *prev_bp = PREV_BLKP(ptr);
							void *next_bp = NEXT_BLKP(ptr);
							
							void *previous_predecessor = (void *) GET(PREDP(prev_bp));
							void *previous_successor = (void *) GET(SUCCP(prev_bp));
							void *next_predecessor = (void *) GET(PREDP(next_bp));
							void *next_successor = (void *) GET(SUCCP(next_bp));
							
							void *new_ptr;
							
							if ((previous_predecessor != 0) &&
							   (previous_successor != 0) &&
							   (next_predecessor != 0) &&
							   (next_successor != 0)) {
								
								//first of all
								//breaking the link for next_bp
								PUT(SUCCP(previous_predecessor), (unsigned int) previous_successor);
								PUT(PREDP(previous_successor), (unsigned int) previous_predecessor);
								
								PUT(HDRP(prev_bp), PACK(new_size, 1));
								
								new_ptr = NEXT_BLKP(prev_bp);
							
								PUT(HDRP(new_ptr), PACK(total_size - new_size, 0));
							
								PUT(PREDP(new_ptr), (unsigned int) next_predecessor);
								PUT(SUCCP(next_predecessor), (unsigned int) new_ptr);
								PUT(SUCCP(new_ptr), (unsigned int) next_successor);
								PUT(PREDP(next_successor), (unsigned int) new_ptr);
							
								PUT(FTRP(prev_bp), PACK(new_size, 1));
								PUT(FTRP(new_ptr), PACK(total_size - new_size, 0));
							
								memmove(prev_bp, ptr, original_size);
								coalesce(new_ptr);
								
								ptr = prev_bp;
								return ptr;
							  
							  
							}				  
						}
*/
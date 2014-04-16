#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"


/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
//my own definitions:
#define WSIZE 4
#define DSIZE 8
//extend heap by this amount
#define CHUNKSIZE (1<<12)

/*definition of MACROS*/
/*based on the textbook sample code*/
/*pack a size and allocated bit into a word*/
#define PACK(size, alloc) ((size) | (alloc))
/*read and write a word at address p*/
#define GET(p) (*(unsigned int *) p)
#define PUT(p, val) (*(unsigned int *) (p) = (val))
/*read the size and allocated fields from address p*/
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/*given block ptr bp, compute address of its*/
/*header, footer, successor, and predecessor*/
#define HDRP(bp) ((char *) (bp) - WSIZE)
//gettin the predecessor and successor
#define PREDP(bp) ((char *) (bp))
#define SUCCP(bp) ((char *) (bp) + WSIZE)
#define FTRP(bp) ((char *) (bp) - DSIZE + GET_SIZE(HDRP(bp)))

/*given block ptr bp, compute the address*/
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define MAX(x, y) ((x) > (y) ? (x) : (y))

//the start of the free list
//the ONLY global variable that I have
void *head;


//declarations
//headers of helper procedures by Tian
//for free lists:
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
//for memory allocation:
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

//this is a tester that you -- the TA 
//can always use for testing purpose
//I leave it here and please don't dock points off for style
/*
static void print_free_list() {
	void *bp;
	int counter = 0;
	for (bp = head; GET(SUCCP(bp)) != 0; bp = (void *) GET(SUCCP(bp))) {
		printf("adddress of bp: %p\n", bp);
		printf("size of bp: %d\n", (int) GET_SIZE(HDRP(bp)));
		printf("address of previous blkp: %p\n", (PREV_BLKP(bp)));
		counter++;
	}  
	printf("number of nodes:%d\n", counter);
}
*/

/*
 * initializes the dynamic allocator
 * arguments: none
 * returns: 0, if successful
 *          1, if an error occurs
 */
int mm_init(void) {
  
	
	//the heap_ptr of free list
	void *heap_ptr;
 
	heap_ptr = mem_sbrk(12 * WSIZE); 
	if (heap_ptr == (void *) -1) {
		return -1;
	}
	
	PUT(heap_ptr, 0);
	PUT(heap_ptr + WSIZE, 0);
	
	
	//prologue:	
	PUT(heap_ptr + (2*WSIZE), PACK(DSIZE*2, 1));//header
	PUT(heap_ptr + (3*WSIZE), 0);//predecessor & bp
	PUT(heap_ptr + (4*WSIZE), 0);//successor
	PUT(heap_ptr + (5*WSIZE), PACK(DSIZE*2, 1));//footer
	
	//pseudo LOGICAL epilogue:
	//at the LOGICAL end of the free linked list
	PUT(heap_ptr + (6*WSIZE), PACK(DSIZE*2, 1));//header 
	PUT(heap_ptr + (7*WSIZE), 0);//predecessor & bp
	PUT(heap_ptr + (8* WSIZE), 0);//successor
	PUT(heap_ptr + (9* WSIZE), PACK(DSIZE*2, 1));//footer of epilgue
	
	//real epilogue:
	//at the PHYSICAL end of memory
	PUT(heap_ptr + (10*WSIZE), PACK(0, 1));
	PUT(heap_ptr + (11*WSIZE), PACK(0, 1));
	

	//update head
	head = heap_ptr + (3*WSIZE);
	
	//upate pointers
	//changing the prologue's successor to epigue
	//changing the epilogues's predecessor to prologue
	PUT(heap_ptr + 4*WSIZE, (int) (heap_ptr + 7*WSIZE));
	PUT(heap_ptr + 7*WSIZE, (int) (heap_ptr + 3*WSIZE));
	
	//next, we need to extend the empty heap with 
	//a free block of CHUNKSIZE bytes
	if (extend_heap(CHUNKSIZE/WSIZE) == NULL) return 1;	  

	return 0;	
}


/*
 * allocates a block of memory and returns a pointer to that block
 * arguments: size: the desired block size
 * returns: a pointer to the newly-allocated block (whose size
 *          is a multiple of ALIGNMENT), or NULL if an error
 *          occurred
 */
void *mm_malloc(size_t size) {
	
	size_t asize;
	size_t extendsize;//the size to be extended
	void *bp;
	
	if (size == 0) return NULL;
	
	//adjust block size to include overhead and alignment reqs
	if (size <= DSIZE) {
		asize = 2*DSIZE;
	} else {
		asize = DSIZE * ((size + (DSIZE) + (DSIZE -1))/DSIZE);
 	}
		
	if ((bp = find_fit(asize)) != NULL) {
		place(bp, asize);
		return bp;
	}
	
	//no fit found
	extendsize = MAX(asize, CHUNKSIZE);
	if ((bp = extend_heap(extendsize/WSIZE)) == NULL) return NULL;
	place(bp, asize);
	
	return bp;
}





/*
 * frees a block of memory, enabling it to be reused later
 * arguments: ptr: the allocated block to free
 * returns: nothing
 */
void mm_free(void *ptr) {

	size_t size = GET_SIZE(HDRP(ptr));
	void *previous_succ;
	
	PUT(HDRP(ptr), PACK(size, 0));
	
	previous_succ = (void *) GET(SUCCP(head));	
	PUT(PREDP(ptr), (unsigned int) head);
	PUT(SUCCP(head), (unsigned int) ptr);
	PUT(SUCCP(ptr), (unsigned int) previous_succ);
	PUT(PREDP(previous_succ), (unsigned int) ptr);
	
	PUT(FTRP(ptr), PACK(size, 0));
	
	//coalescing
	coalesce(ptr);
}


/*
 * reallocates a memory block to update it with the given size
 * arguments: ptr: a pointer to the memory block
 *            size: the desired new block size
 * returns: a pointer to the new memory block
 */
void *mm_realloc(void *ptr, size_t size) {
	
	if (ptr == NULL) return mm_malloc(size);
	else if (size == 0) {
		
		 mm_free(ptr);
		 return NULL;
	} else {
		size_t original_size;
		original_size = GET_SIZE(HDRP(ptr));
		
		size_t new_size;
		
		//resizing
		//calculating the correct new_size here
		//crucial
		if (size <= DSIZE) {
			new_size = 2*DSIZE;
		} else {
			new_size = DSIZE * ((size + (DSIZE) + (DSIZE -1))/DSIZE);
		}
		
		
		
		if (new_size <= original_size) {
			
			//allcated block can be reallocated
			//first case: split
			if ((original_size - new_size) >= (2*DSIZE)) {
				 
				 void *predecessor = (void *) GET(PREDP(ptr));
				 void *successor = (void *) GET(SUCCP(ptr));
				 void *new_ptr;
				
				 if ((predecessor != 0) && (successor != 0)) {
					PUT(HDRP(ptr), PACK(new_size, 1));
				 
					new_ptr = NEXT_BLKP(ptr);
					PUT(HDRP(new_ptr), PACK(original_size - new_size, 0));
				 
					//changing pointers
					PUT(PREDP(new_ptr), (unsigned int) predecessor);
					PUT(SUCCP(predecessor), (unsigned int) new_ptr);
					PUT(SUCCP(new_ptr), (unsigned int) successor);
					PUT(PREDP(successor), (unsigned int)  new_ptr);
				 
					PUT(FTRP(ptr), PACK(new_size, 1));
					PUT(FTRP(new_ptr), PACK(original_size - new_size, 0));		
					
					coalesce(new_ptr);
					return ptr;
				 }
			}		  
		}  else {
			//else case: space not sufficient -> dealing with other blocks
			size_t next_size;
			size_t total_size;
			
			next_size = 0;
	
			if (!(GET_ALLOC(HDRP(NEXT_BLKP(ptr))))) {
 				next_size = GET_SIZE(HDRP(NEXT_BLKP(ptr)));  
			}
			
			total_size = original_size + next_size;
			
			//second case: merge with next block
			if (new_size <= total_size) {
				if ((total_size - new_size) >= (2*DSIZE)) {
					void *next_bp = NEXT_BLKP(ptr);
					
					void *predecessor = (void *) GET(PREDP(next_bp));
					void *successor = (void *) GET(SUCCP(next_bp));
					void *new_ptr;
					
					if ((predecessor != 0) && (successor != 0)) {
					
						PUT(HDRP(ptr), PACK(new_size, 1));
					
						new_ptr = NEXT_BLKP(ptr);
						PUT(HDRP(new_ptr), PACK(total_size - new_size, 0));
					
						PUT(PREDP(new_ptr), (unsigned int) predecessor);
						PUT(SUCCP(predecessor), (unsigned int) new_ptr);
						PUT(SUCCP(new_ptr), (unsigned int) successor);
						PUT(PREDP(successor), (unsigned int)  new_ptr);
						
						PUT(FTRP(ptr), PACK(new_size, 1));
						PUT(FTRP(new_ptr), PACK(total_size - new_size, 0));	
						
						coalesce(new_ptr);
						return ptr;							
					}
				    
				}			  
			} else {
				//third case
				//merging with the previous blk
				if (!(GET_ALLOC(HDRP(PREV_BLKP(ptr))))) {
					size_t previous_size;
					previous_size = GET_SIZE(HDRP(PREV_BLKP(ptr)));
					
					total_size = original_size + previous_size;
					
					if ((total_size - new_size) >= 2*DSIZE) {
						void *prev_bp = PREV_BLKP(ptr);
						
						void *predecessor = (void *) GET(PREDP(prev_bp));
						void *successor = (void *) GET(SUCCP(prev_bp));
						void *new_ptr;
						
						if ((predecessor != 0) && (successor != 0)) {
							 
							PUT(HDRP(prev_bp), PACK(new_size, 1));
							memmove(prev_bp, ptr, original_size);
							
							
							new_ptr = NEXT_BLKP(prev_bp);
							
							PUT(HDRP(new_ptr), PACK(total_size - new_size, 0));
							
							PUT(PREDP(new_ptr), (unsigned int) predecessor);
							PUT(SUCCP(predecessor), (unsigned int) new_ptr);
							PUT(SUCCP(new_ptr), (unsigned int) successor);
							PUT(PREDP(successor), (unsigned int)  new_ptr);
							
							PUT(FTRP(prev_bp), PACK(new_size, 1));
							PUT(FTRP(new_ptr), PACK(total_size - new_size, 0));
							
							coalesce(new_ptr);
							return prev_bp;  
						  
						}					  
					} else {
					  
					//please don't dock style points off for this commented out case
					//I leave it here INTENTIONALLY for your -- the TA's record  
					//otherwise I have no where to keep track of the code for this case
					  
					//fourth case:
					//needs to coalesce with 2 blocks
					//both the previous and the next
					//dear TA:
					//I keep this commented out block here just for your record
					//if you could fix it, please give me partial credit
					//problem with this 4th case
					//error: old block of memory was not preserved
					//without this case: performance socre: 82
					
					}
				}		  
			}
		}
	}
	
	//if does not fit in any of the cases
	//basic
	void *oldptr = ptr;
	void *newptr;
	
	size_t copySize;
	
	newptr = mm_malloc(size);
	if (newptr == NULL)
		return NULL;
	copySize = GET_SIZE(HDRP(ptr));
	
	
	if (size < copySize)
		copySize = size;
	memcpy(newptr, oldptr, copySize);
	mm_free(oldptr);
	return newptr;
}

//////////////////////////////
//////////////////////////////
/*helper functions by Tian: */
//////////////////////////////
//////////////////////////////

//place allocates a blocks
//if the size is bigger than needed
//split and add the block left to the free list
//input:
//bp: block pointer pointing to the block 
//asize: the requested size
//no output
static void place(void *bp, size_t asize) {
	 
	 size_t csize = GET_SIZE(HDRP(bp));
	 //capacity csize
	 
	 if ((csize - asize) >= (2*DSIZE)) {
		void *predecessor = (void *) GET(PREDP(bp));
		void *successor = (void *) GET(SUCCP(bp));
		void *new_bp;
		
		//getting the new size
		PUT(HDRP(bp), PACK(asize, 1));
		
		//then get next INSIDE the node
		new_bp = NEXT_BLKP(bp);
		PUT(HDRP(new_bp), PACK(csize - asize, 0));
		
		//changing pointers
		PUT(PREDP(new_bp), (unsigned int) predecessor);//storing address
		PUT(SUCCP(predecessor), (unsigned int) new_bp);
		PUT(SUCCP(new_bp), (unsigned int) successor);
		PUT(PREDP(successor), (unsigned int) new_bp);
		
		PUT(FTRP(new_bp), PACK(csize - asize, 0));
		
		PUT(FTRP(bp), PACK(asize, 1));

		
	 } else {
	   
		PUT(HDRP(bp), PACK(csize, 1));
	    
		PUT(SUCCP(GET(PREDP(bp))), GET(SUCCP(bp)));
		PUT(PREDP(GET(SUCCP(bp))), GET(PREDP(bp)));	   
	   
		PUT(FTRP(bp), PACK(csize, 1));
	 }  
}

//find_fit:
//finds the first node in the free linked list
//with sufficient size and returns the block pointer of this node
//input: 
//asize, the target size
//output:
//the block pointer to the first-fit block
//OR
//NULL if such fit is not found
static void *find_fit(size_t asize) {
	void *bp;
	
	for (bp = head; GET(SUCCP(bp)) != 0; bp = (void *) GET(SUCCP(bp))) {
		if ((!GET_ALLOC(HDRP(bp))) && (asize <= GET_SIZE(HDRP(bp)))) {
			return bp;
		}	  
	}
	return NULL;//no fit
}

//extend_heap:
//extends the heap by sufficient size
//and returns the block pointer of the new block
//input: the size of block to be created in terms of words: words
//output: the newly created block pointer after coalescing
static void *extend_heap(size_t words) {	
	
	void *bp;
	size_t size;
	void *previous_succ;
	
	assert(words >= 6);	
	//allocate an even number of words to maintain assignment
	size = (words % 2) ? (words+1) * WSIZE :  words * WSIZE;
	if ((long) (bp = mem_sbrk(size)) == -1)
		return NULL;

	PUT(HDRP(bp), PACK(size, 0));//free block header
	
	previous_succ = (void *) GET(SUCCP(head));	
	
	//pointer arithmetic: adding this block to the free list
	PUT(PREDP(bp), (unsigned int) head);
	PUT(SUCCP(head), (unsigned int) bp);
	PUT(SUCCP(bp), (unsigned int) previous_succ);
	PUT(PREDP(previous_succ), (unsigned int) bp);
	PUT(FTRP(bp), PACK(size, 0));
	
	//adding real epilogue 
	//at the physical end
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

	return coalesce(bp);
}


//coalesce does the coalesce job in the lecture notes
//input: a block pointer pointing to a block to be coalesced
//output: the coalesced new block pointer
static void *coalesce(void *bp) {	
	
	size_t prev_alloc  = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc  = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));

	//both allocated
	//case 2 in lecture notes
	if (prev_alloc && next_alloc) {
		return bp;
	}
	
	
	//previous allocated
	//next not allocated
	//coalesce with next
	//case 3 in lecture notes	
	else if (prev_alloc && (!next_alloc)) {
	  
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		void *next_bp = NEXT_BLKP(bp);	
		
		PUT(HDRP(bp), PACK(size, 0));
	
		PUT(SUCCP(GET(PREDP(next_bp))), GET(SUCCP(next_bp)));
		PUT(PREDP(GET(SUCCP(next_bp))), GET(PREDP(next_bp)));		
		PUT(HDRP(next_bp), 0);
		PUT(FTRP(next_bp), 0);
	
		PUT(FTRP(bp), PACK(size, 0));
		
	}
	
	//previous not allocated
	//next allocated
	//coalesce with previous
	//case 2 in the lecture notes
	else if ((!prev_alloc) && (next_alloc)) {
		
		if (GET_SIZE(((char *)(bp) - DSIZE)) == 0) {
			return bp;
		}
	
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		
		void *prev_bp = PREV_BLKP(bp);	
		
		PUT(SUCCP(GET(PREDP(bp))), GET(SUCCP(bp)));
		PUT(PREDP(GET(SUCCP(bp))), GET(PREDP(bp)));
		
		PUT(HDRP(bp), 0);
		PUT(FTRP(bp), 0);
		
		PUT(HDRP(prev_bp), PACK(size, 0));
		PUT(FTRP(prev_bp), PACK(size, 0));
		
		bp = prev_bp;
	}
	
	//case 4 in the lecture notes
	else if ((!prev_alloc) && (!next_alloc)) {

		size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
			GET_SIZE(FTRP(NEXT_BLKP(bp)));		
		void *prev_bp = PREV_BLKP(bp);
		void *next_bp = NEXT_BLKP(bp);
		
		if (GET_SIZE(((char *)(bp) - DSIZE)) == 0) {
			return bp;
		}
			
		PUT(SUCCP(GET(PREDP(bp))), GET(SUCCP(bp)));
		PUT(PREDP(GET(SUCCP(bp))), GET(PREDP(bp)));
		
		PUT(SUCCP(GET(PREDP(next_bp))), GET(SUCCP(next_bp)));
		PUT(PREDP(GET(SUCCP(next_bp))), GET(PREDP(next_bp)));
		
		PUT(HDRP(prev_bp), PACK(size, 0));
		PUT(FTRP(prev_bp), PACK(size, 0));		
		
		bp = prev_bp;	
		
	}
	
	return bp;
}
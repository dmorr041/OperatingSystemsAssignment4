#include <math.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define INIT_SUCCESS 1
#define INIT_FAIL -1
#define CREATE_BLOCK_SUCCESS 1
#define BLOCK_CREATE_FAIL 0
#define VALID_POINTER 1
#define INVALID_POINTER 0
#define FREE_SUCCESS 0
#define FREE_FAILED -1

#define MEM_POLICY_FIRSTFIT 0
#define MEM_POLICY_BESTFIT 1
#define MEM_POLICY_WORSTFIT 2

// Structure for the blocks being allocated within the initialized memory block
typedef struct block
{
	int size;				// Size of the block
	int free;				// Whether or not the block is allocated (1 = free, 0 = allocated)
	void* start;			// Where the block starts in memory
	void* end;				// Where the block ends in memory
	struct block *next;		// Pointer to the next block in our list
} Block;

// Sructure for the memory block initialized
typedef struct mblock
{
	int policy;				// Flag for the memory allocation algorithm
	int size;				// Size of the memory block we're intializing
	int init;				// Flag for initialization (1 = initialized, 0 = not initialized)
	int free_size;			// Free space in the mblock
	int remainder_size;		// Remainder space
	Block *head;			// The first "block"
} Mblock;

void print_block(Block *block);
void print_mblock();
void *FirstFit(int size);
void *BestFit(int size);
void *WorstFit(int size);
void *Fit(int size, int policy);
void *merge(Block *block1, Block *block2);

// Initialize an mblock object
Mblock mblock;







/* 	Function to create a "block", essentially adding it to our linked
	list structure.

	@param Block *current - the block we want to add to the list
	@param int size - the size of the block we want to add
*/
int create_block(Block *current, int size)
{
	if(current->next == NULL)
	{
		current->start = current;
		current->start = (current->start + sizeof(Block));
		current->size = size;
		current->free = 0;
		current->end = (current->start + (size));
		current->next = (current->end + 1);
		mblock.remainder_size = mblock.remainder_size - size;
		mblock.free_size = mblock.free_size - size;
	}
	else
	{
		int previous_size = current->size;
		
		Block *new_block = current + size + sizeof(Block *);
		new_block->end = current->end;
		
		current->size = size;
		new_block->next = current->next;
		current->free = 0;
		current->start = current->start;
		current->end = (current->start + (size));
		
		new_block->size = previous_size - size;
		current->next = new_block;
		new_block->free = 1;
		new_block->start = (current->next + sizeof(Block));
		
		mblock.remainder_size = mblock.remainder_size - size;
		mblock.free_size = mblock.free_size - size;
	}
	
	return CREATE_BLOCK_SUCCESS;
}







/* 	Helper function to allocate memory using the first fit algorithm 
	
	@param int size - the size of the block
	@return - pointer to the block
*/
void *FirstFit(int size)
{
	Block *current = mblock.head;
	
	while(((!current->free) || (current->size < size)) && (current->next != NULL))
	{
		current = (Block *)current->next;
	}
	
	create_block(current, size);
	
	return (void *)current;
}








/* 	Function to allocate memory according to either the best or worst fit algorithm 
	
	@param int size - the size of the block we want to allocate
	@param int policy - the policy number corresponding to the fitting algorithm
	@return - pointer to the allocated block
*/
void *Fit(int size, int policy)
{
	Block *best = NULL;
	Block *worst = NULL;
	Block *current = mblock.head;

	// Find the best and worst fitting blocks
	while(current->next != NULL)
	{
		if(current->free)
		{
			if(best == NULL)
				best = current;
			
			if(worst == NULL)
				worst = current;
			
			if((best->size - size) > (current->size - size))
				best = current;
			
			if((worst->size - size) < (current->size - size))
				worst = current;
		}
		
		current = (Block *)current->next;
	}
	
	// Create a new block depending on the algorithm, passing
	// in our updated best and worst fitting blocks
	if(policy == MEM_POLICY_BESTFIT)
	{
		create_block(best, size);
	}
	else if(policy == MEM_POLICY_WORSTFIT)
	{
		create_block(worst, size);
	}
	else
	{
		perror("Policy must be best fit or worst fit.\n");
		return NULL;
	}
	
	return (void*) current;
}






/* 	Helper function to allocate according to the best fit algorithm. 
	Calls Fit() and passes in the right policy.
	
	@param int size - the size of the block we want to allocate
	@return - pointer to the allocated block
*/
void *BestFit(int size)
{
	return Fit(size, MEM_POLICY_BESTFIT);
}







/* 	Helper function to allocate according to the worst fit algorithm. Calls Fit() and passes in the right policy 

	@param int size - the size of the block we want to allocate
	@return - pointer to the allocated block
*/
void *WorstFit(int size)
{
	return Fit(size, MEM_POLICY_WORSTFIT);
}







/* 	Function to "merge" two blocks of free memory together.
	
	@param Block *block1 - the first block
	@param Block *block2 - the second block
	@return - pointer to the first block, which is now both blocks
*/
void *merge(Block *block1, Block *block2)
{
	block1->next = block2->next;
	block1->end = block2->end;
	block1->size = block1->size + block2->size;
	block2->next = NULL;
	
	return block1;
}








/* 	This function initializes a memory block, requesting memory from the OS using mmap() function.
	
	@param int size - the size of the memory block we're requesting from OS
	@param int policy - the "policy" aka algorithm we're using to allocate memory
 */
int Mem_Init(int size, int policy)
{
	if(!(policy == MEM_POLICY_FIRSTFIT || policy == MEM_POLICY_BESTFIT || policy == MEM_POLICY_WORSTFIT))
	{
		fprintf(stderr, "Invalid policy. Policy number must be %d, %d, or %d\n", 
					MEM_POLICY_FIRSTFIT, MEM_POLICY_BESTFIT, MEM_POLICY_WORSTFIT);
		return INIT_FAIL;
	}
	
	if(mblock.init)
		return INIT_FAIL;
	
	void *ptr;
	
	int page_size = getpagesize();
	int adjusted_size = (int)ceil((size + sizeof(Mblock)) / (double)page_size) * page_size;
	
	int fd = open("/dev/zero", O_RDWR);
	ptr = mmap(NULL, adjusted_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);

	if(ptr == MAP_FAILED)
	{
		perror("mmap failed");
		return INIT_FAIL;
	}
	
	close(fd);

	mblock.policy = policy;
	mblock.size = adjusted_size;
	mblock.init = 1;
	mblock.free_size = size;
	mblock.remainder_size = size;
	mblock.head = (Block *) ptr;
	mblock.head = mblock.head + sizeof(Mblock);
	
	return INIT_SUCCESS;
}







/*	Function to allocate blocks of memory within the 
	memory block initialized by Mem_Init().
	
	@param int size - the size of the block we want to allocate
	@return - pointer to the block we allocated
	
*/
void *Mem_Alloc(int size)
{
	// Make sure we have enough free space left
	if(mblock.free_size < size)
	{
		return NULL;
	}
	
	//Call the appropriate algorithm
	if(mblock.policy == MEM_POLICY_FIRSTFIT)
		return FirstFit(size);
	else if(mblock.policy == MEM_POLICY_BESTFIT)
		return BestFit(size);
	else if(mblock.policy == MEM_POLICY_WORSTFIT)
		return WorstFit(size);
	else
		return NULL;
}







/* 	Function to free blocks of memory previously allocated by Mem_Alloc() 
	
	@param void *ptr - pointer to the block of memory we want to free
	@return - 0 for success, -1 for failure
*/
int Mem_Free(void *ptr)
{
	if(ptr == NULL)
		return FREE_FAILED;
	
	Block *current = NULL;
	Block *previous = NULL;
	
	current = mblock.head;
	
	while(current->next != NULL)
	{
		if(((Block *)ptr >= current) && (ptr <= current->end))
		{
			if(!current->free)
			{
				current->free = 1;
				mblock.free_size += current->size;
				
				if(previous != NULL)
				{
					if(previous->free)
					{
						current = merge(previous, current);
					}
				}
				
				if(current->next->free)
				{
					current = merge(current, current->next);
				}
				
				return FREE_SUCCESS;
			}
			else
			{
				return FREE_FAILED;
			}	
		}
		
		previous = current;
		
		current = (Block *)current->next;
	}
	
	return FREE_FAILED;
}







/* 	Function to determine whether or not an allocated block is valid. 
	
	@param void *ptr - pointer to the block we want to validate
	@return - 1 for valid pointer, 0 for invalid pointer
*/
int Mem_isValid(void *ptr)
{
	Block *current = mblock.head;
	
	while(current->next != NULL)
	{
		if((ptr >= current->start) && (ptr <= current->end))
		{
			if(!current->free)
				return VALID_POINTER;
		}
		
		current = (Block *)current->next;
	}
	
	return INVALID_POINTER;
}






/* 	Function to get the size of a block. 
	
	@param void *ptr - pointer to the block
	@return - the size of the block
*/
int Mem_GetSize(void *ptr)
{
	Block *current = mblock.head;
	
	while(current->next != NULL)
	{
		if((ptr >= current->start) && (ptr <= current->end))
			return current->size;
		
		current = (Block *)current->next;
	}
	
	return -1;
}






/* 	Function to find the fragmentation factor, which
	is largest free chunk size / total free memory.
	
	@return - the fragmentation factor
*/
float Mem_GetFragmentation()
{
	Block *current = mblock.head;
	Block *largest = NULL;
	
	float total_freed_size = 0.0f;
	float fragmentation_factor = 0.0f;
	
	if(current->next == NULL)
		return 1.0f;
	
	while(current->next != NULL)
	{
		if(current->free)
		{
			if(largest == NULL)
				largest = current;
			
			if(current->size > largest->size)
				largest = current;
			
			total_freed_size = total_freed_size + current->size;
		}
		
		current = (Block *)current->next;
	}
	
	if(largest == NULL)
		return 1.0f;
	
	int largestBlock = largest->size;
	
	if(mblock.remainder_size > largest->size)
	{
		largestBlock = mblock.remainder_size;
		
		fragmentation_factor = largestBlock / (float) (total_freed_size + mblock.remainder_size);
	}
	else
	{
		fragmentation_factor = largestBlock / (float) (total_freed_size + mblock.remainder_size);
	}
	
	// If there's no free space available
	if(total_freed_size == largest->size)
		return 1.0f;
	
	if((mblock.free_size - mblock.remainder_size) == 0)
		return 1.0f;
	
	return fragmentation_factor;
}

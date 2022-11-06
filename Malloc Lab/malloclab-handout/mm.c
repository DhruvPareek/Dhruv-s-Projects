/*
-  Simple allocator based on implicit free lists, first fit placement, and boundary tag coalescing.
*          *
*          * Each block has header and footer of the form:
*          *
*          *      63       32   31        1   0
*          *      --------------------------------
*          *     |   unused   | block_size | a/f |
*          *      --------------------------------
*          *
*          * a/f is 1 iff the block is allocated. The list has the following form:
*          *
*          * begin                                       end
*          * heap                                       heap
*          *  ----------------------------------------------
*          * | hdr(8:a) | zero or more usr blks | hdr(0:a) |
*          *  ----------------------------------------------
*          * | prologue |                       | epilogue |
*          * | block    |                       | block    |
*          *
*          * The allocated prologue and epilogue blocks are overhead that
*          * eliminate edge conditions during coalescing.
*/
/*
* Doubly linked explicit list where prologue points to first free block and last free block points to null.
* Every newly freed block is placed first in the explicit list.
* Malloc searches start at the first block in the free list and continue through the doubly linked list.
 
* Segregated Free List where there is an initial block in the heap that holds an array of pointers to each
* list of different sizes.
* List sizes:
* 0-32 bytes        0-2^5 bytes
* 33-64 bytes       2^5 +1 to 2^6
* 65-128 bytes      2^6 +1 to 2^7
* 129-256 bytes     2^7 +1 to 2^8
* 257-512 bytes     2^8 +1 to 2^9
* 513-1024 bytes    2^9 +1 to 2^10
* 1025-2048 bytes   2^10 +1 to 2^11
* 2049-4096 bytes   2^11 +1 to 2^12
* 4097-8192 bytes   2^12 +1 to 2^13
* 8193+             2^13 +1 to inf
*/
#include "memlib.h"
#include "mm.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/* Your info */
team_t team = {
  /* First and last name */
  "Dhruv Pareek",
  /* UID */
  "705716046",
  /* Custom message (16 chars) */
  "not dusty",
};
typedef struct {
  uint32_t allocated : 1;
  uint32_t block_size : 31;
  uint32_t _;
} header_t;
typedef header_t footer_t;
typedef struct {
  uint32_t allocated : 1;
  uint32_t block_size : 31;
  uint32_t _;
  union {
      struct {
          struct block_t* next;
          struct block_t* prev;
      };
      int payload[0];
  } body;
} block_t;
/* This enum can be used to set the allocated bit in the block */
enum block_state { FREE,
                 ALLOC };
#define CHUNKSIZE (1 << 16) /* initial heap size (bytes) */
#define OVERHEAD (sizeof(header_t) + sizeof(footer_t)) /* overhead of the header and footer of an allocated block */
#define MIN_BLOCK_SIZE (32) /* the minimum block size needed to keep in a freelist (header + footer + next pointer + prev pointer) */
/* Global variables */
static block_t *prologue; /* pointer to first block */
//pointers to beginning of free block linked lists
static block_t *head5 = NULL; //0-32 bytes        0-2^5 bytes
static block_t *head6 = NULL; //33-64 bytes       2^5 +1 to 2^6
static block_t *head7 = NULL;  //65-128 bytes      2^6 +1 to 2^7
static block_t *head8 = NULL;  //129-256 bytes     2^7 +1 to 2^8
static block_t *head9 = NULL;  //257-512 bytes     2^8 +1 to 2^9
static block_t *head10 = NULL;  //513-1024 bytes    2^9 +1 to 2^10
static block_t *head11 = NULL;  //1025-2048 bytes   2^10 +1 to 2^11
static block_t *head12 = NULL;  //2049-4096 bytes   2^11 +1 to 2^12
static block_t *head13 = NULL;  //4097-8192 bytes   2^12 +1 to 2^13
static block_t *headinf = NULL;  //8193+             2^13 +1 to inf
 /* function prototypes for internal helper routines */
static block_t *extend_heap(size_t words);
static void place(block_t *block, size_t asize);
static block_t *find_fit(size_t asize);
static block_t *coalesce(block_t *block);
static footer_t *get_footer(block_t *block);
static void printblock(block_t *block);
static void checkblock(block_t *block);
static void removefreeblock(block_t *block);
static void insertfreeblock(block_t *block);
static void debug_explicit_list(int depth);
static void printFreeListLength();
static void checkExpListsForAllocation();
static void printListSizes();
int initCount = 0;
 
/*
*  * mm_init - Initialize the memory manager
*  * Only think I added here was pointing the head of the explicit list
*  * at the this newly freed block.
*   */
/* $begin mminit */
int mm_init(void) {
   initCount++;
  printf("entering init #%d \n", initCount);
  /* create the initial empty heap */
  if ((prologue = mem_sbrk(CHUNKSIZE)) == (void*)-1)
      return -1;
  /* initialize the prologue */
  prologue->allocated = ALLOC;
  prologue->block_size = sizeof(header_t);
  /* initialize the first free block */
  block_t *init_block = (void *)prologue + sizeof(header_t);
  init_block->allocated = FREE;
  init_block->block_size = CHUNKSIZE - OVERHEAD;
  footer_t *init_footer = get_footer(init_block);
  init_footer->allocated = FREE;
  init_footer->block_size = init_block->block_size;
  /* initialize the epilogue - block size 0 will be used as a terminating condition */
  block_t *epilogue = (void *)init_block + init_block->block_size;
  epilogue->allocated = ALLOC;
  epilogue->block_size = 0;
 
  //point innit's next pointer at null
  init_block->body.next = NULL;
 
   insertfreeblock(init_block);
   printListSizes();
   mm_checkheap(0);//DELETE
   printListSizes();
  return 0;
}
/* $end mminit */
/*
*  * mm_malloc - Allocate a block with at least size bytes of payload
*  * Does everything the sam as how it, i just changed the find_fit
*  * and place functions's implementation
*   */
/* $begin mmmalloc */
void *mm_malloc(size_t size) {
  uint32_t asize;       /* adjusted block size */
  uint32_t extendsize;  /* amount to extend heap if no fit */
  uint32_t extendwords; /* number of words to extend heap if no fit */
  block_t *block;
   printf("entering malloc, #bytes = %d \n", size);
  
   mm_checkheap(0);//DELETE
  /* Ignore spurious requests */
  if (size == 0)
      return NULL;
  /* Adjust block size to include overhead and alignment reqs. */
  size += OVERHEAD;
  asize = ((size + 7) >> 3) << 3; /* align to multiple of 8 */
   if (asize < MIN_BLOCK_SIZE) {
      asize = MIN_BLOCK_SIZE;
  }
  /* Search the free list for a fit */
  if ((block = find_fit(asize)) != NULL) {
      place(block, asize);
      return block->body.payload;
  }
  /* No fit found. Get more memory and place the block */
  extendsize = (asize > CHUNKSIZE) // extend by the larger of the two
                   ? asize
                   : CHUNKSIZE;
  extendwords = extendsize >> 3; // extendsize/8
  if ((block = extend_heap(extendwords)) != NULL) {
      place(block, asize);
      return block->body.payload;
  }
  /* no more memory :( */
  return NULL;
}
/* $end mmmalloc */
/*
*  * mm_free - Free a block
*  * all I changed here was calling the insertfreeblock function
*   */
/* $begin mmfree */
void mm_free(void *payload) {
  block_t *block = payload - sizeof(header_t);
  printf("entering free, #bytes = %d \n", block->block_size);
  block->allocated = FREE;
  footer_t *footer = get_footer(block);
  footer->allocated = FREE;
  insertfreeblock(block);
  coalesce(block);
  //mm_checkheap(0);
   //printListSizes();
 
}
/* $end mmfree */
/*
*  * mm_realloc - naive implementation of mm_realloc
*   * NO NEED TO CHANGE THIS CODE!
*    */
void *mm_realloc(void *ptr, size_t size) {
   printf("reallocating\n");
  void *newp;
  size_t copySize;
  if ((newp = mm_malloc(size)) == NULL) {
      printf("ERROR: mm_malloc failed in mm_realloc\n");
      exit(1);
  }
  block_t* block = ptr - sizeof(header_t);
  copySize = block->block_size;
  if (size < copySize)
      copySize = size;
  memcpy(newp, ptr, copySize);
  mm_free(ptr);
  return newp;
}
/*
*  * mm_checkheap - Check the heap for consistency
*  * ADD CHECK FOR SEGREGATION WHEE CHECK TO SEE IF EXTENDED HEAP WHEN THERE IS A FREE BLOCK
*  * THAT WOULD HAVE WORKED
*/
 
void mm_checkheap(int verbose) {
  block_t *block = prologue;
  if (verbose)
      printf("Heap (%p):\n", prologue);
  if (block->block_size != sizeof(header_t) || !block->allocated)
      printf("Bad prologue header\n");
  checkblock(prologue);
  // iterate through the heap (both free and allocated blocks will be present)
  for (block = (void*)prologue+prologue->block_size; block->block_size > 0; block = (void *)block + block->block_size) {
      if (verbose)
          printblock(block);
      checkblock(block);
  }
  if (verbose)
      printblock(block);
  if (block->block_size != 0 || !block->allocated)
      printf("Bad epilogue header\n");
   //call function which checks if the doubly linked list matches
   //when you traverse it forwards and backwards
   //debug_explicit_list(1000);
 
 
  //iterate through each free list to see if every block is free
   checkExpListsForAllocation();
  /*
  //go through every block to check that all free blocks are in the explicit list
  for(block_t *b = (void*)prologue + prologue->block_size; b->block_size > 0; b = (void *)b + b->block_size)
  {
      if(b->allocated == FREE){
          bool foundBlock = false;
          for(block_t *temp = explicitHead; temp != NULL; temp = temp->body.next){
              if(temp == b){
                  foundBlock = true;
              }
          }
          if(foundBlock == false){
              printf("All free blocks not in explicit list\n");
          }
      }
  }
  */
 
 
  //check to see if all blocks coalesced
   block_t *b = (void*)prologue + prologue->block_size;
   int prevBlockAllocated = b->allocated;
   if(b->block_size > 0){
       b = (void *)b + b->block_size;
   }
  for (; b->block_size > 0; b = (void *)b + b->block_size) {
      if (prevBlockAllocated == FREE && b->allocated == FREE) {
            printf("not proper coalescing\n");
      }
      prevBlockAllocated = b->allocated;
  }
 
}
/* The remaining routines are internal helper routines */
 
/*
*  * extend_heap - Extend heap with free block and return its block pointer
*  * Here I added the call to insert this newly freed block which is the extension
*  * to the explicit linked list. Then the coalesce call will combine this block with
*  * a freed block behind it.
*   */
/* $begin mmextendheap */
static block_t *extend_heap(size_t words) {
   printf("Extending Heap \n");
  block_t *block;
  uint32_t size;
  size = words << 3; // words*8
  if (size == 0 || (block = mem_sbrk(size)) == (void *)-1)
      return NULL;
  /* The newly acquired region will start directly after the epilogue block */
  /* Initialize free block header/footer and the new epilogue header */
  /* use old epilogue as new free block header */
  block = (void *)block - sizeof(header_t);
  block->allocated = FREE;
  block->block_size = size;
  /* free block footer */
  footer_t *block_footer = get_footer(block);
  block_footer->allocated = FREE;
  block_footer->block_size = block->block_size;
  /* new epilogue header */
  header_t *new_epilogue = (void *)block_footer + sizeof(header_t);
  new_epilogue->allocated = ALLOC;
  new_epilogue->block_size = 0;
  insertfreeblock(block);
  /* Coalesce if the previous block was free */
  return coalesce(block);
}
/* $end mmextendheap */
/*
*  * place - Place block of asize bytes at start of free block block
*   *         and split if remainder would be at least minimum block size
*   * If the block splinters on a malloc call, remove the whole block from
*   * the doubly linked list then insert the free part of the whole block
*   * into the free list.
*   * If there is no splintering, remove the whole block from the free list.
*    */
/* $begin mmplace */
static void place(block_t *block, size_t asize) {
  size_t split_size = block->block_size - asize;
  if (split_size >= MIN_BLOCK_SIZE) {
      /* split the block by updating the header and marking it allocated*/
       removefreeblock(block);
      block->block_size = asize;
      block->allocated = ALLOC;
      /* set footer of allocated block*/
      footer_t *footer = get_footer(block);
      footer->block_size = asize;
      footer->allocated = ALLOC;
      /* update the header of the new free block */
      block_t *new_block = (void *)block + block->block_size;
      new_block->block_size = split_size;
      new_block->allocated = FREE;
      /* update the footer of the new free block */
      footer_t *new_footer = get_footer(new_block);
      new_footer->block_size = split_size;
      new_footer->allocated = FREE;
      insertfreeblock(new_block);
  } else {
      /* splitting the block will cause a splinter so we just include it in the allocated block */
      block->allocated = ALLOC;
      footer_t *footer = get_footer(block);
      footer->allocated = ALLOC;
      removefreeblock(block);
  }
}
/* $end mmplace */
/*
*  * find_fit - Find a fit for a block with asize bytes
*  * Start at the head of the explicit free list then traverse through
*  * to check if there is a free block that fits.
*   */
static block_t *find_fit(size_t asize) {
  /* first fit search */
  block_t *b;
   int tempSize = asize;
   block_t *explicitHead = NULL;
 
   if(tempSize <= (1<<5)){
       explicitHead = head5;
       for (b = explicitHead; b != NULL; b = b->body.next) {
      /* block must be free and the size must be large enough to hold the request */
           if (!b->allocated && asize <= b->block_size) {
               return b;
           }
       }
   }
   if(tempSize <= (1<<6)){
       explicitHead = head6;
       for (b = explicitHead; b != NULL; b = b->body.next) {
      /* block must be free and the size must be large enough to hold the request */
           if (!b->allocated && asize <= b->block_size) {
               return b;
           }
       }
   }
   if(tempSize <= (1<<7)){
       explicitHead = head7;
       for (b = explicitHead; b != NULL; b = b->body.next) {
      /* block must be free and the size must be large enough to hold the request */
           if (!b->allocated && asize <= b->block_size) {
               return b;
           }
       }
   }
   if(tempSize <= (1<<8)){
       explicitHead = head8;
      for (b = explicitHead; b != NULL; b = b->body.next) {
      /* block must be free and the size must be large enough to hold the request */
           if (!b->allocated && asize <= b->block_size) {
               return b;
           }
       }
   }
   if(tempSize <= (1<<9)){
       explicitHead = head9;
       for (b = explicitHead; b != NULL; b = b->body.next) {
      /* block must be free and the size must be large enough to hold the request */
           if (!b->allocated && asize <= b->block_size) {
               return b;
           }
       }
   }
   if(tempSize <= (1<<10)){
       explicitHead = head10;
       for (b = explicitHead; b != NULL; b = b->body.next) {
      /* block must be free and the size must be large enough to hold the request */
           if (!b->allocated && asize <= b->block_size) {
               return b;
           }
       }
   }
   if(tempSize <= (1<<11)){
       explicitHead = head11;
       for (b = explicitHead; b != NULL; b = b->body.next) {
      /* block must be free and the size must be large enough to hold the request */
           if (!b->allocated && asize <= b->block_size) {
               return b;
           }
       }
   }
   if(tempSize <= (1<<12)){
       explicitHead = head12;
       for (b = explicitHead; b != NULL; b = b->body.next) {
      /* block must be free and the size must be large enough to hold the request */
           if (!b->allocated && asize <= b->block_size) {
               return b;
           }
       }
   }
   if(tempSize <= (1<<13)){
       explicitHead = head13;
       for (b = explicitHead; b != NULL; b = b->body.next) {
      /* block must be free and the size must be large enough to hold the request */
           if (!b->allocated && asize <= b->block_size) {
               return b;
           }
       }
   }
       explicitHead = headinf;
       for (b = explicitHead; b != NULL; b = b->body.next) {
      /* block must be free and the size must be large enough to hold the request */
           if (!b->allocated && asize <= b->block_size) {
               return b;
           }
       }
  
 
  return NULL; /* no fit */
}
/*
*  * coalesce - boundary tag coalescing. Return ptr to coalesced block
*  * each case is explained individualy
*   */
static block_t *coalesce(block_t *block) {
  footer_t *prev_footer = (void *)block - sizeof(header_t);
  header_t *next_header = (void *)block + block->block_size;
  bool prev_alloc = prev_footer->allocated;
  bool next_alloc = next_header->allocated;
  if (prev_alloc && next_alloc) { /* Case 1 */
       //printf("no coalescing \n");
      /* no coalesceing */
      return block;
  }
/* Case 2 - merge w next block by removing the next block from the list and changing headers for new sizes*/
  else if (prev_alloc && !next_alloc) {
       //printf("coalescing current block w block infront \n");
       removefreeblock((void *)block + block->block_size);
       removefreeblock(block);
      /* Update header of current block to include next block's size */
      block->block_size += next_header->block_size;
      /* Update footer of next block to reflect new size */
      footer_t *next_footer = get_footer(block);
      next_footer->block_size = block->block_size;
      insertfreeblock(block);
  }
/* Case 3 - merge w block behind by removing current block from the list and changing headers for new sizes*/
  else if (!prev_alloc && next_alloc) {
       //printf("coalescing current block w block behind \n");
      /* Update header of prev block to include current block's size */
      removefreeblock(block);
      block_t *prev_block = (void *)prev_footer - prev_footer->block_size + sizeof(header_t);
      removefreeblock(prev_block);
      prev_block->block_size += block->block_size;
      /* Update footer of current block to reflect new size */
      footer_t *footer = get_footer(prev_block);
      footer->block_size = prev_block->block_size;
      block = prev_block;
      insertfreeblock(block);
  }
/* Case 4 - merge w behind and infront - by removing current block and next block
from the list and changing headers for new sizes*/
  else {
      //printf("coalescing current block w block infront and behind \n");
      removefreeblock((void *)block + block->block_size);
      removefreeblock(block);
      /* Update header of prev block to include current and next block's size */
      block_t *prev_block = (void *)prev_footer - prev_footer->block_size + sizeof(header_t);
      removefreeblock(prev_block);
      prev_block->block_size += block->block_size + next_header->block_size;
      /* Update footer of next block to reflect new size */
      footer_t *next_footer = get_footer(prev_block);
      next_footer->block_size = prev_block->block_size;
      block = prev_block;
      insertfreeblock(block);
  }
  return block;
}
static footer_t* get_footer(block_t *block) {
  return (void*)block + block->block_size - sizeof(footer_t);
}
static void printblock(block_t *block) {
  uint32_t hsize, halloc, fsize, falloc;
  hsize = block->block_size;
  halloc = block->allocated;
  footer_t *footer = get_footer(block);
  fsize = footer->block_size;
  falloc = footer->allocated;
  if (hsize == 0) {
      printf("%p: EOL\n", block);
      return;
  }
  printf("%p: header: [%d:%c] footer: [%d:%c]\n", block, hsize,
         (halloc ? 'a' : 'f'), fsize, (falloc ? 'a' : 'f'));
}
static void checkblock(block_t *block) {
  if ((uint64_t)block->body.payload % 8) {
      printf("Error: payload for block at %p is not aligned\n", block);
  }
  footer_t *footer = get_footer(block);
  if (block->block_size != footer->block_size) {
      printf("Error: header does not match footer\n");
  }
}
 
//removing a free block from the explicit free list
static void removefreeblock(block_t *block)
{
   int tempSize = block->block_size;
   block_t *explicitHead;
 
   if(tempSize <= (1<<5)){
       if(head5 == NULL)
       {
           return;//nothing in explicit list
       }
       if(block->body.next == NULL && block->body.prev == NULL)
       {//if we are removing only block in list
         //printf("removing only free block in explicit list, #bytes = %d \n", block->block_size);
           head5 = NULL;
       }else if(block->body.prev == NULL && block->body.next != NULL){
       //remove the first block in the explicit list
         //printf("removing first free block in explicit list, #bytes = %d \n", block->block_size);
           head5 = block->body.next;
           head5->body.prev = NULL;
       }else if(block->body.prev != NULL && block->body.next == NULL){
       //remove the last item in the explicit free list
         //printf("removing last free block in explicit list, #bytes = %d \n", block->block_size);
           block_t *newLastBlock = block->body.prev;
           newLastBlock->body.next = NULL;
       }else if(block->body.prev != NULL && block->body.next != NULL){
       //remove block from middle of explicit list
        // printf("removing free block in middle of explicit list, #bytes = %d \n", block->block_size);
           block_t *blockBehind = block->body.prev;
           block_t *blockInfront = block->body.next;
           blockBehind->body.next = blockInfront;
           blockInfront->body.prev = blockBehind;
       }
   } else if(tempSize <= (1<<6)){
       if(head6 == NULL)
       {
           return;//nothing in explicit list
       }
       if(block->body.next == NULL && block->body.prev == NULL)
       {//if we are removing only block in list
         //printf("removing only free block in explicit list, #bytes = %d \n", block->block_size);
           head6 = NULL;
       }else if(block->body.prev == NULL && block->body.next != NULL){
       //remove the first block in the explicit list
         //printf("removing first free block in explicit list, #bytes = %d \n", block->block_size);
           head6 = block->body.next;
           head6->body.prev = NULL;
       }else if(block->body.prev != NULL && block->body.next == NULL){
       //remove the last item in the explicit free list
         //printf("removing last free block in explicit list, #bytes = %d \n", block->block_size);
           block_t *newLastBlock = block->body.prev;
           newLastBlock->body.next = NULL;
       }else if(block->body.prev != NULL && block->body.next != NULL){
       //remove block from middle of explicit list
        // printf("removing free block in middle of explicit list, #bytes = %d \n", block->block_size);
           block_t *blockBehind = block->body.prev;
           block_t *blockInfront = block->body.next;
           blockBehind->body.next = blockInfront;
           blockInfront->body.prev = blockBehind;
       }
   } else if(tempSize <= (1<<7)){
       if(head7 == NULL)
       {
           return;//nothing in explicit list
       }
       if(block->body.next == NULL && block->body.prev == NULL)
       {//if we are removing only block in list
         //printf("removing only free block in explicit list, #bytes = %d \n", block->block_size);
           head7 = NULL;
       }else if(block->body.prev == NULL && block->body.next != NULL){
       //remove the first block in the explicit list
         //printf("removing first free block in explicit list, #bytes = %d \n", block->block_size);
           head7 = block->body.next;
           head7->body.prev = NULL;
       }else if(block->body.prev != NULL && block->body.next == NULL){
       //remove the last item in the explicit free list
         //printf("removing last free block in explicit list, #bytes = %d \n", block->block_size);
           block_t *newLastBlock = block->body.prev;
           newLastBlock->body.next = NULL;
       }else if(block->body.prev != NULL && block->body.next != NULL){
       //remove block from middle of explicit list
        // printf("removing free block in middle of explicit list, #bytes = %d \n", block->block_size);
           block_t *blockBehind = block->body.prev;
           block_t *blockInfront = block->body.next;
           blockBehind->body.next = blockInfront;
           blockInfront->body.prev = blockBehind;
       }
   } else if(tempSize <= (1<<8)){
       if(head8 == NULL)
       {
           return;//nothing in explicit list
       }
       if(block->body.next == NULL && block->body.prev == NULL)
       {//if we are removing only block in list
         //printf("removing only free block in explicit list, #bytes = %d \n", block->block_size);
           head8 = NULL;
       }else if(block->body.prev == NULL && block->body.next != NULL){
       //remove the first block in the explicit list
         //printf("removing first free block in explicit list, #bytes = %d \n", block->block_size);
           head8 = block->body.next;
           head8->body.prev = NULL;
       }else if(block->body.prev != NULL && block->body.next == NULL){
       //remove the last item in the explicit free list
         //printf("removing last free block in explicit list, #bytes = %d \n", block->block_size);
           block_t *newLastBlock = block->body.prev;
           newLastBlock->body.next = NULL;
       }else if(block->body.prev != NULL && block->body.next != NULL){
       //remove block from middle of explicit list
        // printf("removing free block in middle of explicit list, #bytes = %d \n", block->block_size);
           block_t *blockBehind = block->body.prev;
           block_t *blockInfront = block->body.next;
           blockBehind->body.next = blockInfront;
           blockInfront->body.prev = blockBehind;
       }
   } else if(tempSize <= (1<<9)){
       if(head9 == NULL)
       {
           return;//nothing in explicit list
       }
       if(block->body.next == NULL && block->body.prev == NULL)
       {//if we are removing only block in list
         //printf("removing only free block in explicit list, #bytes = %d \n", block->block_size);
           head9 = NULL;
       }else if(block->body.prev == NULL && block->body.next != NULL){
       //remove the first block in the explicit list
         //printf("removing first free block in explicit list, #bytes = %d \n", block->block_size);
           head9 = block->body.next;
           head9->body.prev = NULL;
       }else if(block->body.prev != NULL && block->body.next == NULL){
       //remove the last item in the explicit free list
         //printf("removing last free block in explicit list, #bytes = %d \n", block->block_size);
           block_t *newLastBlock = block->body.prev;
           newLastBlock->body.next = NULL;
       }else if(block->body.prev != NULL && block->body.next != NULL){
       //remove block from middle of explicit list
        // printf("removing free block in middle of explicit list, #bytes = %d \n", block->block_size);
           block_t *blockBehind = block->body.prev;
           block_t *blockInfront = block->body.next;
           blockBehind->body.next = blockInfront;
           blockInfront->body.prev = blockBehind;
       }
   } else if(tempSize <= (1<<10)){
       if(head10 == NULL)
       {
           return;//nothing in explicit list
       }
       if(block->body.next == NULL && block->body.prev == NULL)
       {//if we are removing only block in list
         //printf("removing only free block in explicit list, #bytes = %d \n", block->block_size);
           head10 = NULL;
       }else if(block->body.prev == NULL && block->body.next != NULL){
       //remove the first block in the explicit list
         //printf("removing first free block in explicit list, #bytes = %d \n", block->block_size);
           head10 = block->body.next;
           head10->body.prev = NULL;
       }else if(block->body.prev != NULL && block->body.next == NULL){
       //remove the last item in the explicit free list
         //printf("removing last free block in explicit list, #bytes = %d \n", block->block_size);
           block_t *newLastBlock = block->body.prev;
           newLastBlock->body.next = NULL;
       }else if(block->body.prev != NULL && block->body.next != NULL){
       //remove block from middle of explicit list
        // printf("removing free block in middle of explicit list, #bytes = %d \n", block->block_size);
           block_t *blockBehind = block->body.prev;
           block_t *blockInfront = block->body.next;
           blockBehind->body.next = blockInfront;
           blockInfront->body.prev = blockBehind;
       }
   } else if(tempSize <= (1<<11)){
       if(head11 == NULL)
       {
           return;//nothing in explicit list
       }
       if(block->body.next == NULL && block->body.prev == NULL)
       {//if we are removing only block in list
         //printf("removing only free block in explicit list, #bytes = %d \n", block->block_size);
           head11 = NULL;
       }else if(block->body.prev == NULL && block->body.next != NULL){
       //remove the first block in the explicit list
         //printf("removing first free block in explicit list, #bytes = %d \n", block->block_size);
           head11 = block->body.next;
           head11->body.prev = NULL;
       }else if(block->body.prev != NULL && block->body.next == NULL){
       //remove the last item in the explicit free list
         //printf("removing last free block in explicit list, #bytes = %d \n", block->block_size);
           block_t *newLastBlock = block->body.prev;
           newLastBlock->body.next = NULL;
       }else if(block->body.prev != NULL && block->body.next != NULL){
       //remove block from middle of explicit list
        // printf("removing free block in middle of explicit list, #bytes = %d \n", block->block_size);
           block_t *blockBehind = block->body.prev;
           block_t *blockInfront = block->body.next;
           blockBehind->body.next = blockInfront;
           blockInfront->body.prev = blockBehind;
       }
   } else if(tempSize <= (1<<12)){
       if(head12 == NULL)
       {
           return;//nothing in explicit list
       }
       if(block->body.next == NULL && block->body.prev == NULL)
       {//if we are removing only block in list
         //printf("removing only free block in explicit list, #bytes = %d \n", block->block_size);
           head12 = NULL;
       }else if(block->body.prev == NULL && block->body.next != NULL){
       //remove the first block in the explicit list
         //printf("removing first free block in explicit list, #bytes = %d \n", block->block_size);
           head12 = block->body.next;
           head12->body.prev = NULL;
       }else if(block->body.prev != NULL && block->body.next == NULL){
       //remove the last item in the explicit free list
         //printf("removing last free block in explicit list, #bytes = %d \n", block->block_size);
           block_t *newLastBlock = block->body.prev;
           newLastBlock->body.next = NULL;
       }else if(block->body.prev != NULL && block->body.next != NULL){
       //remove block from middle of explicit list
        // printf("removing free block in middle of explicit list, #bytes = %d \n", block->block_size);
           block_t *blockBehind = block->body.prev;
           block_t *blockInfront = block->body.next;
           blockBehind->body.next = blockInfront;
           blockInfront->body.prev = blockBehind;
       }
   } else if(tempSize <= (1<<13)){
       if(head13 == NULL)
       {
           return;//nothing in explicit list
       }
       if(block->body.next == NULL && block->body.prev == NULL)
       {//if we are removing only block in list
         //printf("removing only free block in explicit list, #bytes = %d \n", block->block_size);
           head13 = NULL;
       }else if(block->body.prev == NULL && block->body.next != NULL){
       //remove the first block in the explicit list
         //printf("removing first free block in explicit list, #bytes = %d \n", block->block_size);
           head13 = block->body.next;
           head13->body.prev = NULL;
       }else if(block->body.prev != NULL && block->body.next == NULL){
       //remove the last item in the explicit free list
         //printf("removing last free block in explicit list, #bytes = %d \n", block->block_size);
           block_t *newLastBlock = block->body.prev;
           newLastBlock->body.next = NULL;
       }else if(block->body.prev != NULL && block->body.next != NULL){
       //remove block from middle of explicit list
        // printf("removing free block in middle of explicit list, #bytes = %d \n", block->block_size);
           block_t *blockBehind = block->body.prev;
           block_t *blockInfront = block->body.next;
           blockBehind->body.next = blockInfront;
           blockInfront->body.prev = blockBehind;
       }
   } else if(tempSize > (1<<13)){
       if(headinf == NULL)
       {
           return;//nothing in explicit list
       }
       if(block->body.next == NULL && block->body.prev == NULL)
       {//if we are removing only block in list
         //printf("removing only free block in explicit list, #bytes = %d \n", block->block_size);
           headinf = NULL;
       }else if(block->body.prev == NULL && block->body.next != NULL){
       //remove the first block in the explicit list
         //printf("removing first free block in explicit list, #bytes = %d \n", block->block_size);
           headinf = block->body.next;
           headinf->body.prev = NULL;
       }else if(block->body.prev != NULL && block->body.next == NULL){
       //remove the last item in the explicit free list
         //printf("removing last free block in explicit list, #bytes = %d \n", block->block_size);
           block_t *newLastBlock = block->body.prev;
           newLastBlock->body.next = NULL;
       }else if(block->body.prev != NULL && block->body.next != NULL){
       //remove block from middle of explicit list
        // printf("removing free block in middle of explicit list, #bytes = %d \n", block->block_size);
           block_t *blockBehind = block->body.prev;
           block_t *blockInfront = block->body.next;
           blockBehind->body.next = blockInfront;
           blockInfront->body.prev = blockBehind;
       }
   }
 
 
}
 
//determine which explicit list this block should go in
//append block to beginning of that explicit list
static void insertfreeblock(block_t *block)
{
   int tempSize = block->block_size;
   block_t *explicitHead;
 
   if(tempSize <= (1>>5)){
       if(head5 == NULL){//if we are adding the only item into the explicit free list
         //printf("inserting only free block in explicit list, #bytes = %d \n", block->block_size);
       head5 = block;
       block->body.next = NULL;
       block->body.prev = NULL;
       return;
       }
 
   //if there is already at least one block in free list
     //printf("inserting a free block in explicit list before other free blocks, #bytes = %d \n", block->block_size);
   block_t *secondBlock = head5;
   head5 = block;
   block->body.next = secondBlock;
   secondBlock->body.prev = head5;
   head5->body.prev = NULL;
   } else if(tempSize <= (1<<6)){
       if(head6 == NULL){//if we are adding the only item into the explicit free list
         //printf("inserting only free block in explicit list, #bytes = %d \n", block->block_size);
       head6 = block;
       block->body.next = NULL;
       block->body.prev = NULL;
       return;
       }
 
   //if there is already at least one block in free list
     //printf("inserting a free block in explicit list before other free blocks, #bytes = %d \n", block->block_size);
   block_t *secondBlock = head6;
   head6 = block;
   block->body.next = secondBlock;
   secondBlock->body.prev = head6;
   head6->body.prev = NULL;
   } else if(tempSize <= (1<<7)){
       if(head7 == NULL){//if we are adding the only item into the explicit free list
         //printf("inserting only free block in explicit list, #bytes = %d \n", block->block_size);
       head7 = block;
       block->body.next = NULL;
       block->body.prev = NULL;
       return;
       }
 
   //if there is already at least one block in free list
     //printf("inserting a free block in explicit list before other free blocks, #bytes = %d \n", block->block_size);
   block_t *secondBlock = head7;
   head7 = block;
   block->body.next = secondBlock;
   secondBlock->body.prev = head7;
   head7->body.prev = NULL;
   } else if(tempSize <= (1<<8)){
   if(head8 == NULL){//if we are adding the only item into the explicit free list
         //printf("inserting only free block in explicit list, #bytes = %d \n", block->block_size);
       head8 = block;
       block->body.next = NULL;
       block->body.prev = NULL;
       return;
       }
 
   //if there is already at least one block in free list
     //printf("inserting a free block in explicit list before other free blocks, #bytes = %d \n", block->block_size);
   block_t *secondBlock = head8;
   head8 = block;
   block->body.next = secondBlock;
   secondBlock->body.prev = head8;
   head8->body.prev = NULL;
   } else if(tempSize <= (1<<9)){
   if(head9 == NULL){//if we are adding the only item into the explicit free list
         //printf("inserting only free block in explicit list, #bytes = %d \n", block->block_size);
       head9 = block;
       block->body.next = NULL;
       block->body.prev = NULL;
       return;
       }
 
   //if there is already at least one block in free list
     //printf("inserting a free block in explicit list before other free blocks, #bytes = %d \n", block->block_size);
   block_t *secondBlock = head9;
   head9 = block;
   block->body.next = secondBlock;
   secondBlock->body.prev = head9;
   head9->body.prev = NULL;
   } else if(tempSize <= (1<<10)){
           if(head10 == NULL){//if we are adding the only item into the explicit free list
         //printf("inserting only free block in explicit list, #bytes = %d \n", block->block_size);
       head10 = block;
       block->body.next = NULL;
       block->body.prev = NULL;
       return;
       }
 
   //if there is already at least one block in free list
     //printf("inserting a free block in explicit list before other free blocks, #bytes = %d \n", block->block_size);
   block_t *secondBlock = head10;
   head10 = block;
   block->body.next = secondBlock;
   secondBlock->body.prev = head10;
   head10->body.prev = NULL;
   } else if(tempSize <= (1<<11)){
       if(head11 == NULL){//if we are adding the only item into the explicit free list
         //printf("inserting only free block in explicit list, #bytes = %d \n", block->block_size);
       head11 = block;
       block->body.next = NULL;
       block->body.prev = NULL;
       return;
       }
 
   //if there is already at least one block in free list
     //printf("inserting a free block in explicit list before other free blocks, #bytes = %d \n", block->block_size);
   block_t *secondBlock = head11;
   head11 = block;
   block->body.next = secondBlock;
   secondBlock->body.prev = head11;
   head11->body.prev = NULL;
   } else if(tempSize <= (1<<12)){
       if(head12 == NULL){//if we are adding the only item into the explicit free list
         //printf("inserting only free block in explicit list, #bytes = %d \n", block->block_size);
       head12 = block;
       block->body.next = NULL;
       block->body.prev = NULL;
       return;
       }
 
   //if there is already at least one block in free list
     //printf("inserting a free block in explicit list before other free blocks, #bytes = %d \n", block->block_size);
   block_t *secondBlock = head12;
   head12 = block;
   block->body.next = secondBlock;
   secondBlock->body.prev = head12;
   head12->body.prev = NULL;
   } else if(tempSize <= (1<<13)){
           if(head13 == NULL){//if we are adding the only item into the explicit free list
         //printf("inserting only free block in explicit list, #bytes = %d \n", block->block_size);
       head13 = block;
       block->body.next = NULL;
       block->body.prev = NULL;
       return;
       }
 
   //if there is already at least one block in free list
     //printf("inserting a free block in explicit list before other free blocks, #bytes = %d \n", block->block_size);
   block_t *secondBlock = head13;
   head13 = block;
   block->body.next = secondBlock;
   secondBlock->body.prev = head13;
   head13->body.prev = NULL;
   } else if(tempSize > (1<<13)){
       if(headinf == NULL){//if we are adding the only item into the explicit free list
         //printf("inserting only free block in explicit list, #bytes = %d \n", block->block_size);
       headinf = block;
       block->body.next = NULL;
       block->body.prev = NULL;
       return;
       }
 
   //if there is already at least one block in free list
     //printf("inserting a free block in explicit list before other free blocks, #bytes = %d \n", block->block_size);
   block_t *secondBlock = headinf;
   headinf = block;
   block->body.next = secondBlock;
   secondBlock->body.prev = headinf;
   headinf->body.prev = NULL;
   }
 
}
 
 
/************************DEBUG***************************************/
/************************DEBUG***************************************/
/************************DEBUG***************************************/
/************************DEBUG***************************************//*
static  uint32_t global_counter = 0;
static void debug_explicit_list(int depth) {
 global_counter++;
 printf("\nDEBUG EXPLICIT LIST: %d\n", global_counter);
 
 if (explicitHead == NULL) {
   printf("0 elements.\n");
   return;
 }
 
 int f_len = 0;
 int b_len = 0;
 
 // Traverse forward.
 block_t *forward = explicitHead;
 int f_idx = 0;
 
 for (; f_idx < depth; f_idx++) {
   if (forward->body.next == NULL) {
     printf("%p (%d bytes) TAIL\n", forward, forward->block_size);
     f_len++;
     printf("  Forward traversal: %d elements.\n", f_len);
     break;
   }
 
   //printf("%p (%d bytes) -> ", forward, forward->block_size);
   forward = forward->body.next;
   f_len++;
 }
 
 if (f_idx == depth) {
   printf("\nWARNING: Reached forward depth limit.\n");
 }
 
 // Traverse backwards.
 block_t *backward = forward;
 int b_idx = 0;
 
 for (; b_idx < depth; b_idx++) {
   if (backward->body.prev == NULL) {
     printf("%p (%d bytes) HEAD\n", backward, backward->block_size);
     b_len++;
     printf("  Backward traversal: %d elements.\n", b_len);
     break;
   }
 
   //printf("%p (%d bytes) -> ", backward, backward->block_size);
   backward = backward->body.prev;
   b_len++;
 }
 
 if (b_idx == depth) {
   printf("\nWARNING: Reached backward depth limit.\n");
 }
 
 if (f_len != b_len) {
   printf("ERROR: length mismatch for forward and backward traversal.\n");
   exit(1);
 } else {
   printf(
       "Validated: equal lengths (%d) for forward and backward traversal.\n",
       f_len);
 }
}
*/
/*
static void printFreeListLength() {
   block_t *b;
   int listLength = 0;
  for (b = explicitHead; b != NULL; b = b->body.next) {
      listLength++;
  }
  printf("Size of free list: %d \n", listLength);
}
*/
 
static void printListSizes()
{
   block_t *b;
   int listLength = 0;
  for (b = head5; b != NULL; b = b->body.next) {
      listLength++;
  }
  printf("Size of head5 list: %d \n", listLength);
 
   listLength = 0;
  for (b = head6; b != NULL; b = b->body.next) {
      listLength++;
  }
  printf("Size of head6 list: %d \n", listLength);
 
      listLength = 0;
  for (b = head7; b != NULL; b = b->body.next) {
      listLength++;
  }
  printf("Size of head7 list: %d \n", listLength);
 
      listLength = 0;
  for (b = head8; b != NULL; b = b->body.next) {
      listLength++;
  }
  printf("Size of head8 list: %d \n", listLength);
 
      listLength = 0;
  for (b = head9; b != NULL; b = b->body.next) {
      listLength++;
  }
  printf("Size of head9 list: %d \n", listLength);
 
      listLength = 0;
  for (b = head10; b != NULL; b = b->body.next) {
      listLength++;
  }
  printf("Size of head10 list: %d \n", listLength);
 
      listLength = 0;
  for (b = head11; b != NULL; b = b->body.next) {
      listLength++;
  }
  printf("Size of head11 list: %d \n", listLength);
 
      listLength = 0;
  for (b = head12; b != NULL; b = b->body.next) {
      listLength++;
  }
  printf("Size of head12 list: %d \n", listLength);
 
      listLength = 0;
  for (b = head13; b != NULL; b = b->body.next) {
      listLength++;
  }
  printf("Size of head13 list: %d \n", listLength);
 
      listLength = 0;
  for (b = headinf; b != NULL; b = b->body.next) {
      listLength++;
  }
  printf("Size of headinf list: %d \n", listLength);
}
 
static void checkExpListsForAllocation()
{
      for(block_t *temp = head5; temp != NULL; temp = temp->body.next)
  {
      if(temp->allocated != FREE){
          printf("head5 List contains allocated block\n");
      }
  }
 
     for(block_t *temp = head6; temp != NULL; temp = temp->body.next)
  {
      if(temp->allocated != FREE){
          printf("head6 List contains allocated block\n");
      }
  }
 
     for(block_t *temp = head7; temp != NULL; temp = temp->body.next)
  {
      if(temp->allocated != FREE){
          printf("head7 List contains allocated block\n");
      }
  }
 
     for(block_t *temp = head8; temp != NULL; temp = temp->body.next)
  {
      if(temp->allocated != FREE){
          printf("head8 List contains allocated block\n");
      }
  }
 
     for(block_t *temp = head9; temp != NULL; temp = temp->body.next)
  {
      if(temp->allocated != FREE){
          printf("head9 List contains allocated block\n");
      }
  }
 
     for(block_t *temp = head10; temp != NULL; temp = temp->body.next)
  {
      if(temp->allocated != FREE){
          printf("head10 List contains allocated block\n");
      }
  }
 
     for(block_t *temp = head11; temp != NULL; temp = temp->body.next)
  {
      if(temp->allocated != FREE){
          printf("head11 List contains allocated block\n");
      }
  }
 
     for(block_t *temp = head12; temp != NULL; temp = temp->body.next)
  {
      if(temp->allocated != FREE){
          printf("head12 List contains allocated block\n");
      }
  }
 
     for(block_t *temp = head13; temp != NULL; temp = temp->body.next)
  {
      if(temp->allocated != FREE){
          printf("head13 List contains allocated block\n");
      }
  }
 
     for(block_t *temp = headinf; temp != NULL; temp = temp->body.next)
  {
      if(temp->allocated != FREE){
          printf("headinf List contains allocated block\n");
      }
  }
}
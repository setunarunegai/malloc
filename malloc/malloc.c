//
// >>>> malloc challenge! <<<<
//
// Your task is to improve utilization and speed of the following malloc
// implementation.
// Initial implementation is the same as the one implemented in simple_malloc.c.
// For the detailed explanation, please refer to simple_malloc.c.

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// Interfaces to get memory pages from OS
//

void *mmap_from_system(size_t size);
void munmap_to_system(void *ptr, size_t size);

//
// Struct definitions
//

typedef struct my_metadata_t {
  size_t size;
  struct my_metadata_t *next;
} my_metadata_t;

typedef struct my_heap_t {
  my_metadata_t *free_head;
  my_metadata_t dummy;
} my_heap_t;

//
// Static variables (DO NOT ADD ANOTHER STATIC VARIABLES!)
//
my_heap_t bin[4]; // i * 1000 <= size && size < (i + 1) * 1000
my_heap_t dummy;
//
// Helper functions (feel free to add/remove/edit!)
//

void my_remove_from_free_list(my_metadata_t *metadata, my_metadata_t *prev, size_t bin_size) {
  if (prev) {
    prev->next = metadata->next;
  } else {
    bin[bin_size].free_head = metadata->next;
  }
  metadata->next = NULL;
}


void marge_free_list(my_metadata_t *metadata, size_t bin_size) {
  my_metadata_t *left_metadata = bin[bin_size].free_head;
  while (left_metadata) {
    // ... | metadata    | free slot | metadata | free slot | ...
    //     ^                         ^
    //     left_metadata             metadata
    //                    <---------->           <---------->
    //                     left_size             metadata_size
    // ... | metadata    |            free slot             | ...
    //     ^                    
    //     metadata
    //                    <---------------------------------->
    //                                 left_size   
    if ((my_metadata_t *)((char *)left_metadata + (left_metadata->size) + 1) == metadata) {
      //printf("left");
      left_metadata->size += metadata->size + 1;
      my_remove_from_free_list(metadata, left_metadata, bin_size);
      metadata = left_metadata;
      break;
    }
    left_metadata = left_metadata->next;
  }
  my_metadata_t *right_metadata = bin[bin_size].free_head;
  while (right_metadata) {
    if (right_metadata == (my_metadata_t *)((char *)metadata + (metadata->size) + 1)) {
      //printf("right");
      metadata->size += right_metadata->size;
      my_remove_from_free_list(right_metadata, metadata, bin_size);
      break;
    }
    right_metadata = right_metadata->next;
  }
}


void my_add_to_free_list(my_metadata_t *metadata, size_t bin_size) {
  assert(!(metadata->next));
  metadata->next = bin[bin_size].free_head;
  bin[bin_size].free_head = metadata;
  marge_free_list(metadata, bin_size); //changing here makes error
}

//
// Interfaces of malloc (DO NOT RENAME FOLLOWING FUNCTIONS!)
//

// This is called at the beginning of each challenge.
void my_initialize() {
  for(size_t i=0;i<4;i++){
    bin[i].free_head = &dummy.dummy;
  }
  dummy.dummy.size = 0;
  dummy.dummy.next = NULL;
}

// my_malloc() is called every time an object is allocated.
// |size| is guaranteed to be a multiple of 8 bytes and meets 8 <= |size| <=
// 4000. You are not allowed to use any library functions other than
// mmap_from_system() / munmap_to_system().
void *my_malloc(size_t size) {
  size_t bin_size = size/1000;
  my_metadata_t *metadata = bin[bin_size].free_head;
  my_metadata_t *prev = NULL;
  // First-fit: Find the first free slot the object fits.
  // Updated this logic to Best-fit!
  int min_size = 4096;
  my_metadata_t *min_metadata = NULL;
  my_metadata_t *min_metadata_prev = NULL;
  while (metadata) {
    prev = metadata;
    metadata = metadata->next;
    if (metadata && metadata->size >= size && metadata->size < min_size){
      min_metadata = metadata;
      min_metadata_prev = prev;
      min_size = metadata->size;
    }
  }
  if (min_metadata) {
    metadata = min_metadata;
    prev = min_metadata_prev;
  }
  // now, metadata points to the best free slot
  // and prev is the previous entry.

  if (!metadata) {
    // There was no free slot available. We need to request a new memory region
    // from the system by calling mmap_from_system().
    //
    //     | metadata | free slot |
    //     ^
    //     metadata
    //     <---------------------->
    //            buffer_size
    size_t buffer_size = 4096;
    my_metadata_t *metadata = (my_metadata_t *)mmap_from_system(buffer_size);
    metadata->size = buffer_size - sizeof(my_metadata_t);
    metadata->next = NULL;
    // Add the memory region to the free list.
    my_add_to_free_list(metadata, bin_size);
    // Now, try my_malloc() again. This should succeed.
    return my_malloc(size);
  }

  // |ptr| is the beginning of the allocated object.
  //
  // ... | metadata | object | ...
  //     ^          ^
  //     metadata   ptr
  void *ptr = metadata + 1;
  size_t remaining_size = metadata->size - size;
  // Remove the free slot from the free list.
  my_remove_from_free_list(metadata, prev, bin_size);

  if (remaining_size > sizeof(my_metadata_t)) {
    // Shrink the metadata for the allocated object
    // to separate the rest of the region corresponding to remaining_size.
    // If the remaining_size is not large enough to make a new metadata,
    // this code path will not be taken and the region will be managed
    // as a part of the allocated object.
    metadata->size = size;
    // Create a new metadata for the remaining free slot.
    //
    // ... | metadata | object | metadata | free slot | ...
    //     ^          ^        ^
    //     metadata   ptr      new_metadata
    //                 <------><---------------------->
    //                   size       remaining size
    my_metadata_t *new_metadata = (my_metadata_t *)((char *)ptr + size);
    new_metadata->size = remaining_size - sizeof(my_metadata_t);
    new_metadata->next = NULL;
    // Add the remaining free slot to the free list.
    my_add_to_free_list(new_metadata, bin_size);
  }
  return ptr;
}

// This is called every time an object is freed.  You are not allowed to
// use any library functions other than mmap_from_system / munmap_to_system.
void my_free(void *ptr) {
  // Look up the metadata. The metadata is placed just prior to the object.
  //
  // ... | metadata | object | ...
  //     ^          ^
  //     metadata   ptr
  my_metadata_t *metadata = (my_metadata_t *)ptr - 1;
  size_t bin_size = (metadata->size) / 1000;
  // Add the free slot to the free list.
  my_add_to_free_list(metadata, bin_size);
}

// This is called at the end of each challenge.
void my_finalize() {
  // Nothing is here for now.
  // feel free to add something if you want!
}

void test() {
  // Implement here!
  assert(1 == 1); /* 1 is 1. That's always true! (You can remove this.) */
}

#include "my_malloc.h"
#include <unistd.h>
#include <string.h>
#include <pthread.h>

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock2 = PTHREAD_MUTEX_INITIALIZER;

__thread block * freeList_nolock = NULL; // TLS



// Helper function to merge adjacent free blocks
static void merge_free_blocks(block *b) {   
    //merge b with the next block
    if (b->next && (char *)b + BLOCK_SIZE + b->size == (char *)b->next) {
        b->size += BLOCK_SIZE + b->next->size;
        b->next = b->next->next;
        if (b->next) {
            b->next->prev = b;
        }
    }
    //merge b with the previous block
    if (b->prev && (char *)b->prev + BLOCK_SIZE + b->prev->size == (char *)b) {
        b->prev->size += BLOCK_SIZE + b->size;
        b->prev->next = b->next;
        if (b->next) {
            b->next->prev = b->prev;
        }
        b = b->prev;
    }
}


// Helper function to extend the heap
static block *extend_heap(size_t size) {
    pthread_mutex_lock(&lock2);
    block *b = (block*)sbrk(BLOCK_SIZE + size);
    pthread_mutex_unlock(&lock2);
    if (b == (void *)-1) {
        return NULL;
    }
    b->size = size;
    b->next = NULL;
    b->prev = NULL;
    return b;
}


// Helper function to split a block
static void split_block(block *b, size_t size) {
    block *new_block = (block *)((char *)b + BLOCK_SIZE + size);
    new_block->size = b->size - size - BLOCK_SIZE;
    if(b->prev){ 
        b->prev->next = new_block;
    }
    else{
        freeList = new_block;
    }
    new_block->prev = b->prev;
    if(b->next){
        b->next->prev = new_block;
    }
    new_block->next = b->next;
    //remove used b
    b->size = size;
    b->next = NULL;
    b->prev = NULL;
}



void bf_free(void *ptr) {
    // if (!ptr) {
    //     return;
    // }
    block *b = (block *)ptr - 1;
    
//version2:addBlock
    block *cur_block = freeList;
    block *prev_block = NULL;
    //while(cur_block && (char*)b > (char*)cur_block + cur_block->size + BLOCK_SIZE){
    while(cur_block && (char*)b > (char*)cur_block ){
        prev_block = cur_block;
        cur_block = cur_block->next;
    }
    // Insert b into the list
    if (prev_block) {
        prev_block->next = b;
    } else {
        freeList = b; // b becomes the new head if no lower address found
    }

    b->prev = prev_block;
    b->next = cur_block;
    if (cur_block) {
        cur_block->prev = b;
    }

    

    // Merge with adjacent free blocks
    merge_free_blocks(b);
}





block *find_best_fit(size_t size) {
    block *current = freeList;
    size_t remaining_space = INT_MAX;
    block *best_fit = NULL;

    while (current) {
        if (current->size >= size) {
            size_t current_remaining_space = current->size - size;

            // If a perfectly matching block is found (no remaining space), return immediately
            if (current_remaining_space == 0) {
                return current; 
            }

            // update remaining space, find the least remaining  
            if (current_remaining_space < remaining_space) {
                remaining_space = current_remaining_space;
                best_fit = current;
            }
        }
        current = current->next;
    }
    return best_fit;
}



void *ts_malloc_lock(size_t size) {
    pthread_mutex_lock(&lock);
   
    block *b = find_best_fit(size);
    if (!b) {
        
        b = extend_heap(size);
        if (!b) {
            pthread_mutex_unlock(&lock);
            return NULL;
        }
    } else {
        //large enough to split
        if (b->size > size + BLOCK_SIZE) {
            split_block(b, size);
            
        } 
        //remove block from list
        else {
            
            if (b->prev) {
                b->prev->next = b->next;
            } else {
                freeList = b->next;
            }
            if (b->next) {
                b->next->prev = b->prev;
            }
        }
    }
    pthread_mutex_unlock(&lock);
    return (void *)(b + 1);
}

void ts_free_lock(void * ptr) {
  // Acquire the lock
  pthread_mutex_lock(&lock);
  bf_free(ptr);
  // Release the lock
  pthread_mutex_unlock(&lock);
}


// Helper function to split a block
static void split_block_nolock(block *b, size_t size) {
    block *new_block = (block *)((char *)b + BLOCK_SIZE + size);
    new_block->size = b->size - size - BLOCK_SIZE;
    if(b->prev){ 
        b->prev->next = new_block;
    }
    else{
        freeList_nolock = new_block;
    }
    new_block->prev = b->prev;
    if(b->next){
        b->next->prev = new_block;
    }
    new_block->next = b->next;
    //remove used b
    b->size = size;
    b->next = NULL;
    b->prev = NULL;
}

block *find_best_fit_nolock(size_t size) {
    block *current = freeList_nolock;
    size_t remaining_space = INT_MAX;
    block *best_fit = NULL;

    while (current) {
        if (current->size >= size) {
            size_t current_remaining_space = current->size - size;

            // If a perfectly matching block is found (no remaining space), return immediately
            if (current_remaining_space == 0) {
                return current; 
            }

            // update remaining space, find the least remaining  
            if (current_remaining_space < remaining_space) {
                remaining_space = current_remaining_space;
                best_fit = current;
            }
        }
        current = current->next;
    }
    return best_fit;
}

void *ts_malloc_nolock(size_t size){
    // if (size <= 0) {
    //     return NULL;
    // }

    block *b = find_best_fit_nolock(size);
    if (!b) {
        b = extend_heap(size);
        if (!b) {
            return NULL;
        }
    } else {
        //large enough to split
        if (b->size > size + BLOCK_SIZE) {
            split_block_nolock(b, size);
            
        } 
        //remove block from list
        else {
            
            if (b->prev) {
                b->prev->next = b->next;
            } else {
                freeList_nolock = b->next;
            }
            if (b->next) {
                b->next->prev = b->prev;
            }
        }
    }

    return (void *)(b + 1);
}

void ts_free_nolock(void *ptr){
    // if (!ptr) {
    //     return;
    // }
    block *b = (block *)ptr - 1;
    
//version2:addBlock
    block *cur_block = freeList_nolock;
    block *prev_block = NULL;
    //while(cur_block && (char*)b > (char*)cur_block + cur_block->size + BLOCK_SIZE){
    while(cur_block && (char*)b > (char*)cur_block ){
        prev_block = cur_block;
        cur_block = cur_block->next;
    }
    // Insert b into the list
    if (prev_block) {
        prev_block->next = b;
    } else {
        freeList_nolock = b; // b becomes the new head if no lower address found
    }

    b->prev = prev_block;
    b->next = cur_block;
    if (cur_block) {
        cur_block->prev = b;
    }


    // Merge with adjacent free blocks
    merge_free_blocks(b);
}

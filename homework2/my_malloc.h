#ifndef MY_MALLOC_H
#define MY_MALLOC_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>


typedef struct block_t {
    size_t size;
    struct block_t *prev;
    struct block_t *next;
} block;

#define BLOCK_SIZE sizeof(block)
static block *freeList = NULL; //list of free memory


void *bf_malloc(size_t size);
void bf_free(void *ptr);

void *ts_malloc_lock(size_t size); 
void ts_free_lock(void *ptr);

void *ts_malloc_nolock(size_t size); 
void ts_free_nolock(void *ptr);

#endif

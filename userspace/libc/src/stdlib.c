#include "stdlib.h"
#include "pthread.h"
#include "nonstd.h"

metadata* head = NULL;
pthread_spinlock_t memory_lock;


metadata* add_block(size_t size_sbrk)
{
  //add new block with sbreak size
  void* new_mem_void = NULL;
  if ((long) size_sbrk < 0)
    return NULL;
  new_mem_void = sbrk(size_sbrk);
  if(new_mem_void == (void*)-1)
  {
      //new_mem_void = sbrk(0);
      return NULL;
  }
  char* new_mem = (char*) new_mem_void;
  metadata* new_block = (metadata*) new_mem;
  new_block->user_start = new_mem + sizeof(metadata);
  new_block->free = 0;
  new_block->magic_number = MAGIC_NUMBER;
  new_block->size = size_sbrk - sizeof(metadata);
  new_block->next = NULL;
  return new_block;  
}

metadata* split(metadata* free_block, size_t size){
  //split block - dont split for exact fit!
  if (free_block == NULL)
    return NULL;
  char* new_block_start = (char*) free_block->user_start + size;
  metadata* new_block = (metadata*)new_block_start;
  new_block->user_start =  (void*)(new_block_start + sizeof(metadata));
  new_block->free = 1;
  new_block->magic_number = MAGIC_NUMBER;
  new_block->size = free_block->size - size - sizeof(metadata);
  new_block->next = free_block->next;
  free_block->next = new_block;
  free_block->free = 0;
  free_block->size = size;
  return free_block;
}

void reduce_break(metadata* last_used, size_t total_size){
  if (last_used == NULL)
  {
    sbrk(-total_size);
    return;
  }
  metadata* leftover_block;
  char* leftover_block_start;
  size_t trash_bits = (total_size % PAGE_SIZE_US < sizeof(metadata) + 1) ? total_size % PAGE_SIZE_US : 0;
  sbrk(-(((total_size/PAGE_SIZE_US) * PAGE_SIZE_US) + trash_bits));
  if(total_size % PAGE_SIZE_US > sizeof(metadata) + 1)
  {
    leftover_block_start = (char*)last_used->user_start + last_used->size;
    leftover_block = (metadata*) leftover_block_start;
    leftover_block->user_start = (void*)(leftover_block_start + sizeof(metadata));
    leftover_block->size = (total_size % PAGE_SIZE_US) - sizeof(metadata);
    leftover_block->free = 1;
    leftover_block->next = NULL;
    leftover_block->magic_number = MAGIC_NUMBER;
    last_used->next = leftover_block;
    return;
  }
  last_used->next = NULL;
  return;
}

void merge(metadata* first_block, metadata* second_block){
  first_block->next = second_block->next;
  first_block->size += second_block->size + sizeof(metadata);
}

void *malloc(size_t size)
{
  if (memory_lock.initialized_ == 0)
  {
    pthread_spin_init(&memory_lock, 0);
  }
  
  metadata* this_block = NULL;
  int found_block = 0;
  if (size == 0)
    return NULL;

  pthread_spin_lock(&memory_lock);
  metadata* iter = head;
  if (head == NULL)
  {
    if((size + sizeof(metadata)) % PAGE_SIZE_US == 0)
      this_block = add_block(size + sizeof(metadata));
    else
    {
      this_block = split(add_block(((size + 2* sizeof(metadata)) / PAGE_SIZE_US) * PAGE_SIZE_US + 1 * PAGE_SIZE_US), size);
      if(this_block == NULL)
      {
        this_block = add_block(size + sizeof(metadata));
        if(this_block == NULL)
        {
          pthread_spin_unlock(&memory_lock);
          return NULL;
        }
      }
    }
    head = this_block;
  }
  else 
  {
    while (iter->next != NULL)
    {
      if(iter->magic_number != MAGIC_NUMBER)
        exit(-1);
      if(!found_block && iter->free && iter->size == size)
      {
        iter->free = 0;
        this_block = iter;
        found_block = 1;
      }
      if(!found_block && iter->free && iter->size > size + sizeof(metadata))
      {
        this_block = split(iter, size);
        found_block = 1;
      }
      if (iter->free && iter->next != NULL && iter->next->free)
        merge(iter, iter->next);
      if(iter->next == NULL)
        break;
      iter = iter->next;
    }
    if(iter->magic_number != MAGIC_NUMBER)
        exit(-1);
    if(!found_block && iter->free && iter->size == size)
    {
      iter->free = 0;
      this_block = iter;
      found_block = 1;
    }
    if(!found_block && iter->free && iter->size > size + sizeof(metadata))
    {
      this_block = split(iter, size);
      found_block = 1;
    }
    if(!found_block)
    { 
      if((size + sizeof(metadata)) % PAGE_SIZE_US == 0)
        this_block = add_block(size + sizeof(metadata));
      else
      { 
        this_block = split(add_block(((size + 2 * sizeof(metadata)) / PAGE_SIZE_US) * PAGE_SIZE_US + 1 * PAGE_SIZE_US),size);
        if(this_block == NULL)
        {
          this_block = add_block(size + sizeof(metadata));
          if(this_block == NULL)
          {
            pthread_spin_unlock(&memory_lock);
            return NULL;
          }
        }
      }
      iter->next = this_block;
    }
  }
  //void* addr = sbrk(0);fs
  pthread_spin_unlock(&memory_lock);
  return this_block->user_start;
}

void free(void *ptr)
{
  //void* curbreak = (void*) sbrk(0);
  int found_block = 0;
  size_t free_mem_count = 0;
  if(ptr == NULL)
    return;
  
  pthread_spin_lock(&memory_lock);
  metadata* iter = head;
  metadata* last_address_blocked = head;
  if (head == NULL)
    exit(-1);
//  else 
//  {
    do {
      if(iter->user_start == ptr)
      {
        if(iter->free)
          exit(-1);
        iter->free = 1; 
        found_block = 1;
      }
      if(iter->magic_number != MAGIC_NUMBER)
        exit(-1);
      if (iter->free && iter->next != NULL && iter->next->free)
        merge(iter, iter->next);
      else if (iter->free && iter->next != NULL && iter->next->user_start == ptr)
      {
        if(iter->next->free)
          exit(-1);
        merge(iter, iter->next);
        found_block = 1;
      }
      if(iter->free)
        free_mem_count += iter->size + sizeof(metadata);
      else
      {
        last_address_blocked = iter;
        free_mem_count = 0;
      }
      iter = iter->next;
    } while (iter != NULL);
    if(!found_block)
      exit(-1);
    if(last_address_blocked == head && head->free)
    {
      last_address_blocked = NULL;
      head = NULL;
    }
    if(free_mem_count/PAGE_SIZE_US > 0 || head == NULL)
    {
      reduce_break(last_address_blocked, free_mem_count);
    }
//  }
  pthread_spin_unlock(&memory_lock);
  // curbreak = (void*) sbrk(0);
}

int atexit(void (*function)(void))
{
  return -1;
}

void *calloc(size_t nmemb, size_t size)
{
  char* user_return;
  if (nmemb == 0 || size == 0)
    return NULL;
  user_return = (char*) malloc(nmemb * size);
  for (size_t i = 0; i < nmemb * size; i++)
  {
    user_return[i] = 0;
  }
  return (void*) user_return;
}

void *realloc(void *ptr, size_t size)
{
  return 0;
}


// TODO Student End



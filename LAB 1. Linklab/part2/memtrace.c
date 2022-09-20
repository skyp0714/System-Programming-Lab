//------------------------------------------------------------------------------
//
// memtrace
//
// trace calls to the dynamic memory manager
//
#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <memlog.h>
#include <memlist.h>

//
// function pointers to stdlib's memory management functions
//
static void *(*mallocp)(size_t size) = NULL;
static void (*freep)(void *ptr) = NULL;
static void *(*callocp)(size_t nmemb, size_t size);
static void *(*reallocp)(void *ptr, size_t size);

//
// statistics & other global variables
//
static unsigned long n_malloc  = 0;
static unsigned long n_calloc  = 0;
static unsigned long n_realloc = 0;
static unsigned long n_allocb  = 0;
static unsigned long n_freeb   = 0;
static item *list = NULL;

//
// init - this function is called once when the shared library is loaded
//
__attribute__((constructor))
void init(void)
{
  char *error;

  LOG_START();

  // initialize a new list to keep track of all memory (de-)allocations
  // (not needed for part 1)
  list = new_list();

  // ...
}

//
// fini - this function is called once when the shared library is unloaded
//
__attribute__((destructor))
void fini(void)
{
  // ...
  unsigned long alloc_total = n_malloc + n_calloc + n_realloc;
  unsigned long alloc_avg = alloc_total/n_allocb;
  LOG_STATISTICS(alloc_total, alloc_avg, n_freeb);
  item *next;
  int first = 1;
  while (list) {
    next = list->next;
    if(list->cnt){
      if(first) {
        LOG_NONFREED_START();
        first = 0;
      }
      LOG_BLOCK(list->ptr, list->size, list->cnt);
    }
    list = next;
  }

  LOG_STOP();

  // free list (not needed for part 1)
  free_list(list);
}

// ...
void *malloc(size_t size){
  char *error;
  
  if(!mallocp){
    mallocp = dlsym(RTLD_NEXT, "malloc");
    if((error = dlerror()) != NULL){
      fputs(error, stderr);
      exit(1);
    }
  }
  void *ptr = mallocp(size);
  alloc(list, ptr, size);
  n_malloc += size;
  n_allocb ++;
  LOG_MALLOC(size, ptr);
  return ptr;
}
void free(void *ptr){
  char *error;
  if(!ptr) return;
  if(!freep){
    freep = dlsym(RTLD_NEXT, "free");
    if((error = dlerror()) != NULL){
      fputs(error, stderr);
      exit(1);
    }
  }
  item *node = dealloc(list, ptr);
  n_freeb += node->size;
  freep(ptr);
  LOG_FREE(ptr);
}
void *calloc(size_t nmemb, size_t size){
  char *error;

  if(!callocp){
    callocp = dlsym(RTLD_NEXT, "calloc");
    if((error = dlerror()) != NULL){
      fputs(error, stderr);
      exit(1);
    }
  }
  void *ptr = callocp(nmemb, size);
  alloc(list, ptr, nmemb*size);
  n_calloc += nmemb*size;
  n_allocb ++;
  LOG_CALLOC(nmemb, size, ptr);
  return ptr;
}
void *realloc(void *ptr, size_t size){
  char *error;
  if(!reallocp){
    reallocp = dlsym(RTLD_NEXT, "realloc");
    if((error = dlerror()) != NULL){
      fputs(error, stderr);
      exit(1);
    }
  }
  void *nptr = reallocp(ptr, size);
  item *node = dealloc(list, ptr);
  n_freeb += node->size;
  alloc(list, nptr, size);
  n_realloc += size;
  n_allocb ++;
  LOG_REALLOC(ptr, size, nptr);
  return nptr;
}
#include "kernel.h"

typedef struct arraylist {
  void** buffer;            // pointer to allocated memory
  unsigned int buffer_size; // the max number of elements the buffer can hold
  unsigned int length;      // the number of values stored in the list
} arraylist;

void arraylist_add(arraylist* a, void* x);

/*
 * Create a new arraylist
 */
arraylist* arraylist_new();

/*
 * Free any memory used by that arraylist.
 */
void arraylist_free(arraylist* a);

void arraylist_remove(arraylist* a, unsigned int index);

void* arraylist_get(arraylist* a, unsigned int index);

////////////////////////////////// Hashtable ///////////////////////////////////

unsigned int hash(unsigned int a);

typedef struct hashtable {
  void* buckets;            // pointer to allocated memory
  unsigned int n;           // number of buckets
  unsigned int length;      // the number of integers stored in the list
  unsigned int num_inserts; // the number of insert operations performed so far
  int* lock;
} hashtable;

typedef struct pair {
  signed int key;
  signed int value;
} pair;

void hashtable_create(struct hashtable *self);

void hashtable_put(struct hashtable *self, int key, int value);

int hashtable_get(struct hashtable *self, int key);

void hashtable_remove(struct hashtable *self, int key);

void hashtable_stats(struct hashtable *self, char* name);

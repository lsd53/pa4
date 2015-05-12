#include "hashtable.h"

#define STARTING_SIZE 32

/*
 * Append the value x to the end of the arraylist. If necessary, double the
 * capacity of the arraylist.
 */
void arraylist_add(arraylist* a, void* x) {
  if (a->buffer_size == a->length) {
    // Need to realloc
    a->buffer_size *= 2;

    // Malloc_safe new space, copy elements, free old space
    void** old_space = a->buffer;
    a->buffer = (void**) malloc_safe(sizeof(void*) * a->buffer_size);
    memcpy(a->buffer, old_space, sizeof(void*) * a->length);

    free(old_space);
  }

  a->buffer[a->length] = x;
  a->length++;
}

/*
 * Create a new arraylist
 */
arraylist* arraylist_new() {
  arraylist* a = (arraylist*)malloc_safe(sizeof(arraylist));
  a->buffer = malloc_safe(STARTING_SIZE * sizeof(void*));
  a->buffer_size = STARTING_SIZE;
  a->length = 0;

  return a;
}

/*
 * Free any memory used by that arraylist.
 */
void arraylist_free(arraylist* a) {
  free(a->buffer);
  free(a);
}

void arraylist_remove(arraylist* a, unsigned int index) {
  unsigned int i;
  for (i = index; i < a->length - 1; i++)
    a->buffer[i] = a->buffer[i + 1];

  --a->length;
}

void* arraylist_get(arraylist* a, unsigned int index) {
  return a->buffer[index];
}

////////////////////////////////// Hashtable ///////////////////////////////////

unsigned int hash(unsigned int a) {
  a = (a ^ 61) ^ (a >> 16);
  a = a + (a << 3);
  a = a ^ (a >> 4);
  a = a * 0x27d4eb2d;
  a = a ^ (a >> 15);
  return a;
}

void hashtable_create(struct hashtable *self) {
  // Add 2 buckets initially
  self->buckets = arraylist_new();
  self->n           = STARTING_SIZE;
  self->length      = 0;
  self->num_inserts = 0;
  self->lock        = (int*) malloc_safe(sizeof(int));
  *(self->lock)     = 0;

  unsigned int i;
  for (i = 0; i < self->n; i++) {
    arraylist_add(self->buckets, arraylist_new());
  }
}

void hashtable_put(struct hashtable *self, int key, int value) {
  pair* element  = (pair*)malloc_safe(sizeof(pair));
  element->key   = key;
  element->value = value;

  int h = hash(key);

  mutex_lock(self->lock);

  self->length++;
  self->num_inserts++;

  if (((double)self->length / (double)self->n) > 0.75) {
    // Have to realloc
    self->n *= 2;
    arraylist* new_buckets = arraylist_new();

    // Create new arraylist for each bucket
    unsigned int i;
    for (i = 0; i < self->n; i++) {
      arraylist_add(new_buckets, arraylist_new());
    }

    // Rehash all elements
    unsigned int k;
    for (k = 0; k < (self->n / 2); k++) {
      arraylist* current = arraylist_get(self->buckets, k);

      int j;
      for (j = 0; j < current->length; j++) {
        // Rehash key
        pair* old = arraylist_get(current, j);
        int new_location = hash(old->key) % self->n;

        self->num_inserts++;

        // Add pair to new bucket
        arraylist* new_bucket = arraylist_get(new_buckets, new_location);
        arraylist_add(new_bucket, old);

      }

      // Free each bucket
      arraylist_free(current);
    }

    // Free the buckets arraylist
    arraylist_free(self->buckets);

    self->buckets = new_buckets;
  }

  int which_bucket = h % self->n;
  arraylist* bucket = arraylist_get(self->buckets, which_bucket);

  unsigned int i;
  for (i = 0; i < bucket->length; i++) {
    pair* each = arraylist_get(bucket, i);

    // Return if keys match
    if (each->key == key) {
      self->length--;
      arraylist_remove(bucket, i);
      break;
    }
  }

  // Add to bucket
  arraylist_add(bucket, element);

  mutex_unlock(self->lock);
}

int hashtable_get(struct hashtable *self, int key) {
  int h = hash(key);

  mutex_lock(self->lock);

  int which_bucket = h % self->n;

  arraylist* bucket = arraylist_get(self->buckets, which_bucket);

  int i;
  for (i = 0; i < bucket->length; i++) {
    pair* element = arraylist_get(bucket, i);

    // Return if keys match
    if (element->key == key) {
      mutex_unlock(self->lock);
      return element->value;
    }
  }

  // Error if nothing returned yet
  mutex_unlock(self->lock);
  return -1;
}

void hashtable_remove(struct hashtable *self, int key) {
  int h = hash(key);

  mutex_lock(self->lock);

  int which_bucket = h % self->n;

  arraylist* bucket = arraylist_get(self->buckets, which_bucket);

  int i;
  for (i = 0; i < bucket->length; i++) {
    pair* element = arraylist_get(bucket, i);

    // Remove if keys match
    if (element->key == key) {
      arraylist_remove(bucket, i);
      self->length--;
      break;
    }
  }

  mutex_unlock(self->lock);
}

void hashtable_stats(struct hashtable *self, char* name) {
  mutex_lock(self->lock);

  printf("\toccurrences\t%s\n", name);
  unsigned int total_occ = 0;
  unsigned int records   = 0;

  unsigned int b;
  unsigned int e;
  for (b = 0; b < self->n; b++) {
    arraylist* bucket = arraylist_get(self->buckets, b);
    for (e = 0; e < bucket->length; e++) {
      pair* tuple = arraylist_get(bucket, e);
      printf("\t%d\t\t0x%x\n", tuple->value, tuple->key);

      total_occ += tuple->value;
      records++;
    }
  }
  printf("total count:\t%d\n", total_occ);
  printf("entries:\t%d\n", records);
  puts("");

  mutex_unlock(self->lock);
}

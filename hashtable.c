#include "hashtable.h"

/*
 * Append the value x to the end of the arraylist. If necessary, double the
 * capacity of the arraylist.
 */
void arraylist_add(arraylist* a, void* x) {
  if (a->buffer_size == a->length) {
    // Need to realloc
    a->buffer_size *= 2;

    // Malloc new space, copy elements, free old space
    void** old_space = a->buffer;
    a->buffer = (void**) malloc(sizeof(void*) * a->buffer_size);

    int i;
    for (i = 0; i < a->length; i++) {
      a->buffer[i] = old_space[i];
    }

    free(old_space);
  }

  a->buffer[a->length] = x;
  a->length++;
}

/*
 * Create a new arraylist
 */
arraylist* arraylist_new() {
  arraylist* a = (arraylist*)malloc(sizeof(arraylist));
  a->buffer = (void**)malloc(2 * sizeof(void*));
  a->buffer_size = 2;
  a->length = 0;

  return a;
}

/*
 * Free any memory used by that arraylist.
 */
void arraylist_free(arraylist* a) {
  // Hint: How many times is malloc called when creating a new arraylist?
  free(a->buffer);
  free(a);
}

void arraylist_remove(arraylist* a, unsigned int index) {
  for (unsigned int i = index; i < a->length - 1; i++)
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
  arraylist_add(self->buckets, arraylist_new());
  arraylist_add(self->buckets, arraylist_new());

  self->n           = 2;
  self->length      = 0;
  self->num_inserts = 0;
}

void hashtable_put(struct hashtable *self, int key, int value) {
  pair* element  = (pair*)malloc(sizeof(pair));
  element->key   = key;
  element->value = value;

  self->num_inserts++;
  self->length++;

  int h = hash(key);

  if (((double)self->length / (double)self->n) > 0.75) {
    // Have to realloc
    self->n *= 2;
    arraylist* new_buckets = arraylist_new();

    // Create new arraylist for each bucket
    int i;
    for (i = 0; i < self->n; i++) {
      arraylist_add(new_buckets, arraylist_new());
    }

    // Rehash all elements
    for (i = 0; i < (self->n / 2); i++) {
      arraylist* current = arraylist_get(self->buckets, i);

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

  // Check if key already exists
  int overwrite = 0;
  int i;

  int which_bucket = h % self->n;
  arraylist* bucket = arraylist_get(self->buckets, which_bucket);

  for (i = 0; i < bucket->length; i++) {
    pair* each = arraylist_get(bucket, i);

    // Return if keys match
    if (each->key == key) {
      overwrite = 1;
      self->length--;
      each->value = value;
    }
  }

  // Add to bucket if nothing overwritten
  if (!overwrite) arraylist_add(bucket, element);
}

int hashtable_get(struct hashtable *self, int key) {
  int h = hash(key);
  int which_bucket = h % self->n;

  arraylist* bucket = arraylist_get(self->buckets, which_bucket);

  int i;
  for (i = 0; i < bucket->length; i++) {
    pair* element = arraylist_get(bucket, i);

    // Return if keys match
    if (element->key == key) {
      return element->value;
    }
  }

  // Error if nothing returned yet
  return -1;
}

void hashtable_remove(struct hashtable *self, int key) {
  int h = hash(key);
  int which_bucket = h % self->n;
  int removed = 0;

  arraylist* bucket = arraylist_get(self->buckets, which_bucket);

  int i;
  for (i = 0; i < bucket->length; i++) {
    pair* element = arraylist_get(bucket, i);

    // Remove if keys match
    if (element->key == key) {
      arraylist_remove(bucket, i);
      self->length--;
      removed = 1;
      break;
    }
  }
}

void hashtable_stats(struct hashtable *self) {
  printf("length = %d, N = %d, puts = %d\n", self->length, self->n, self->num_inserts);
}

#include <stddef.h>
#include "kernel.h"

// Queue data structure implemented using LinkedList
// Implemented from scratch

typedef struct Node {
  void* data;
  int length;
  struct Node* next;
} Node;

typedef struct Queue {
  Node* head;
  Node* tail;
} Queue;

Queue* queue_new();

void queue_delete(Queue* q);

void queue_push(Queue* q, void* new_data,int length);

void* queue_pop(Queue* q);

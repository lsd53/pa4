#include <stdlib.h>

// Queue data structure implemented using LinkedList
// Implemented from scratch

typedef struct Node {
  void* data;
  struct Node* next;
} Node;

typedef struct Queue {
  Node* head;
  Node* tail;
} Queue;

/**
 * Returns a new, empty queue
 */
Queue* queue_new() {
  // Alloc a new queue
  Queue* new_queue = (Queue*)malloc(sizeof(Queue));
  new_queue->head = NULL;
  new_queue->tail = NULL;
  return new_queue;
}

/**
 * Loops over all elements and frees them, effectively deleting the queue
 */
void queue_delete(Queue* q) {
  while (q->head != NULL) {
    // Get head
    Node* first = q->head;

    // Update head in queue then free
    q->head = first->next;
    free(first);
  }
}

/**
 * Pushes a new value onto the queue
 */
void queue_push(Queue* q, void* new_data) {
  // Create new Node
  Node* new_node = (Node*)malloc(sizeof(Node));
  new_node->data = new_data;
  new_node->next = NULL;

  if (q->head == NULL) {
    // Set head and tail if queue empty
    q->head = new_node;
    q->tail = new_node;

  } else {
    // Update pointer in queue
    q->tail->next = new_node;
    q->tail = new_node;

  }
}

/**
 * Pops a value off the queue
 */
void* queue_pop(Queue* q) {
  // Return NULL if empty queue
  if (q->head == NULL) {
    return NULL;
  }

  // Get head
  Node* first = q->head;

  // Update head in queue
  q->head = first->next;

  // Get data and free Node
  void* data = first->data;
  free(first);
  return data;
}

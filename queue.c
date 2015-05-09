// TODO: Inserting first element, removing last
// Queue data structure implemented using LinkedList

typedef struct ListNode {
  void* data;
  ListNode* next;
} ListNode;

typedef struct Queue {
  void* head;
  void* tail;
} Queue;

Queue* queue_new() {
  // Alloc a new queue
  Queue&* new_queue = (Queue*)malloc(sizeof(Queue));
  return new_queue;
}

void queue_push(Queue* q, void* new_data) {
  // Push the new data onto the queue as a ListNode
  // Update rear next pointer

  // Create new ListNode
  ListNode* new_node = (ListNode*)malloc(sizeof(ListNode));
  new_node->data = new_data;

  // Update pointer in queue
  q->tail->next = new_node;
  q->tail = new_node
}

void* queue_pop(Queue* q) {
  // Remove head of queue set head to head->next
  // Get head
  ListNode* first = q->head;

  // Update head in queue
  q->head = first->next;

  // Get data and free ListNode
  void* data = first->data;
  free(first);
  return data;
}

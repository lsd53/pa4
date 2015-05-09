#include <stdio.h>
#include "queue.c"

int main() {
  Queue* my_queue = queue_new();

  // Push 2 values
  int first  = 34;
  int second = 10;

  queue_push(my_queue, &first);
  queue_push(my_queue, &second);
  printf("Pushed: %d, %d\n", first, second);

  // Pop 1
  int* reti = (int*)queue_pop(my_queue);
  printf("Popped: %d\n", *reti);
  

  // Push 1 then pop 2
  int third = 65;
  queue_push(my_queue, &third);
  printf("Pushed: %d\n", third);

  int* retf = (int*)queue_pop(my_queue);
  int* rets = (int*)queue_pop(my_queue);
  printf("Popped: %d, %d\n", *retf, *rets);

  int* empty = (int*)queue_pop(my_queue);
  if (empty == NULL) {
    printf("Nothing left\n");
  }

  queue_delete(my_queue);
  return 0;
}

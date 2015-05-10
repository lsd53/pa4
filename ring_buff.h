#include "kernel.h"


typedef struct ring_slot{
  void* dma_base;
  unsigned int dma_len;

}ring_slot;


typedef struct ring_buff {
  ring_slot* ring_base;
  unsigned int ring_capacity;
  unsigned int ring_head;
  unsigned int ring_tail;
}ring_buff;

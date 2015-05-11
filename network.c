#include "kernel.h"
#include "ring_buff.h"


//define the size of the ring and each buffer inside it
#define RING_SIZE 16
#define BUFFER_SIZE 4096 // ask about buffer size



// pointer to memory-mapped I/0 region for network driver
volatile struct dev_net *net_driver;

volatile struct ring_buff *ring_buffer;

void network_init(int cores_reading){
  /* Find virtual address of I/0 region for network driver */
  for (int i=0; i < 16; i++){
    if (bootparams->devtable[i].type == DEV_TYPE_NETWORK){
      puts("Detected network device...");
      // find virtual address of this region
      net_driver = physical_to_virtual(bootparams->devtable[i].start);
      break;
    }
  }
  /* allocate room for ring buffer and set rx_base to start of the array */
  struct dma_ring_slot* ring=(struct dma_ring_slot*) malloc(sizeof(struct dma_ring_slot) * RING_SIZE);
  net_driver->rx_base =  virtual_to_physical((void *) ring); //ASK ABOUT THIS PART
  net_driver->rx_capacity = RING_SIZE;

  /*for(int i = 0 ; i < cores_reading; i++){
    ring_buffer[i].ring_capacity = 10;
    ring_buffer[i].ring_head = 0;
    ring_buffer[i].ring_tail = 0;
    ring_buffer[i].ring_base =(struct ring_slot*) malloc(sizeof(struct ring_slot)*10);
    // allocate room for each buffer in each ring
    for(int j = 0; i < 10 ; i++){
      ring_buffer[i].ring_base[j].dma_base = malloc(BUFFER_SIZE);
      ring_buffer[i].ring_base[j].dma_len = BUFFER_SIZE;
    }
  }*/
  // allocate room for each buffer in the ring and set dma_base and dma_len to appropriate values
  for (int i = 0; i < RING_SIZE; i++) {
    void* space = alloc_pages(1);
    ring[i].dma_base = virtual_to_physical((void *) space);
    ring[i].dma_len = BUFFER_SIZE;
  }


  puts("...network driver is ready.");
}

void network_start_receive(){
  // write command into cmd register in order to turn on network card
  net_driver->cmd = NET_SET_POWER;
  net_driver->data = 1;
  puts("Network card on");

  // allow network card to recieve packets
  net_driver->cmd = NET_SET_RECEIVE;
  net_driver->data = 1;
  puts("Ready to start receiving packets");
}

void network_set_interrupts(int opt){

}


void network_poll(struct ring_buff** ring_buffers, int cores_reading) {
  struct dma_ring_slot* ring = (struct dma_ring_slot*) physical_to_virtual(net_driver->rx_base);
  while (1) {
    // if ((net_driver->rx_head % net_driver->rx_capacity) == (net_driver->rx_tail % net_driver->rx_capacity)) {
    if (net_driver->rx_head != net_driver->rx_tail) {
      int index = net_driver->rx_tail % cores_reading;
      struct ring_buff* ring_buffer = ring_buffers[index];

      // access the buffer at the ring slot and retrieve the packet
      void* packet = physical_to_virtual(ring[net_driver->rx_tail % RING_SIZE].dma_base);
      int ring_index = ring_buffer->ring_head % ring_buffer->ring_capacity;

      // Do nothing if ring buffer full (drop packet)
      int rb_head = ring_buffer->ring_head;
      int rb_tail = ring_buffer->ring_tail;

      if (rb_head != rb_tail && (rb_head % ring_buffer->ring_capacity == rb_tail % ring_buffer->ring_capacity)) {
        // Just free the page (drop the packet) if the buffer is full
        free_pages(packet, 1);
      } else {
        ring_buffer->ring_base[ring_index].dma_base = packet;
        ring_buffer->ring_head++;
      }

      // free(packet);
      // reallocate memory for ring buffer when done with packet, reset length and update the tail
      void* space = alloc_pages_safe(1);
      ring[net_driver->rx_tail % RING_SIZE].dma_base = virtual_to_physical(space);
      ring[net_driver->rx_tail % RING_SIZE].dma_len = BUFFER_SIZE;
      net_driver->rx_tail++;

      // reallocate memory for ring buffer when done with packet, reset length and update the tail
      // ring[net_driver->rx_tail % RING_SIZE].dma_len = BUFFER_SIZE;
      // net_driver->rx_tail++;
    }
  }
}


void network_trap(){

}

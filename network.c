#include "kernel.h"
#include "ring_buff.h"


//define the size of the ring and each buffer inside it
#define RING_SIZE 16
#define BUFFER_SIZE 4096 // ask about buffer size



// pointer to memory-mapped I/0 region for network driver
volatile struct dev_net *net_driver;

unsigned int received_packets = 0;
unsigned int received_bytes   = 0;
unsigned int avg_bytes        = 0;

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

  // allocate room for ring buffer and set rx_base to start of the array 
  struct dma_ring_slot* ring=(struct dma_ring_slot*) malloc(sizeof(struct dma_ring_slot) * RING_SIZE);
  net_driver->rx_base =  virtual_to_physical((void *) ring); //ASK ABOUT THIS PART
  net_driver->rx_capacity = RING_SIZE;
  net_driver->rx_head = 0;
  net_driver->rx_tail = 0;

  // allocate room for each buffer in the ring and set dma_base and dma_len to appropriate values
  for (int i = 0; i < RING_SIZE; i++) {
    void* space = alloc_pages(1);
    ring[i].dma_base = virtual_to_physical((void *) space);
    ring[i].dma_len = BUFFER_SIZE;
  }


  puts("...network driver is ready.");
}

void network_start_receive() {
  // write command into cmd register in order to turn on network card
  net_driver->cmd = NET_SET_POWER;
  net_driver->data = 1;
  puts("Network card on");

  // allow network card to recieve packets
  net_driver->cmd = NET_SET_RECEIVE;
  net_driver->data = 1;
  puts("Ready to start receiving packets");
}

void network_set_interrupts(int opt) {}

void network_poll(struct ring_buff** queues, int cores_reading, struct page_buff* unused_pages) {
  int which_core = 0;
  struct dma_ring_slot* net_driver_slot = (struct dma_ring_slot*) physical_to_virtual(net_driver->rx_base);
  while (1) {
    if (net_driver->rx_head != net_driver->rx_tail) {

      struct ring_buff* q = queues[which_core % cores_reading];
      which_core++;

      // Do nothing if ring buffer full (drop packet)
      int q_head = q->ring_head;
      int q_tail = q->ring_tail;

      // Ignore the packet if this buffer is full
      if (q_head == q_tail || (q_head % q->ring_capacity != q_tail % q->ring_capacity)) {

        // Get indices for use later
        int q_push_idx = q->ring_head % q->ring_capacity;
        int which_packet = net_driver->rx_tail % RING_SIZE;

        // access the buffer at the ring slot and retrieve the packet
        void* packet = physical_to_virtual(net_driver_slot[which_packet].dma_base);

        // For global stats
        received_packets++;
        received_bytes += net_driver_slot[which_packet].dma_len;
        avg_bytes = ((avg_bytes * (received_packets - 1)) + net_driver_slot[which_packet].dma_len) / received_packets;

        // Add pointer to packet to specific core ring buffer
        q->ring_base[q_push_idx].dma_base = packet;
        q->ring_base[q_push_idx].dma_len = net_driver_slot[which_packet].dma_len;
        q->ring_head++;

        // reallocate memory for ring buffer when done with packet, reset length and update the tail
        // get new page for network ring buffer when done with packet
        int up_index = unused_pages->ring_tail % unused_pages->ring_capacity;
        void* new_space = unused_pages->pages[up_index];
        unused_pages->ring_tail++;

        net_driver_slot[which_packet].dma_base = virtual_to_physical(new_space);
        net_driver_slot[which_packet].dma_len = BUFFER_SIZE;
        net_driver->rx_tail++;
      }
    }
  }
}

// Getter methods for global stats
unsigned int get_dropped_packets() {
  net_driver->cmd = NET_GET_DROPCOUNT;
  return net_driver->data;
}

unsigned int get_received_packets() {
  return received_packets;
}

unsigned int get_received_bytes() {
  return received_bytes;
}

unsigned int get_avg_bytes() {
  printf("%d\n", avg_bytes);
  return avg_bytes;
}

void network_trap() {}

#include "kernel.h"
#include "ring_buff.h"


//define the size of the ring and each buffer inside it
#define RING_SIZE 16
#define BUFFER_SIZE 4096 // ask about buffer size



// pointer to memory-mapped I/0 region for network driver
volatile struct dev_net *net_driver;

volatile struct ring_buff *ring_buffer;

unsigned int freed_packets = 0;
unsigned int received_packets = 0;
unsigned int received_bytes = 0;

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

void network_poll(struct ring_buff** ring_buffers, int cores_reading) {
  int which_core = 0;
  struct dma_ring_slot* ring = (struct dma_ring_slot*) physical_to_virtual(net_driver->rx_base);
  while (1) {
    if (net_driver->rx_head != net_driver->rx_tail) {

      int index = which_core % cores_reading;
      which_core++;
      struct ring_buff* ring_buffer = ring_buffers[index];

      // Do nothing if ring buffer full (drop packet)
      int rb_head = ring_buffer->ring_head;
      int rb_tail = ring_buffer->ring_tail;

      // Ignore the packet if this buffer is full
      if (rb_head == rb_tail || (rb_head % ring_buffer->ring_capacity != rb_tail % ring_buffer->ring_capacity)) {

        // Get indices for use later
        int ring_base_index = ring_buffer->ring_head % ring_buffer->ring_capacity;
        int ring_index = net_driver->rx_tail % RING_SIZE;

        // access the buffer at the ring slot and retrieve the packet
        void* packet = physical_to_virtual(ring[ring_index].dma_base);

        // Add pointer to packet to specific core ring buffer
        received_packets++;
        received_bytes += ring[ring_index].dma_len;
        ring_buffer->ring_base[ring_base_index].dma_base = packet;
        ring_buffer->ring_base[ring_base_index].dma_len = ring[ring_index].dma_len;
        ring_buffer->ring_head++;

        // reallocate memory for ring buffer when done with packet, reset length and update the tail
        void* space = alloc_pages_safe(1);
        ring[ring_index].dma_base = virtual_to_physical(space);
        ring[ring_index].dma_len = BUFFER_SIZE;
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

void network_trap() {}

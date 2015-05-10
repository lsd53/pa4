#include "kernel.h"
#include "queue.h"

//define the size of the ring and each buffer inside it
#define RING_SIZE 16
#define BUFFER_SIZE 4096 // ask about buffer size



// pointer to memory-mapped I/0 region for network driver
volatile struct dev_net *net_driver;

void network_init(){
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

  /* allocate room for each buffer in the ring and set dma_base and dma_len to appropriate values */
  for (int i = 0; i < RING_SIZE; i++) {
    void* space = malloc(BUFFER_SIZE);
    ring[i].dma_base = virtual_to_physical((void *) space);
    ring[i].dma_len = BUFFER_SIZE;
  }
  puts("...keyboard driver is ready.");
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



void network_poll(Queue* q){
  //int k=0;
  struct dma_ring_slot* ring= (struct dma_ring_slot*) physical_to_virtual(net_driver->rx_base); 
  while(1){ 
    if (net_driver->rx_head != net_driver->rx_tail){
      // access the buffer at the ring slot and retrieve the packet
      void* packet = physical_to_virtual(ring[net_driver->rx_tail % RING_SIZE].dma_base);

      // store the packet in a queue for buffering by other cores
      queue_push(q, packet,ring[net_driver->rx_tail % RING_SIZE].dma_len);  
   
   
      free(packet);
      void* space = malloc(BUFFER_SIZE);
      ring[net_driver->rx_tail % RING_SIZE].dma_base = virtual_to_physical(space);
      ring[net_driver->rx_tail % RING_SIZE].dma_len = BUFFER_SIZE; 
      net_driver->rx_tail+=1;  
      
      // free memory used up by packet 
      
	/*  for (int i = 0; i < ring[net_driver->rx_tail % RING_SIZE].dma_len; i++){
  
    } */
      
      puts("Received packet");
    }
  }
}


void network_trap(){

}

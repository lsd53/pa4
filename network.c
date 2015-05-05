#include "kernel.h"


//define the size of the ring and each buffer inside it
#define RING_SIZE 16
#define BUFFER_SIZE 4096



// pointer to memory-mapped I/0 region for network driver
volatile struct network_dev *dev_net

void network_init(){
  /* Find virtual address of I/0 region for network driver */
  for (int i=0; i < 16; i++){
    if (bootparams->devtable[i].type == DEV_TYPE_NETWORK){
       //find virtual address of this region
      dev_net = physical_to_virtual(bootparams->devtable[i].start);
      break;
    }
  }
  /* allocate room for ring buffer and set rx_base to start of the array */
  struct dma_ring_slot* ring=(struct dma_ring_slot*) malloc(sizeof(struct dma_ring_slot) * RING_SIZE);
  dev_net->rx_base =  virtual_to_physical((void *) ring); //ASK ABOUT THIS PART
  dev_net->rx_capacity = RING_SIZE;  

  /* allocate room for each buffer in the ring and set dma_base and dma_len to appropriate values */
  for (i = 0; i < RING_SIZE; i++) {
    void* space = malloc(BUFFER_SIZE);
    ring[i].dma_base = virtual_to_physical((void *) space);
    ring[i].dma_len = BUFFER_SIZE;
  }
}

void network_start_recieve(){
  // write command into cmd register in order to turn on network card
  dev_net->cmd = NET_SET_POWER;
  dev_net->data = 1;
 
  // allow network card to recieve packets 
  dev_net->cmd = NET_SET_RECEIVE
  dev_net->data = 1; 
}



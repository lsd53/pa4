#include "kernel.h"


//define the size of the ring buffer
#define RING_SIZE 16

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
  dev_net[8] = 

}

#include "kernel.h"
#include "ring_buff.h"
#include "hashtable.h"

#define BUFFER_SIZE 4096 // ask about buffer size
#define CORES_READING 30
#define CORE_BUFFER_CAP 8

struct bootparams *bootparams;

int debug = 0; // change to 0 to stop seeing so many messages

void shutdown() {
  puts("Shutting down...");
  // this is really the "wait" instruction, which gcc doesn't seem to know about
  __asm__ __volatile__ ( ".word 0x42000020\n\t");
  while (1);
}

/* Hash function */
unsigned long djb2(unsigned char *pkt, int n) {
  unsigned long hash = 5381;
  int i = 0;
  while (i < n-8) {
    hash = hash * 33 + pkt[i++];
    hash = hash * 33 + pkt[i++];
    hash = hash * 33 + pkt[i++];
    hash = hash * 33 + pkt[i++];
    hash = hash * 33 + pkt[i++];
    hash = hash * 33 + pkt[i++];
    hash = hash * 33 + pkt[i++];
    hash = hash * 33 + pkt[i++];
  }
  while (i < n)
    hash = hash * 33 + pkt[i++];
  return hash;
}

/* Byte-wise endian switch */
int switch_endian(unsigned int old) {
  return  (old << 24) +
         ((old &  0x00FF0000) >> 8) +
         ((old &  0x0000FF00) << 8) +
          (old >> 24);
}

/* Byte-wise endian switch for short */
short switch_endian_short(unsigned short old) {
  return (old >> 8) + (old << 8);
}

/* Trap handling.
 *
 * Only core 0 will get interrupts, but any core can get an exception (or
 * syscall).  So we allocate enough space for every core i to have its own place
 * to store register state (trap_save_state[i]), its own stack to use in the
 * interrupt handler (trap_stack_top[i]), and a valid $gp to use during
 * interrupt handling (trap_gp[i]).
 */

struct mips_core_data *trap_save_state[MAX_CORES]; /* one save-state per core */
void *trap_stack_top[MAX_CORES]; /* one trap stack per core */
unsigned int trap_gp[MAX_CORES]; /* one trap $gp per core */

void trap_init()
{
  int id = current_cpu_id();

  /* trap should use the same $gp as the current code */
  trap_gp[id] = current_cpu_gp();

  /* trap should use a fresh stack */
  void *bottom = alloc_pages(4);
  trap_stack_top[id] = bottom + 4*PAGE_SIZE - 4;

  /* put the trap save-state at the top of the corresponding trap stack */
  trap_stack_top[id] -= sizeof(struct mips_core_data);
  trap_save_state[id] = trap_stack_top[id];

  /* it is now safe to take interrupts on this core */
  intr_restore(1);
}

void interrupt_handler(int cause)
{
  // note: interrupts will only happen on core 0
  // diagnose the source(s) of the interrupt trap
  int pending_interrupts = (cause >> 8) & 0xff;
  int unhandled_interrupts = pending_interrupts;

  if (pending_interrupts & (1 << INTR_KEYBOARD)) {
    if (debug) printf("interrupt_handler: got a keyboard interrupt, handling it\n");
    keyboard_trap();
    unhandled_interrupts &= ~(1 << INTR_KEYBOARD);
  }

  if (pending_interrupts & (1 << INTR_TIMER)) {
    printf("interrupt_handler: got a spurious timer interrupt, ignoring it and hoping it doesn't happen again\n");
    unhandled_interrupts &= ~(1 << INTR_TIMER);
  }

  if (unhandled_interrupts != 0) {
    printf("got interrupt_handler: one or more other interrupts (0x%08x)...\n", unhandled_interrupts);
  }

}

void trap_handler(struct mips_core_data *state, unsigned int status, unsigned int cause)
{
  if (debug) printf("trap_handler: status=0x%08x cause=0x%08x on core %d\n", status, cause, current_cpu_id());
  // diagnose the cause of the trap
  int ecode = (cause & 0x7c) >> 2;
  switch (ecode) {
    case ECODE_INT:   /* external interrupt */
      interrupt_handler(cause);
      return; /* this is the only exception we currently handle; all others cause a shutdown() */
    case ECODE_MOD:   /* attempt to write to a non-writable page */
      printf("trap_handler: some code is trying to write to a non-writable page!\n");
      break;
    case ECODE_TLBL:    /* page fault during load or instruction fetch */
    case ECODE_TLBS:    /* page fault during store */
      printf("trap_handler: some code is trying to access a bad virtual address!\n");
      break;
    case ECODE_ADDRL:   /* unaligned address during load or instruction fetch */
    case ECODE_ADDRS:   /* unaligned address during store */
      printf("trap_handler: some code is trying to access a mis-aligned address!\n");
      break;
    case ECODE_IBUS:    /* instruction fetch bus error */
      printf("trap_handler: some code is trying to execute non-RAM physical addresses!\n");
      break;
    case ECODE_DBUS:    /* data load/store bus error */
      printf("trap_handler: some code is read or write physical address that can't be!\n");
      break;
    case ECODE_SYSCALL:   /* system call */
      printf("trap_handler: who is doing a syscall? not in this project...\n");
      break;
    case ECODE_BKPT:    /* breakpoint */
      printf("trap_handler: reached breakpoint, or maybe did a divide by zero!\n");
      break;
    case ECODE_RI:    /* reserved opcode */
      printf("trap_handler: trying to execute something that isn't a valid instruction!\n");
      break;
    case ECODE_OVF:   /* arithmetic overflow */
      printf("trap_handler: some code had an arithmetic overflow!\n");
      break;
    case ECODE_NOEX:    /* attempt to execute to a non-executable page */
      printf("trap_handler: some code attempted to execute a non-executable virtual address!\n");
      break;
    default:
      printf("trap_handler: unknown error code 0x%x\n", ecode);
      break;
  }
  shutdown();
}

/**
 * Locks on a memory location
 */
void mutex_lock(int* m) {
  asm __volatile__ (
    ".set mips2 \n\t"
    "test_and_set: addiu $8, $0, 1\n\n"
    "ll $9, 0($4)\n\t"
    "bnez $9, test_and_set\n\t"
    "sc $8, 0($4)\n\t"
    "beqz $8, test_and_set\n\t"
  );
}

/**
 * Unlocks a locked memory location
 */
void mutex_unlock(int* m) {
  asm __volatile__ (
    ".set mips2 \n\t"
    "sw $0, 0($4)\n\t"
  );
}

int* mallock;
void* malloc_safe(unsigned int size) {
  // Mutex lock, call malloc, unlock
  mutex_lock(mallock);
  void* newly_malloced = malloc(size);
  mutex_unlock(mallock);
  return newly_malloced;
}

void* alloc_pages_safe(unsigned int num_pages) {
  // Mutex lock, call alloc_pages, unlock
  mutex_lock(mallock);
  void* newly_alloced = alloc_pages(num_pages);
  mutex_unlock(mallock);
  return newly_alloced;
}

struct ring_buff** queues;
struct hashtable* evil_packets;
struct hashtable* spammer_packets;
struct hashtable* vuln_ports;

int* is_printing;
void print_stats() {
  mutex_lock(is_printing);

  // Hashtable stats
  puts("Spammer stats");
  hashtable_stats(spammer_packets, "source address");

  puts("Evil stats");
  hashtable_stats(evil_packets, "hash");

  puts("Vulnerable stats");
  hashtable_stats(vuln_ports, "dest port");

  // Dropped packet stats
  unsigned int dropped = get_dropped_packets();
  double pkt_drop_rate = dropped * (double)CPU_CYCLES_PER_SECOND / current_cpu_cycles();
  printf("Dropped packets:\t%d (%f pkts/s)\n", dropped, pkt_drop_rate);

  // Received packet stats
  unsigned int r_pkts  = get_received_packets();
  double pkt_rec_rate  = r_pkts * (double)CPU_CYCLES_PER_SECOND / current_cpu_cycles();
  double byte_rec_rate = get_received_bytes() * (double)CPU_CYCLES_PER_SECOND / current_cpu_cycles() / (1024 * 1024) * 8;
  printf("Received packets:\t%d (%f pkts/s, %f Mbit/s)\n", r_pkts, pkt_rec_rate, byte_rec_rate);

  puts("");

  mutex_unlock(is_printing);
}

/* kernel entry point called at the end of the boot sequence */
void __boot() {

  if (current_cpu_id() == 0) {
    /* core 0 boots first, and does all of the initialization */

    // boot parameters are on physical page 0
    bootparams = physical_to_virtual(0x00000000);

    // initialize console early, so output works
    console_init();

    // output should now work
    printf("Welcome to my kernel!\n");
    printf("Running on a %d-way multi-core machine\n", current_cpu_exists());

    // initialize memory allocators
    mem_init();

    // prepare to handle interrupts, exceptions, etc.
    trap_init();

    // initialize keyboard late, since it isn't really used by anything else
    keyboard_init();

    // see which cores are already on
    for (int i = 0; i < 32; i++)
      printf("CPU[%d] is %s\n", i, (current_cpu_enable() & (1<<i)) ? "on" : "off");


    // allocate room for ring buffers that are to be used for additional queueing
    queues = malloc(sizeof(struct ring_buff*) * CORES_READING);

    unsigned int i;
    for (i = 0; i < CORES_READING; i++) {
      struct ring_buff* new_rb = malloc(sizeof(struct ring_buff));

      // initialize correct values for ring buffers allocated
      new_rb->ring_capacity = CORE_BUFFER_CAP;
      new_rb->ring_head = 0;
      new_rb->ring_tail = 0;
      new_rb->ring_base = (struct ring_slot*) malloc(sizeof(struct ring_slot) * CORE_BUFFER_CAP);

      int j;
      for (j= 0; j < CORE_BUFFER_CAP; j++) {
        new_rb->ring_base[j].dma_base = malloc(BUFFER_SIZE);
        new_rb->ring_base[j].dma_len = BUFFER_SIZE;
      }

      queues[i] = new_rb;
    }

    // Initialize int pointer for malloc_safe
    mallock = malloc(sizeof(int));
    *mallock = 0;

    // Initialize int pointer for printing
    is_printing = malloc(sizeof(int));
    *is_printing = 0;

    // Init hashtables
    evil_packets = malloc(sizeof(hashtable));
    spammer_packets = malloc(sizeof(hashtable));
    vuln_ports = malloc(sizeof(hashtable));
    hashtable_create(evil_packets);
    hashtable_create(spammer_packets);
    hashtable_create(vuln_ports);

    //initiliaze network driver and polling
    network_init(CORES_READING);
    network_start_receive();

    // turn on core 1
    set_cpu_enable(1 << 1);

  } else if (current_cpu_id() == 1) {
    // Core 1 polls the network
    set_cpu_enable(0xFFFFFFFF);
    network_poll(queues, CORES_READING);

  } else {

    // The remaining cores analyze packets
    int me = current_cpu_id() - (32 - CORES_READING);
    struct ring_buff* ring_buffer = queues[me];

    while (1) {
      if (ring_buffer->ring_head != ring_buffer->ring_tail) {

        // access the buffer at the ring slot and retrieve the packet
        struct ring_slot* ring = (struct ring_slot*) ring_buffer->ring_base;
        int ring_index = ring_buffer->ring_tail % ring_buffer->ring_capacity;
        void* packet = ring[ring_index].dma_base;

        // Use packet header values so that we can free the page
        struct honeypot_command_packet* pkt = (struct honeypot_command_packet*) packet;
        short little_secret = switch_endian_short(pkt->secret_big_endian);
        int little_src_addr = switch_endian((pkt->headers).ip_source_address_big_endian);
        short little_dest_port = switch_endian_short((pkt->headers).udp_dest_port_big_endian);

        // Command data
        short cmd = switch_endian_short(pkt->cmd_big_endian);
        int little_data = switch_endian(pkt->data_big_endian);

        unsigned long packet_hash = djb2((unsigned char*) packet, ring[ring_index].dma_len);

        free_pages(packet, 1);

        // Check if command packet
        if (little_secret == HONEYPOT_SECRET) {

          // It is a command packet
          switch (cmd) {
            // Note: Only add the data if it doesn't exist already
            case HONEYPOT_ADD_SPAMMER:
              hashtable_put_safe(spammer_packets, little_data, 0);
              break;

            case HONEYPOT_ADD_EVIL:
              hashtable_put_safe(evil_packets, little_data, 0);
              break;

            case HONEYPOT_ADD_VULNERABLE:
              hashtable_put_safe(vuln_ports, little_data, 0);
              break;

            case HONEYPOT_DEL_SPAMMER:
              hashtable_remove(spammer_packets, little_data);
              break;

            case HONEYPOT_DEL_EVIL:
              hashtable_remove(evil_packets, little_data);
              break;

            case HONEYPOT_DEL_VULNERABLE:
              hashtable_remove(vuln_ports, little_data);
              break;

            case HONEYPOT_PRINT:
              // Print statistics
              print_stats();
              break;
          }

        }

        // Increment if packet
        hashtable_increment(evil_packets, packet_hash);
        hashtable_increment(spammer_packets, little_src_addr);
        hashtable_increment(vuln_ports, (int)little_dest_port);

        // Increment head since packet was read
        ring[ring_index].dma_len = PAGE_SIZE;
        ring_buffer->ring_tail++;
      }
    }
  }

  while (1);

  shutdown();
}

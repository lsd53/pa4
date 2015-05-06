//Initializes network driver use the same approach as the keyboard driver.
void network_init();

void network_start_receive();

void network_set_interrupts(int opt);


void network_poll();

void network_trap();


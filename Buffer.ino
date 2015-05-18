#define BUFFER_SIZE 10

typedef struct circular_buffer
{
	unsigned long buffer[BUFFER_SIZE];
	volatile unsigned int head;
	volatile unsigned int tail;
}ring_buffer;

volatile circular_buffer my_buf = { { 0 }, 0, 0 };

void store_in_buffer(unsigned long data)
{
	unsigned int next = (unsigned int)(my_buf.head + 1) % BUFFER_SIZE;
	if (next != my_buf.tail)
	{
		my_buf.buffer[my_buf.head] = data;
		my_buf.head = next;
	}
}

unsigned long read_from_buffer()
{
	noInterrupts();
	// if the head isn't ahead of the tail, we don't have any characters
	if (my_buf.head == my_buf.tail) {
		interrupts();
		return 0;        // quit with an error
	}
	else {
		unsigned long data = my_buf.buffer[my_buf.tail];
		my_buf.tail = (unsigned int)(my_buf.tail + 1) % BUFFER_SIZE;
		interrupts();
		return data;
	}
}

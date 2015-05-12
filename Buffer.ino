#define BUFFER_SIZE 10

typedef struct circular_buffer
{
	unsigned long buffer[BUFFER_SIZE];
	volatile unsigned int head;
	volatile unsigned int tail;
}ring_buffer;

circular_buffer my_buf = { { 0 }, 0, 0 };

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
	// if the head isn't ahead of the tail, we don't have any characters
	if (my_buf.head == my_buf.tail) {
		return -1;        // quit with an error
	}
	else {
		char data = my_buf.buffer[my_buf.tail];
		my_buf.tail = (unsigned int)(my_buf.tail + 1) % BUFFER_SIZE;
		return data;
	}
}

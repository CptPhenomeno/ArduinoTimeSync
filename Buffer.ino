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


void simpLinReg(long double* x, long double* y, long double* lrCoef, int n){
	// pass x and y arrays (pointers), lrCoef pointer, and n.  The lrCoef array is comprised of the slope=lrCoef[0] and intercept=lrCoef[1].  n is length of the x and y arrays.
	// http://en.wikipedia.org/wiki/Simple_linear_regression

	// initialize variables
	long double xbar = 0;
	long double ybar = 0;
	long double xybar = 0;
	long double xsqbar = 0;

	// calculations required for linear regression
	for (int i = 0; i<n; i++){
		xbar = xbar + x[i];
		ybar = ybar + y[i];
		xsqbar = xsqbar + x[i] * x[i];
	}

	for (int i = 0; i<n; i++){
		xybar = xybar + x[i] * y[i];

	}


	xbar = xbar / n;
	ybar = ybar / n;
	xybar = xybar / n;
	xsqbar = xsqbar / n;

	// simple linear regression algorithm
	lrCoef[0] = (xybar - xbar*ybar) / (xsqbar - xbar*xbar);
	lrCoef[1] = ybar - lrCoef[0] * xbar;
}

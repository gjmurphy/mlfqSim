#ifndef QUEUE
#define QUEUE

typedef struct node_t {
	int index;
	struct node_t *next;
} node_t;

typedef struct queue_t {
	node_t *head;
	node_t *tail;
	int size;
} queue_t;

void push(queue_t *, int);

int pop(queue_t *);

void printQueue(queue_t *);

queue_t createQueue();

#endif
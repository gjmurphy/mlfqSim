/**
* queue.h
* Project 4 4760-E01
* Author: Gabriel Murphy (gjmcn6)
* Date: Mon Mar 20 STD 2017
* Summary: Definitions of the structs used for a queue
*/
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
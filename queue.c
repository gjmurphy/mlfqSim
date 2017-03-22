/**
* queue.c
* Project 4 4760-E01
* Author: Gabriel Murphy (gjmcn6)
* Date: Mon Mar 20 STD 2017
* Summary: Implementation of functions for queue
*/
#include "queue.h"
#include <stdlib.h>
#include <stdio.h>


/**
* Pushes a new node to back of queue
*/
void push(queue_t *queue, int index) {
	node_t *newNode;

	if ((newNode = calloc(1, sizeof(node_t))) == NULL) {
		perror("Failed to allocate memory for new node in queue");
		return;
	}

	newNode->next = NULL;
	newNode->index = index;

	// If queue is empty, set head to new node
	if (queue->head == NULL) {
		queue->head = newNode;
	}
	else {
		queue->tail->next = newNode;
	}

	queue->tail = newNode;

	queue->size++;
}

/**
* Removes first node of queue and returns the element 
*/
int pop(queue_t *queue) {
	node_t *node = queue->head;
	int index = node->index;

	queue->head = node->next;

	free(node);

	queue->size--;

	return index;
}

/**
* Prints the contents of the queue
* Really only used during debugging 
*/
void printQueue(queue_t *queue) {
	node_t *node = queue->head;

	printf("Queue:\n");

	if (node == NULL) {
		printf("Queue is empty\n");
	}

	while (node != NULL) {
		printf("%d\n", node->index);
		node = node->next;
	}
}

/**
* Initializes a new empty queue
*/
queue_t createQueue() {
	queue_t queue = {NULL, NULL, 0};

	return queue;
}
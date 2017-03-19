#include "queue.h"
#include <stdlib.h>
#include <stdio.h>

void push(queue_t *queue, int index) {
	node_t *newNode;

	if ((newNode = calloc(1, sizeof(node_t))) == NULL) {
		perror("Failed to allocate memory for new node in queue");
		return;
	}

	newNode->next = NULL;
	newNode->index = index;

	if (queue->head == NULL) {
		queue->head = newNode;
	}
	else {
		queue->tail->next = newNode;
	}

	queue->tail = newNode;

	queue->size++;
}

int pop(queue_t *queue) {
	node_t *node = queue->head;
	int index = node->index;

	queue->head = node->next;

	free(node);

	queue->size--;

	return index;
}

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

queue_t createQueue() {
	queue_t queue = {NULL, NULL, 0};

	return queue;
}
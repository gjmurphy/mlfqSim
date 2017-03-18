#include "stime.h"

void incrementTime(struct stime_t *time, int delta) {
	time->nnsec += delta;
	
	while (time->nnsec >= NS_PER_S) {
		time->sec++;
		time->nnsec -= NS_PER_S;
	}	
}

int gte(struct stime_t *a, struct stime_t *b) {
	if ((a->sec > b->sec) || (a->sec == b->sec && a->nnsec >= b->nnsec)) {
		return 1;
	}

	return 0;
}
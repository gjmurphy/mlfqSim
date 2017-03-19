#include "stime.h"

void incrementTime(stime_t *time, long long delta) {
	if (delta < NS_PER_S) {
		time->nnsec += delta;

		if (time->nnsec >= NS_PER_S) {
			time->sec++;
			time->nnsec -= NS_PER_S;
		}
	}
	else {
		while (delta >= NS_PER_S) {
			time->sec++;
			delta -= NS_PER_S;
		}

		time->nnsec = delta;
	}	
}

int gte(stime_t *a, stime_t *b) {
	if ((a->sec > b->sec) || (a->sec == b->sec && a->nnsec >= b->nnsec)) {
		return 1;
	}

	return 0;
}

long long combined(stime_t *time) {
	return ((long long) time->sec * NS_PER_S + (long long) time->nnsec);
}
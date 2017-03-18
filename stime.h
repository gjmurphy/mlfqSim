#ifndef S_TIME
#define S_TIME

// Number of nanoseconds in second
#define NS_PER_S 1000000000

struct stime_t {
	int sec;
	int nnsec;
};

void incrementTime(struct stime_t*, int);

int gte(struct stime_t *, struct stime_t *);

#endif
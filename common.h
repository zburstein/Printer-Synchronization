#ifndef COMMON_H_
#define COMMON_H_

typedef struct jobs
{
	int clientID;
	int duration;
}jobs;

typedef struct shared_data
{
	jobs jobsList[10];
}shared_data;

#endif //common_H


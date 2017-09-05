#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include "common.h"

shared_data* attach_shared_mem(const char *name);//attaches shared memory and returns a pointer to it
jobs* create_a_job(int clientID, int duration); //creates the new job based upon use input
int put_a_job(sem_t *availSpace, sem_t *mutex, jobs *newJob, shared_data *shared_mem_ptr); //puts the new job in the shared job buffer once space si available. returns its buffer location
void print_a_msg(sem_t *printerMutex, jobs *newJob, int location); //prints the stack strace message
void notify_printer(sem_t *takenSpace);//notifies the printer that client has entered the job. AS anote, this is made a seperate funciton after print_a_msg for ease of understanding the stack trace and oreder of events. could instead be placed at end of put_a_job
void release_shared_mem(shared_data *shared_mem_ptr);//release the mem

int main(int argc, char *argv[])
{
	jobs *newJob;
	shared_data *shared_mem_ptr;
	int clientID, duration, location;
	const char *mutexName = "mutex", *availName = "availSpace", *takenSpaceName = "takenSpace", *printerMutexName = "printerMutex", *name = "/printer"; //names
	sem_t *mutex, *availSpace, *takenSpace, *printerMutex; //the semaphores

	//check for proper inputs. exit if failed
	if(argc < 3 || argc > 4 || (argc == 4 && strcmp(argv[3], "&") != 0))
	{
		printf("\nIncorrect input: \"./client.exe clientID duration &(optional)\"\n");
		exit(EXIT_FAILURE);
	}

	shared_mem_ptr = attach_shared_mem(name); //open and attatch shared memory

	//link with the open semaphores
	mutex = sem_open(mutexName, 0, 0666);
	availSpace = sem_open(availName, 0, 0666);
	takenSpace = sem_open(takenSpaceName, 0, 0666);
	printerMutex = sem_open(printerMutexName, 0, 0666);

	//Seemed unnecessary to call a function to get params since they are command line params so I just assign them in main here
	clientID = atoi(argv[1]);
	duration = atoi(argv[2]);

	newJob = create_a_job(clientID, duration);//create teh new job to place in buffer
	location = put_a_job(availSpace, mutex, newJob, shared_mem_ptr); //put a job. Waits until space is available. returns location in buffer
	print_a_msg(printerMutex, newJob, location); //print a message saying that
	notify_printer(takenSpace);//notify the printer that a job is available in buffer. PLaced this after print_a_msg for ease in understanding stacktrace and debugging.
	// Could instead go at end of putajob function
	release_shared_mem(shared_mem_ptr);//then release the shared mem
	free(newJob);//free the job that was created in client before termination
}  

shared_data* attach_shared_mem(const char *name)
{
	int shm_fd;
	shared_data *shared_mem_ptr;

	shm_fd = shm_open(name, O_RDWR, 0666); //open the shared memory
	if (shm_fd == -1) 
	{
		perror("Open failed");
		exit(EXIT_FAILURE);//exit if failed
	}
	if (ftruncate(shm_fd, sizeof(shared_data)) == -1) //Truncate to proper size
	{
		perror("Failed truncate");
		exit(EXIT_FAILURE);//exit if failed
	}
	shared_mem_ptr = (shared_data *) mmap(NULL, sizeof(shared_data), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0); //map it
	if (shared_mem_ptr == MAP_FAILED)
	{
		perror("Failed");
		exit(EXIT_FAILURE);//exit if failed
	}

	return shared_mem_ptr;//return the pointer to the shared memory
}

jobs* create_a_job(int clientID, int duration)
{
	//create a new job based upon user inputs
	jobs *newJob;  
	newJob = (jobs *) malloc(sizeof(jobs));
	newJob->clientID = clientID;
	newJob->duration = duration;
	return newJob;
}	

int put_a_job(sem_t *availSpace, sem_t *mutex, jobs *newJob, shared_data *shared_mem_ptr)
{

	int location; 

	sem_wait(availSpace); //need to wait until there is an available slot to edit. will decrement when it can go through indicating a space was taken up

	sem_wait(mutex);//if space available need the lock before making any changes
	//once recieves lock it enters critical section and makes changes to shared memory
	if(sem_getvalue(availSpace, &location)) //recieves lowest free locaiton in buffer
		perror("Failed getValue");
	shared_mem_ptr->jobsList[10 - location - 1].clientID = newJob->clientID;//set clientID in shared mem
	shared_mem_ptr->jobsList[10 - location - 1].duration = newJob->duration;//set duration in shared mem
	
	sem_post(mutex);//release the lock

	return location;// retunr location in buffer
}

void print_a_msg(sem_t *printerMutex, jobs *newJob, int location)
{
	//print to screen what happened
	sem_wait(printerMutex); //get printer mutex before printing
	printf("\nClient %d put %d page(s) into buffer location %d\n", newJob->clientID, newJob->duration, 10 - (location  + 1));
	sem_post(printerMutex); //release the lock
	return;
}

void notify_printer(sem_t *takenSpace)
{
	sem_post(takenSpace); //increment taken space to indicate there is a value to be printed
	//could be done in retrieve a job but is placed here instead to ease the reading of the stack trace. 
	//ensures client will print that it placed its job before printer prints that it is executing that job
	return;
}

void release_shared_mem(shared_data *shared_mem_ptr)
{
	//remove this mapping from clients memory
	if(munmap(shared_mem_ptr, sizeof(shared_data)))
	{
		perror("Client unlink failed");	
		exit(EXIT_FAILURE);
	}
	return;
}

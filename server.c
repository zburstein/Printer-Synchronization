#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <time.h>
#include "common.h"
#include  <signal.h>

//global variables because makes signal handler easier
const char *mutexName = "mutex", *availName = "availSpace", *takenSpaceName = "takenSpace", *printerMutexName = "printerMutex", *name = "/printer";
sem_t *mutex, *availSpace, *takenSpace, *printerMutex;

int setup_shared_mem(); //opens and truncates shared memory file
shared_data* attatch_shared_mem(int shm_fd); //attatches sharedmemory file with mmap
sem_t* init_semaphore(const char *name, int initVal); //initializes named semaphores

jobs take_a_job(shared_data *shared_mem_ptr); //pulls a job from the buffer
void print_a_msg(jobs currentJob); //prints message to screen about printer actions
void go_sleep(jobs currentJob); //goes to sleep as a simulation of the print
void removeJob(shared_data *shared_mem_ptr); //removes the job from the buffer

void semUnlinkClose (const char *name, sem_t *semaphore); //function declaration to remove semaphores
void INThandler(int sig); //to handle exit

int main()
{
	signal(SIGINT, INThandler);//if to exit. ctrl c. can exit gracefully

	int shm_fd;
	shared_data *shared_mem_ptr;
	jobs currentJob;

	shm_fd = setup_shared_mem();//set up shared memory and hold file descript in shm_fd
	shared_mem_ptr = attatch_shared_mem(shm_fd);//map the shared memory and hold it in shared_mem_ptr

	//initialize the semaphores
	mutex = (sem_t *) init_semaphore(mutexName, 1); //mutex lock
	availSpace = (sem_t *) init_semaphore(availName, 10); //the number of spaces available. initialized to 10
	takenSpace = (sem_t *) init_semaphore(takenSpaceName, 0); //no spaces taken. initialized to 0 b/c forces wait in server until signal from client
	printerMutex = (sem_t *) init_semaphore(printerMutexName, 1); //printer mutex to ensure only one process printing to screen at one time for purpose of trace

	//repeat unitl process is cancelled
	while(1)
	{
		currentJob = take_a_job(shared_mem_ptr); //retrieve job when one is available	
		print_a_msg(currentJob); //print the message
		go_sleep(currentJob);//wait for duration of job
		removeJob(shared_mem_ptr);//remove it from memory
	}
}

int setup_shared_mem()
{
	int x;
	x = shm_open(name, O_CREAT | O_RDWR, 0666); //open the shared memory
	if (x == -1)
	{
		perror("Failed open"); //if it failed
		exit(EXIT_FAILURE);
	}
	if (ftruncate(x, sizeof(shared_data)) == -1) //Truncate to size
	{
		perror("Failed truncate");//if it failed
		exit(EXIT_FAILURE);
	}
	return x; //return the file descriptor
}

shared_data* attatch_shared_mem(int shm_fd)
{

	shared_data *shared_mem_ptr;
	shared_mem_ptr = (shared_data *) mmap(NULL, sizeof(shared_data), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0); //map the memory
	if (shared_mem_ptr == MAP_FAILED)//if it failed
	{
		perror("Map failed");
		exit(EXIT_FAILURE);
	}
	
	return shared_mem_ptr;//return the pointer
}

sem_t* init_semaphore(const char *semName, int initVal) //initialize the semaphore of given name
{
	return sem_open(semName, O_CREAT, 0666, initVal); //creates the semaphore
}


jobs take_a_job(shared_data *shared_mem_ptr)
{
	jobs currentJob;
	sem_wait(takenSpace);//Need to wait until a job is created by client. If 0, waits until signalled by client
	currentJob = shared_mem_ptr->jobsList[0]; //there is a job available so set it equal to current job and return it
	return currentJob; //return the job being printed
}

void print_a_msg(jobs currentJob)
{
	sem_wait(printerMutex); //wait until no one else is printing to screen
	printf("\nPrinter starts printing %d pages from client %d\n", currentJob.duration, currentJob.clientID); //print
	sem_post(printerMutex); //release the lock so other processes can print
}

void go_sleep(jobs currentJob)
{
	//wait the duration to mimic the print
	int retTime;
	retTime = time(0) + currentJob.duration;
	while(time(0) < retTime);
}

void removeJob(shared_data *shared_mem_ptr)
{

	int i;
	sem_wait(mutex);//need to wait for the lock to manipulate the data	
	//remove last print job from memory
	for (i = 0; i < 10; i++) 
	{
		shared_mem_ptr->jobsList[i].clientID = shared_mem_ptr->jobsList[i+1].clientID;
		shared_mem_ptr->jobsList[i].duration = shared_mem_ptr->jobsList[i+1].duration;
	}

	sem_post(availSpace);//increments availspace when it is complete while inside the mutex to ensure that things wont be written in wrong place

	sem_post(mutex); //release the lock

}

void semUnlinkClose (const char *name, sem_t *semaphore)
{
	//unlink and close the semaphores
	sem_unlink(name);
	sem_close(semaphore);
}

void INThandler(int sig)
{
	char  c;

	signal(sig, SIG_IGN);
	printf("Other processes may not be finished. Do you really want to quit? [y/n] ");
	c = getchar();
	if (c == 'y' || c == 'Y')
	{
		//unlink and close everything
		semUnlinkClose(mutexName, mutex);
		semUnlinkClose(availName, availSpace);
		semUnlinkClose(takenSpaceName, takenSpace);
		semUnlinkClose(printerMutexName, printerMutex);
		if(shm_unlink(name))
			perror("Unlink failed");
		exit(EXIT_SUCCESS);
	}
	else
		signal(SIGINT, INThandler);
	getchar(); // Get new line character
}

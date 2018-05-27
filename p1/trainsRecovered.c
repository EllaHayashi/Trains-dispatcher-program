//By Ella hayashi
//csc360 Assignment 2

#include <pthread.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <math.h>


pthread_cond_t cv;		//used to signal all threads to start loading
pthread_mutex_t lock;		//used to lock threads in so that they dont start loading right away
pthread_mutex_t m;		//used to protect the linked list when trains are adding them in.
pthread_mutex_t dispatcher;	//used for the trains crossing the main track, and lock out the others

typedef struct train train;	//train list
struct train
{
	int trainNum;		//unique train number, given in order of input file read in
	char direction;		//direction either 'e', 'E', 'w', or 'W'
	char* fullDirectionName;	//full direction name, either East or West
	int loading_time;		
	int crossing_time;
	train *next;			
	double lastKnownTime;		//accumulated time, (crossing, waiting for other trains ext)
	double actualLoadingTime;	//acctual loading time of the trains
	int ready;			//after loading, set ready to 1,
	int send;			//after sent to main track, set send to 1
};


train *east_LP = NULL;	
train *east_HP = NULL;
train *west_HP = NULL;
train *west_LP = NULL;
int is_finished = 0;		//flag that says if all the train threads have been joined, and lets dispatcher know to stop
int eastGone = 0;		//flag that says if the last train was east.
int trainGone = 0;		//flat that tells dispatcher if there is currently a train on the main track
int num = 0;			//used for train number (trainNum)

/*
* addEnd is a function that inserts a train into a linked list. It will compare itself to the elements
* in the linked list, and if there is a train with the same loading time, then it will verify it's
* position in the linked list by it's trainNum. Otherwise, it will be inserted into the end of the 
* list
* it returns the front of the list.
*/
train *addEnd(train* front, train* insertionNode)
{

	if(front ==NULL)	//if linked list is empty
	{
		return insertionNode;	
	}

	train *p;
	//if the loading time of the front node is the same, and the front node has a higher trainNum,
	//then new node becomes front
	if(insertionNode->actualLoadingTime == front->actualLoadingTime && insertionNode->trainNum < front->trainNum)
	{
		insertionNode->next = front;
		return insertionNode;
	}
	
	//if we find a node with the same loading time but a higher trainNum, then the new
	//node get's inserted before it.
	for(p=front; p->next !=NULL; p=p->next)
	{
		if(insertionNode->actualLoadingTime == p->next->actualLoadingTime && insertionNode->trainNum < p->next->trainNum)
		{
			insertionNode->next = p->next;
			p->next = insertionNode;
			return front;
		}
	}
	p->next = insertionNode;
	return front;
}


/*
* this function takes an input line and tokenizes it.
* returns a new train node 
*/
train *tokenize(char *input_line)
{
	train *newTrain;
	newTrain = (train *)malloc(sizeof(train));

	char *t = strtok(input_line, " \n");
	newTrain->direction = t[0];				//direction is either e, E, w, or W
	if(newTrain->direction == 'E'||newTrain->direction == 'e')
	{
		newTrain->fullDirectionName = "East";		//direction name
	}
	if(newTrain->direction == 'W'|| newTrain->direction == 'w')
	{
		newTrain->fullDirectionName = "West";
	}

	t = strtok(NULL, " \n");
	newTrain->loading_time = atoi(t);

	t = strtok(NULL, " \n");
	newTrain->crossing_time = atoi(t);

	newTrain->trainNum = num;
	num++;
	newTrain->ready = 0;
	newTrain->send = 0;	

	return (newTrain);	
	
}		


//time function used for clock calculations into miliseconds
static double timeFunction(struct timespec* ts)
{
	return (double)ts->tv_sec + (double)ts->tv_nsec/1000000000.0;
}

//print the time when train is ready to go
void printReadyTime(double elapsedTime, train* temp)
{
	int hours = 0;
	int minets = 0;
	float seconds = 0;
	if(elapsedTime > 3600)
	{
		hours = elapsedTime/3600;
		elapsedTime = elapsedTime - (hours * 3600);
	}	
	if(elapsedTime > 60)
	{
		minets = elapsedTime/60;
		elapsedTime = elapsedTime - (minets * 60);
	}
	printf("%02d:%02d:%04.1f Train  %d is ready to go %s\n", 
		hours, minets, elapsedTime, temp->trainNum, temp->fullDirectionName);
}

//print the time when train is on the main track
void printOnTime(double elapsedTime, train* temp)
{
	int hours = 0;
	int minets = 0;
	float seconds = 0;
	if(elapsedTime > 3600)
	{
		hours = elapsedTime/3600;
		elapsedTime = elapsedTime - (hours * 3600);
	}	
	if(elapsedTime > 60)
	{
		minets = elapsedTime/60;
		elapsedTime = elapsedTime - (minets * 60);
	}
	printf("%02d:%02d:%04.1f Train  %d is ON the main track going %s\n", 
		hours, minets, elapsedTime, temp->trainNum, temp->fullDirectionName);
}

//print the time when the train is off the main track
void printOffTime(double elapsedTime, train* temp)
{
	int hours = 0;
	int minets = 0;
	float seconds = 0;
	if(elapsedTime > 3600)
	{
		hours = elapsedTime/3600;
		elapsedTime = elapsedTime - (hours * 3600);
	}	
	if(elapsedTime > 60)
	{
		minets = elapsedTime/60;
		elapsedTime = elapsedTime - (minets * 60);
	}
	printf("%02d:%02d:%04.1f Train  %d is OFF the main track after going %s\n", 
		hours, minets, elapsedTime, temp->trainNum, temp->fullDirectionName);
}

/*
* Thread function
* This function deals with waiting before it is loading, loading, waiting to cross the main track, and crossing the main track.
* before the trains are loaded, we wait until all trains are read in, and so they are sent at the same time
* the trains all load for their loading time, and we record the time it takes
* the trains then wait until it is their turn to go on the main track, we record this time
* one train at a time get's crossing time on the main track
*/
void *threads(void *newtrain)
{	

	//WAITING BEFORE LOADING
	pthread_mutex_lock(&lock);		
	pthread_cond_wait(&cv, &lock);		//make all threads wait here until they are all told to start loading
	pthread_mutex_unlock(&lock);	
	
	clockid_t clock;		
	struct timespec start, end;	
	train *temp = newtrain;

	clock_gettime(CLOCK_MONOTONIC, &start);		//start clock
	usleep(temp->loading_time*100000);		//train sleeps for loading time
	clock_gettime(CLOCK_MONOTONIC, &end);		//stop clock
	double elapsedTime = timeFunction(&end) - timeFunction(&start);

	temp->actualLoadingTime = elapsedTime;
	temp->lastKnownTime = elapsedTime;
	printReadyTime(elapsedTime, temp);		//print out the train is finished loading
	
	//ADDING NODE TO TRAIN LIST
	pthread_mutex_lock(&m);				//protecting the linked list when adding a train to it
	if(temp->direction == 'E')
	{
		east_HP = addEnd(east_HP, temp);
	}
	else if(temp->direction == 'e')
	{
		east_LP = addEnd(east_LP, temp);
	}
	else if(temp->direction == 'W')
	{
		west_HP = addEnd(west_HP, temp);
	}
	else
	{
		west_LP = addEnd(west_LP, temp);
	}
	pthread_mutex_unlock(&m);
	temp->ready = 1;				//train is ready to be loaded onto the main track
	
	//WAITING TO CROSS ON MAIN TRACK
	clock_gettime(CLOCK_MONOTONIC, &start);		//start the clock
	while(temp->send == 0)				//busy waiting loop until dispatcher tells train it can be sent
	{
	}
	clock_gettime(CLOCK_MONOTONIC, &end);		//stop the clock when train is ready to be loaded onto the main track

	trainGone = 1;					//trainGone to tell dispatcher that there is a train on the main track

	//CROSSING ON MAIN TRACK
	pthread_mutex_lock(&dispatcher);		
	elapsedTime = timeFunction(&end) - timeFunction(&start);	
//	temp->lastKnownTime = temp->lastKnownTime + floor(10*elapsedTime)/10;
	temp->lastKnownTime = temp->lastKnownTime + elapsedTime;
	elapsedTime = temp->lastKnownTime;
	printOnTime(elapsedTime, temp);			//print out the time it took till train was on the main track
	
	
	clock_gettime(CLOCK_MONOTONIC, &start);		//start the clock
	usleep(temp->crossing_time*100000);		//train sleeps for crossing time
	clock_gettime(CLOCK_MONOTONIC, &end);		//stop the clock when crossing time over

	elapsedTime = timeFunction(&end) - timeFunction(&start);
	temp->lastKnownTime = temp->lastKnownTime + elapsedTime;
	elapsedTime = temp->lastKnownTime;
	printOffTime(elapsedTime, temp);		//print the time it took

						//remove the train from the list and
	if(temp->direction == 'E')		//report if the train was east or west for scheduling reasons
	{
		east_HP = east_HP->next;
		eastGone = 1;
	}
	else if(temp->direction == 'e')
	{
		east_LP = east_LP->next;
		eastGone = 1;
	}
	else if(temp->direction == 'W')
	{
		west_HP = west_HP->next;
		eastGone = 0;
	}
	else
	{
		west_LP = west_LP->next;
		eastGone = 0;
	}	
	free(temp);				//free the node on the linked list
	trainGone = 0;
	pthread_mutex_unlock(&dispatcher);

	return NULL;

}

/*
* Scheduler decides which train will be loaded onto the main track next
*/

void *scheduler()
{	
	//Continue to loop until all linked lists are empty, or all train threads have been joined (is_finished flag)
	while((west_HP != NULL || west_LP != NULL || east_HP != NULL || east_LP != NULL) || 
		is_finished == 0)
	{	
		//If there is a high priority train in a list, then we send it out before lower priority
		if(west_HP!=NULL||east_HP!=NULL)
		{
			//If two trains have the same loading time, but the last train to have gone was east bound
			//If there is only a west bound train, and there is no east bound train
			//If the west bound train had a smaller loading time then the east bound train
			if(((east_HP != NULL && west_HP != NULL) 
				&& (west_HP->actualLoadingTime == east_HP->actualLoadingTime) && eastGone == 1)
				||(east_HP==NULL)
				||((east_HP != NULL && west_HP != NULL) 
				&& (west_HP->actualLoadingTime < east_HP->actualLoadingTime)))
			{
				train *temp = west_HP;
				if (temp->ready = 1) //train has to be ready to leave
				{
					temp->send = 1;	//tell train it can cross the main track
					trainGone = 1;	
				}
			}

			//otherwise send the east bound train
			else
			{
				train *temp = east_HP;
				if (temp->ready = 1)
				{
					temp->send = 1;
					trainGone = 1;
				}
			}
		}
		
		//same scheduling for lower priority lists as for higher priority lists
		else if(west_LP!=NULL||east_LP!=NULL)
		{
			if(((east_LP != NULL && west_LP != NULL) 
				&& (west_LP->actualLoadingTime == east_LP->actualLoadingTime) && eastGone == 1)
				||(east_LP==NULL)
				||((east_LP !=NULL && west_LP != NULL) 
				&& (west_LP->actualLoadingTime < east_LP->actualLoadingTime)))
			{
				train *temp = west_LP;
				if (temp->ready = 1)
				{
					temp->send = 1;
					trainGone = 1;
				}
			}
			else
			{
				train *temp = east_LP;
				if (temp->ready = 1)
				{
					temp->send = 1;
					trainGone = 1;
				}
			}
		}			
		
		// if a train is on the main track, then we do not want to send another, so wait until its done
		while(trainGone == 1 && is_finished != 1)
		{
		}
	}		
}

/*
* Main function deals with reading in the input file, creating a train node, creating the train threads
* broadcasting to all threads when they can start loading, and then joining all the threads back together
*/
int main(int argc, char *argv[])
{

	FILE *fp;
	char* line = NULL;
	size_t len = 0;
	size_t read;
	fp = fopen(argv[1], "r");
	if (fp==NULL)
	{
		exit(1);	
	}

	int n = 10;
	pthread_t schedulerThd[1];
	pthread_create(&schedulerThd[0], NULL, scheduler, NULL);

	pthread_t *tid = (pthread_t *) malloc (sizeof(pthread_t) * n);
	int threadInt = 0;
	
	while((read = getline(&line, &len, fp)) != -1)
	{
		train *newtrain = tokenize(line);
		if(threadInt >= n)
		{
			n = n*2;
			tid = (pthread_t *) realloc(tid, n*sizeof(pthread_t));
		}				
		pthread_create(&tid[threadInt], NULL, threads, newtrain);
		threadInt++;
	}
	sleep(1);
	pthread_cond_broadcast(&cv);			//tells all threads they can start loading

	int i;
	for(i=0; i<threadInt; i++)
	{
		pthread_join(tid[i], NULL); 
	}

	is_finished = 1;
	pthread_join(schedulerThd[0], NULL);	
	pthread_mutex_destroy(&lock);
	pthread_mutex_destroy(&m);
	pthread_mutex_destroy(&dispatcher);
	free(tid);


}
















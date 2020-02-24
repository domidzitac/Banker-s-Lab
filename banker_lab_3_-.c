#ifdef _WIN32
#define _CRT_SECURE_NO_DEPRECATE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <tgmath.h>
#define QUEUE_SIZE 1000

char inputFile[256]; // name of input file
int T; // number of tasks
int R; // number of resources
int* totalResource; // number of units for each resource;
int* availableResource; // number of units available at a given cycle


/* Data structures for Banker*/
int** maxDemand; // an array of TxR with the maximum initial claim of each process;
/**/

int* releaseResourceBucket; // when a process releases resource put it in this array. at next cycle add it to availableResource

int* checkOrder; // an array of values form 1 to T that gives us at each cycle the order in which tasks have to be ckecked;
// first we check blocked processes (that are in the waiting queue and then the nonblocked in increasing order of id

const char* activities[] = { "initiate", "request", "release", "compute", "terminate" };
const int activityCount = 5;
enum Activity
{
	Initiate,
	Request,
	Release,
	Compute,
	Terminate
};
struct TaskData
{
	enum Activity activity;
	int n1; // for inititate and release: resource-type, for compute: number of cycles, for terminate unused;
	int n2; // for initiate initial claim; for request, release: number requested/released, for terminate unused;
};




struct Node
{
	struct TaskData data;
	struct Node* next;
};
struct TaskActivityList
{
	struct Node* first, * last;
	struct Node* current;
};

struct TaskActivityList** taskData;
enum TaskState
{
	Initiating,
	CompleteRequest,
	Computing,
	Releasing,
	Aborted,
	Terminated,
	Blocked
};
struct Task
{
	enum Activity activity;
	enum TaskState state;
	int* alocatedResources;
	int waitingTime; // total waiting time for a process (part of output) 
	int timeTaken; //  time taken for a process (part of output);
	int cycles; // number o cycles in computing state; (decreases by one at each cycle)

	int* initialClaim; // used only by BANKER
};


struct Task* taskRunFIFO;
struct Task* taskRunBANKER;


// for debugging purpose; print on stdout the raw data read from the input file
void printTaskActivities()
{
	struct TaskActivityList* p;
	struct Node* node;
	for (int i = 1; i <= T; i++)
	{
		p = taskData[i];
		node = p->first;
		while (node != NULL)
		{
			printf("%s %d %d %d\n", activities[node->data.activity], i, node->data.n1, node->data.n2);
			node = node->next;
		}
		printf("\n");
	}
}

// Read input. Initialise data
void readData(char* file)
{
	FILE* f;
	int i, count;
	int n1, n2, n3; // for the three numeric values in each activity;
	char activity[20] = ""; // activity name.
	struct Node* aux;
	f = fopen(file, "r");
	if (f == NULL)
	{
		printf("Error: cannot open file %s. Terminated.", file);
		exit(1);
	}

	count = fscanf(f, "%d %d", &T, &R);
	if (count == -1)
	{
		printf("Something is wrong in the input file.");
		exit(1);
	}
	totalResource = (int*)malloc(sizeof(int) * (R + 1));
	availableResource = (int*)malloc(sizeof(int) * (R + 1));
	releaseResourceBucket = (int*)malloc(sizeof(int) * (R + 1));
	checkOrder = (int*)malloc(sizeof(int) * (T + 1));

	maxDemand = (int**)malloc(sizeof(int*) * (T + 1));
	for (i = 1; i <= T; i++)
		maxDemand[i] = (int*)malloc(sizeof(int) * (R + 1));

	for (i = 1; i <= R; i++)
	{
		count = fscanf(f, "%d", &(totalResource[i]));
		if (count == -1)
		{
			printf("Something is wrong in the input file.");
			exit(1);
		}
		availableResource[i] = totalResource[i];
		releaseResourceBucket[i] = 0;
	}



	taskData = (struct TaskActivityList**)malloc(sizeof(struct TaskActivityList*) * (T + 1));
	if (taskData == NULL)
	{
		printf("Not enough memory");
		exit(1);
	}
	for (i = 1; i <= T; i++) // index is the process id
		taskData[i] = NULL;
	do
	{
		count = fscanf(f, "%s %d %d %d", activity, &n1, &n2, &n3);
		if (count == -1)
			break;

		if (strcmp(activity, "initiate") == 0)
			maxDemand[n1][n2] = n3;

		int index = -1;
		for (i = 0; i < activityCount; i++)
			if (strcmp(activity, activities[i]) == 0)
			{
				index = i;
				break;
			}
		if (index == -1)
		{
			printf("Invalid activity read from file.");
			exit(1);
		}


		// Creating new Node with data
		aux = (struct Node*)malloc(sizeof(struct Node));
		if (aux)
		{
			aux->next = NULL;
			aux->data.activity = (enum Activity)index;
			aux->data.n1 = n2;
			aux->data.n2 = n3;
		}


		if (taskData[n1] == NULL)
		{
			taskData[n1] = (struct TaskActivityList*)malloc(sizeof(struct TaskActivityList));
			if (taskData[n1] == NULL)
			{
				printf("Not enough memory");
				exit(1);
			}
			taskData[n1]->first = taskData[n1]->last = taskData[n1]->current = aux;

		}
		else
		{
			if (taskData[n1]->last != NULL)
			{
				taskData[n1]->last->next = aux;
				taskData[n1]->last = aux;
			}

		}
		//printf("%s %d %d %d\n", activity, n1, n2, n3);
	} while (1);

	//printTaskActivities();
}


void printResults(struct Task* taskRun)
{
	int totalWaitingTime = 0, totalTimeTaken = 0;
	int terminatedProcess = 0;
	for (int i = 1; i <= T; i++)
	{
		printf("Task%3d", i);
		if (taskRun[i].state == Aborted)
			printf("aborted\n");
		else
		{
			printf("%6d%6d%6.0lf%%\n", taskRun[i].timeTaken, taskRun[i].waitingTime,
				(double)taskRun[i].waitingTime / taskRun[i].timeTaken * 100);
			totalWaitingTime += taskRun[i].waitingTime;
			totalTimeTaken += taskRun[i].timeTaken;
		}
	}
	printf("total:%7d%6d%6.0lf%%\n\n", totalTimeTaken, totalWaitingTime, (double)totalWaitingTime / totalTimeTaken * 100);
}


// check if all tasks terminated or aborted 
int runningTaskCount(struct Task* tasks)
{
	int count = 0, state;
	for (int i = 1; i <= T; i++)
	{
		state = tasks[i].state;
		if (state == Initiating || state == CompleteRequest || state == Computing || state == Releasing || state == Blocked)
			count++;
	}

	return count;
}

void UpdateAvailableResources()
{
	for (int i = 1; i <= R; i++)
	{
		availableResource[i] += releaseResourceBucket[i];
		releaseResourceBucket[i] = 0;
	}
}


/*Begin Waiting Queue implementation*/
/* Basic Queue implementation - array that has two indices left, right; we add a new element at right and remove from left */
int waitingQueue[QUEUE_SIZE] = { 0 }; // 
int left = 0, right = 0;

// enqueue operation
void addTaskToWaitingQueue(int task)
{
	waitingQueue[right++] = task;
}
// dequeue operation
int getTaskFromWaitingQueue()
{
	if (left < right)
		return waitingQueue[left++];
	else
	{
		printf("Trying to remove from empty queue");
		return -1;
	}
}

// remove an arbitrary element from the array
void removeTaskFromWaitingQueue(int task)
{
	int i, j;
	for (i = left; i < right; i++)
		if (task == waitingQueue[i])
			break;
	if (i < right)
	{
		// task found - remove from queue
		for (j = i; j < right - 1; j++)
			waitingQueue[j] = waitingQueue[j + 1];
		right--;
	}
	else
	{
		// task not in queue - do nothing
	}
}
// remove the lowest number in the queue
int getLowestNumberTaskFromWaitingQueue()
{
	int task;
	task = waitingQueue[left];
	for (int i = left; i < right; i++)
		if (waitingQueue[i] < task)
			task = waitingQueue[i];
	removeTaskFromWaitingQueue(task);
	return task;
}
void resetWaitingQueue()
{
	left = right = 0;
}
/*End Waiting Queue implementation */


// Used by FIFO algorithm to detect Deadlock
int isDeadLock()
{
	int deadlock = 1; // assume we have deadlock
	int i, state;
	for (i = 1; i <= T; i++)
	{
		state = taskRunFIFO[i].state;
		if (state != Aborted && state != Terminated)
			if (state == Initiating || state == Computing || state == CompleteRequest || state == Releasing)
				deadlock = 0;
	}
	return deadlock;
}

// get the order in which the tasks are processed at each cycle;
// first we check blocked processes that are in the waiting queue
// and then the other process in increasing order
void getCheckOrder()
{
	int j, i = 0, k, ok = 1, s;
	for (j = left; j < right; j++)
		checkOrder[++i] = waitingQueue[j];

	s = i;
	for (k = 1; k <= T; k++)
	{
		ok = 1;
		for (j = 1; j <= i; j++)
			if (checkOrder[j] == k)
				ok = 0;
		if (ok)
			checkOrder[++s] = k;
	}
}

// starts the simulation of the optimistic resource manager
void simultateFIFO()
{
	int cycle = 0;
	int i, j;
	int resourceType = 0;
	int numberRequested = 0;
	int abortedTask;

	taskRunFIFO = (struct Task*)malloc(sizeof(struct Task) * (T + 1));
	if (taskRunFIFO == NULL)
	{
		printf("Not enough memory");
		exit(1);
	}
	for (int i = 1; i <= T; i++)
	{
		taskRunFIFO[i].waitingTime = 0;
		taskRunFIFO[i].timeTaken = 0;
		taskRunFIFO[i].alocatedResources = (int*)malloc(sizeof(int) * (R + 1));
		for (int j = 1; j <= R; j++)
		{
			taskRunFIFO[i].alocatedResources[j] = 0;

		}

	}



	//printf("\tFIFO\n");


	while (cycle == 0 || runningTaskCount(taskRunFIFO) > 0)
	{
		UpdateAvailableResources();
		getCheckOrder();
		//printf("During: %d %d\n", cycle, cycle + 1);
		for (j = 1; j <= T; j++)
		{
			i = checkOrder[j];
			if (taskRunFIFO[i].state != Aborted && taskRunFIFO[i].state != Terminated && taskData[i]->current != NULL)
				switch (taskData[i]->current->data.activity)
				{
				case Initiate:
					taskRunFIFO[i].activity = Initiate;
					taskRunFIFO[i].state = Initiating;
					taskRunFIFO[i].timeTaken++;
					taskData[i]->current = taskData[i]->current->next;
					//printf("\tTask %d initiate\n", i);
					break;
				case Request:
					resourceType = taskData[i]->current->data.n1;
					numberRequested = taskData[i]->current->data.n2;
					if (availableResource[resourceType] >= numberRequested)
					{
						//printf("\tTask %d completes its request\n", i);
						availableResource[resourceType] -= numberRequested;
						taskRunFIFO[i].alocatedResources[resourceType] += numberRequested;
						taskRunFIFO[i].activity = Request;
						taskRunFIFO[i].state = CompleteRequest;
						taskRunFIFO[i].timeTaken++;
						removeTaskFromWaitingQueue(i);
						taskData[i]->current = taskData[i]->current->next;
					}
					else
					{
						//printf("\tTask %d request cannot be granted\n", i);
						if (taskRunFIFO[i].state != Blocked)
							addTaskToWaitingQueue(i);
						taskRunFIFO[i].state = Blocked;
						taskRunFIFO[i].activity = Request;
						taskRunFIFO[i].waitingTime++;
						taskRunFIFO[i].timeTaken++;
					}
					break;
				case Release:
					//printf("\tTask %d release resource\n", i);
					taskRunFIFO[i].activity = Release;
					taskRunFIFO[i].state = Releasing;
					taskRunFIFO[i].timeTaken++;

					releaseResourceBucket[taskData[i]->current->data.n1] = taskData[i]->current->data.n2;
					taskRunFIFO[i].alocatedResources[taskData[i]->current->data.n1] -= taskData[i]->current->data.n2;
					taskData[i]->current = taskData[i]->current->next;
					break;
				case Compute:

					if (taskRunFIFO[i].state != Computing)
					{
						taskRunFIFO[i].state = Computing;
						taskRunFIFO[i].activity = Compute;
						taskRunFIFO[i].cycles = taskData[i]->current->data.n1;
						taskRunFIFO[i].timeTaken++;
						//printf("\tTask %d computing %d of %d\n", i, 
						//	taskData[i]->current->data.n1 - taskRunFIFO[i].cycles + 1,
						//	taskData[i]->current->data.n1);
						if (taskRunFIFO[i].cycles == 1)
							taskData[i]->current = taskData[i]->current->next;
					}
					else
					{
						if (taskRunFIFO[i].cycles > 1)
						{
							taskRunFIFO[i].timeTaken++;
							taskRunFIFO[i].cycles--;
							//printf("\tTask %d computing %d of %d\n", i,
							//	taskData[i]->current->data.n1 - taskRunFIFO[i].cycles + 1,
							//	taskData[i]->current->data.n1);
							if (taskRunFIFO[i].cycles == 1)
								taskData[i]->current = taskData[i]->current->next;
						}

						else // computing ended; go to next activity
							taskData[i]->current = taskData[i]->current->next;
					}

					break;
				case Terminate:
					//printf("\tTask %d terminate\n", i);
					taskRunFIFO[i].activity = Terminate;
					taskRunFIFO[i].state = Terminated;
					taskData[i]->current = taskData[i]->current->next; // now current should be NULL;
					break;

				}
		}

		while (runningTaskCount(taskRunFIFO) > 1 && isDeadLock())
		{
			// print message and abort lowest numbered deadlocked task;
			abortedTask = getLowestNumberTaskFromWaitingQueue();
			taskRunFIFO[abortedTask].state = Aborted;
			//printf("\tTask %d aborted\n", abortedTask);
			// release all resources of the aborted task;
			for (j = 1; j <= R; j++)
			{
				releaseResourceBucket[j] += taskRunFIFO[abortedTask].alocatedResources[j];
				taskRunFIFO[abortedTask].alocatedResources[j] = 0;
			}

		}
		cycle++;
	}

	//printResults(taskRunFIFO);
}

// return 1 if at least one initial claim for resource exceeds total available units for that resource; otherwise return 0
int claimExceedResources(int task)
{
	struct TaskActivityList* p;
	p = taskData[task];
	struct Node* q;

	q = p->first;
	while (q != NULL)
	{
		if (q->data.activity == Initiate && q->data.n2 > totalResource[q->data.n1])
		{
			printf("  Banker aborts task %d before run begins:\n", task);
			printf("       claim for resourse %d (%d) exceeds number of units present (%d)\n",
				q->data.n1, q->data.n2, totalResource[q->data.n1]);
			return 1;
		}

		q = q->next;
	}
	return 0;
}
// starts the simulation of the Banker's algorithm
void setInitialClaim(int i)
{


	struct TaskActivityList* p;
	p = taskData[i];

	struct Node* q;
	q = p->first;
	while (q != NULL)
	{
		if (q->data.activity == Initiate)
			taskRunBANKER[i].initialClaim[q->data.n1] = q->data.n2;
		q = q->next;
	}
}

void printInitialClaim()
{
	for (int i = 1; i <= T; i++)
	{
		printf("Task %d: ", i);
		for (int j = 1; j <= R; j++)
			printf("%d ", taskRunBANKER[i].initialClaim[j]);
		printf("\n");
	}
}


// check if state is safe for one resouce (resourceType)
// this is a flawed implementation that works for inputs 1-8 :)
// NOT USED ; 
int isSafeState(int resourceType)
{
	int maxClaim;
	for (int i = 1; i <= T; i++)
	{
		maxClaim = taskRunBANKER[i].initialClaim[resourceType] - taskRunBANKER[i].alocatedResources[resourceType];
		if (maxClaim <= availableResource[resourceType])
			return 1;
	}
	return 0;
}

// check if state is safe in Banker's
int isSafeStateReloaded()
{
	// create new data structures - vectors and arrays
	int** allocation; // how much resources each procees has currently allocated
	int** need; // how much resources each process can request max - allocation

	int* finish; // state of process (0 / 1; finished / not finished)
	int* work; // available resources working array
	int i, j, index;
	allocation = (int**)malloc(sizeof(int*) * (T + 1));
	need = (int**)malloc(sizeof(int*) * (T + 1));
	for (i = 1; i <= T; i++)
	{
		allocation[i] = (int*)malloc(sizeof(int) * (R + 1));
		need[i] = (int*)malloc(sizeof(int) * (R + 1));
	}



	// create matrix of allocated resources to each process;
	for (i = 1; i <= T; i++)
	{
		for (j = 1; j <= R; j++)
		{
			allocation[i][j] = taskRunBANKER[i].alocatedResources[j];
			need[i][j] = maxDemand[i][j] - allocation[i][j];
		}
	}

	finish = (int*)malloc(sizeof(int) * (T + 1));
	if (finish == NULL)
	{
		printf("Not enough memory");
		exit(1);
	}
	for (i = 1; i <= T; i++)
		if (taskRunBANKER[i].state != Aborted && taskRunBANKER[i].state != Terminated)
			finish[i] = 0;
		else
			finish[i] = 1;


	work = (int*)malloc(sizeof(int) * (R + 1));
	if (work == NULL)
	{
		printf("Not enough memory.");
		exit(1);
	}
	for (i = 1; i <= R; i++)
		work[i] = availableResource[i];



	// cheking if state si safe
	while (1)
	{
		index = -1;
		for (i = 1; i <= T; i++)
		{
			if (finish[i] == 0)
			{
				for (j = 1; j <= R; j++)
					if (need[i][j] > work[j])
						break;
				if (j == R + 1)
				{
					index = i; // found task with all resources met;
					break;
				}
			}
		}
		if (index >= 1)
		{
			finish[index] = 1;
			for (j = 1; j <= R; j++)
			{
				work[j] += allocation[index][j];
				allocation[index][j] = 0;
			}
		}
		else
		{
			break; // no task found whose max request can be met;
		}
	}

	for (i = 1; i <= T; i++)
		if (finish[i] == 0) // at least one task could not finish; not safe state
			return 0;
	return 1; // all tasks finished; safe state; 
}

// starts the simulation of the optimistic resource manager
void simultateBANKER()
{
	int cycle = 0, i, j;
	int resourceType;
	int numberRequested;
	//int remainingResource;
	taskRunBANKER = (struct Task*)malloc(sizeof(struct Task) * (T + 1));
	if (taskRunBANKER == NULL)
	{
		printf("Not enough memory");
		exit(1);
	}
	for (i = 1; i <= T; i++)
	{
		taskRunBANKER[i].waitingTime = 0;
		taskRunBANKER[i].timeTaken = 0;
		taskRunBANKER[i].alocatedResources = (int*)malloc(sizeof(int) * (R + 1));
		taskRunBANKER[i].initialClaim = (int*)malloc(sizeof(int) * (R + 1));
		for (int j = 1; j <= R; j++)
		{
			taskRunBANKER[i].alocatedResources[j] = 0;
		}
		setInitialClaim(i);

	}
	//printInitialClaim();

	for (i = 1; i <= T; i++)
	{
		if (claimExceedResources(i))
		{
			taskRunBANKER[i].state = Aborted;
			//printf("Banker aborts task %d before run begins\n", i);
		}

	}


	//printf("All processes initial claim is OK\n");

	//printf("\tBANKER'S\n");

	while (cycle == 0 || runningTaskCount(taskRunBANKER) > 0)
	{
		UpdateAvailableResources();
		getCheckOrder();
		//printf("During: %d %d\n", cycle, cycle + 1);
		for (j = 1; j <= T; j++)
		{
			i = checkOrder[j];
			if (taskRunBANKER[i].state != Aborted && taskRunBANKER[i].state != Terminated && taskData[i]->current != NULL)
				switch (taskData[i]->current->data.activity)
				{
				case Initiate:
					taskRunBANKER[i].activity = Initiate;
					taskRunBANKER[i].state = Initiating;
					taskRunBANKER[i].timeTaken++;
					taskData[i]->current = taskData[i]->current->next;
					//printf("\tTask %d initiate\n", i);
					break;
				case Request:
					resourceType = taskData[i]->current->data.n1;
					numberRequested = taskData[i]->current->data.n2;


					// check if current request + allocated resource exceeds initial claim; if so abort task and release resources;
					if (numberRequested + taskRunBANKER[i].alocatedResources[resourceType] > taskRunBANKER[i].initialClaim[resourceType])
					{
						taskRunBANKER[i].state = Aborted;
						printf("During cycle %d-%d of Banker's algorithms\n", cycle, cycle + 1);
						printf("   Task %d's request exceeds its claim; aborted; %d units available next cycle\n", i, taskRunBANKER[i].alocatedResources[resourceType]);

						removeTaskFromWaitingQueue(i);
						releaseResourceBucket[resourceType] = taskRunBANKER[i].alocatedResources[resourceType];
						taskRunBANKER[i].alocatedResources[resourceType] = 0;
						break;
					}


					// check if completing the request leads to unsafe state; if so block the task; otherwise complete de request


					taskRunBANKER[i].alocatedResources[resourceType] += numberRequested;
					availableResource[resourceType] -= numberRequested;
					if (isSafeStateReloaded())
					{
						// complete request
						//printf("\tTask %d completes its request\n", i);
						taskRunBANKER[i].activity = Request;
						taskRunBANKER[i].state = CompleteRequest;
						taskRunBANKER[i].timeTaken++;
						removeTaskFromWaitingQueue(i);
						taskData[i]->current = taskData[i]->current->next;
					}
					else
					{
						taskRunBANKER[i].alocatedResources[resourceType] -= numberRequested;
						availableResource[resourceType] += numberRequested;
						// block task; add to waiting queue.
						//printf("\tTask %d request cannot be granted\n", i);
						if (taskRunBANKER[i].state != Blocked)
							addTaskToWaitingQueue(i);
						taskRunBANKER[i].state = Blocked;
						taskRunBANKER[i].activity = Request;
						taskRunBANKER[i].waitingTime++;
						taskRunBANKER[i].timeTaken++;
					}
					break;
				case Release:
					//printf("\tTask %d release resource\n", i);
					taskRunBANKER[i].activity = Release;
					taskRunBANKER[i].state = Releasing;
					taskRunBANKER[i].timeTaken++;

					releaseResourceBucket[taskData[i]->current->data.n1] = taskData[i]->current->data.n2;
					taskRunBANKER[i].alocatedResources[taskData[i]->current->data.n1] -= taskData[i]->current->data.n2;
					taskData[i]->current = taskData[i]->current->next;
					break;
				case Compute:

					if (taskRunBANKER[i].state != Computing)
					{
						taskRunBANKER[i].state = Computing;
						taskRunBANKER[i].activity = Compute;
						taskRunBANKER[i].cycles = taskData[i]->current->data.n1;
						taskRunBANKER[i].timeTaken++;
						//printf("\tTask %d computing %d of %d\n", i,
						//	taskData[i]->current->data.n1 - taskRunBANKER[i].cycles + 1,
						//	taskData[i]->current->data.n1);
						if (taskRunBANKER[i].cycles == 1)
							taskData[i]->current = taskData[i]->current->next;
					}
					else
					{
						if (taskRunBANKER[i].cycles > 1)
						{
							taskRunBANKER[i].timeTaken++;
							taskRunBANKER[i].cycles--;
							//printf("\tTask %d computing %d of %d\n", i,
							//	taskData[i]->current->data.n1 - taskRunBANKER[i].cycles + 1,
							//	taskData[i]->current->data.n1);
							if (taskRunBANKER[i].cycles == 1)
								taskData[i]->current = taskData[i]->current->next;
						}

						else // computing ended; go to next activity
							taskData[i]->current = taskData[i]->current->next;
					}

					break;
				case Terminate:
					//printf("\tTask %d terminate\n", i);
					taskRunBANKER[i].activity = Terminate;
					taskRunBANKER[i].state = Terminated;
					taskData[i]->current = taskData[i]->current->next; // now current should be NULL;
					break;

				}
		}


		cycle++;
	}

	//printResults(taskRunBANKER);
}

// reseting the initial data structures
// used after running FIFO, before runngin BANKER's
void resetReadDataStructure()
{
	int i;
	for (i = 1; i <= T; i++)
		taskData[i]->current = taskData[i]->first;

	for (i = 1; i <= R; i++)
	{
		releaseResourceBucket[i] = 0;
		availableResource[i] = totalResource[i];
	}

	resetWaitingQueue();
}

void printResult2()
{
	printf("              FIFO                        BANKER'S\n");
	int totalWaitingTimeFIFO = 0, totalTimeTakenFIFO = 0;
	int terminatedProcessFIFO = 0;

	int totalWaitingTimeBANK = 0, totalTimeTakenBANK = 0;
	int terminatedProcessBANK = 0;

	for (int i = 1; i <= T; i++)
	{
		printf("     Task %d", i);
		if (taskRunFIFO[i].state == Aborted)
			printf("      aborted   ");
		else
		{
			printf("%7d%4d%4.0lf%%", taskRunFIFO[i].timeTaken, taskRunFIFO[i].waitingTime,
				round((double)taskRunFIFO[i].waitingTime / taskRunFIFO[i].timeTaken * 100));
			totalWaitingTimeFIFO += taskRunFIFO[i].waitingTime;
			totalTimeTakenFIFO += taskRunFIFO[i].timeTaken;
		}


		printf("     Task %d", i);
		if (taskRunBANKER[i].state == Aborted)
			printf("      aborted\n");
		else
		{
			printf("%7d%4d%4.0lf%%\n", taskRunBANKER[i].timeTaken, taskRunBANKER[i].waitingTime,
				round((double)taskRunBANKER[i].waitingTime / taskRunBANKER[i].timeTaken * 100));
			totalWaitingTimeBANK += taskRunBANKER[i].waitingTime;
			totalTimeTakenBANK += taskRunBANKER[i].timeTaken;
		}

	}
	printf("     total%8d%4d%4.0lf%%", totalTimeTakenFIFO, totalWaitingTimeFIFO, round((double)totalWaitingTimeFIFO / totalTimeTakenFIFO * 100));

	printf("     total%8d%4d%4.0lf%%\n", totalTimeTakenBANK, totalWaitingTimeBANK, round((double)totalWaitingTimeBANK / totalTimeTakenBANK * 100));
}
int main(int argc, char* argv[])
{
	if (argc != 2)
	{
		printf("Usage: <program_name> <input_file>");
		return 1;
	}

	
	

	strcpy(inputFile, argv[1]);

	readData(inputFile);

	simultateFIFO();

	resetReadDataStructure();

	simultateBANKER();

	printResult2();
	return 0;
}
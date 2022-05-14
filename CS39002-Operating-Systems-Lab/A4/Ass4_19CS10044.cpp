
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <queue>
#include <stdlib.h>
using namespace std;

#define BACKOFF 0
#define ufactor 1000000
#define mfactor 1000

enum jobstatus
{
	invalid,
	neww,
	producing,
	running,
	completed
};

typedef struct node
{
	int job_id;
	int run_time;
	jobstatus status;
	pthread_mutex_t lock;
	struct node * first_child;
	struct node * parent;
	struct node * next_sib;
	struct node * prev_sib;
	int level;
}
node;

typedef struct tree
{
	int inserted;
	int size;
	int leaf_count;
	node * root;
	int new_count;
	int producing_count;
	int running_count;
}
tree;

int max_tree_size;
sighandler_t def_signal_handler;
int nodes_array_shmid;
int tree_shmid;
int tree_lock_shmid;
pthread_mutex_t * tree_lock;
tree * T;
node * all_nodes;
int cons_proc_id_gbl;

void error(const char *msg)
{
	printf("\n [ ERROR : %s ]\n\n\n", msg);
}

int init_shared_lock()
{
	tree_lock_shmid = shmget(2024, sizeof(pthread_mutex_t), 0666 | IPC_CREAT);
	if (tree_lock_shmid < 0) return -1;

	tree_lock = (pthread_mutex_t*) shmat(tree_lock_shmid, NULL, 0);
	if (tree_lock == (pthread_mutex_t*)(-1))
	{
		error("MUTEX-LOCK in the shared memory could not be attached to process A");
		shmctl(tree_lock_shmid, IPC_RMID, NULL);
		return -1;
	}

	pthread_mutexattr_t mutexattr;
	if (pthread_mutexattr_init(&mutexattr))
	{
		error("Mutex attribute could not be initialized");
		shmdt(tree_lock);
		shmctl(tree_lock_shmid, IPC_RMID, NULL);
		return -1;
	}

	if (pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED))
	{
		error("Mutex attribute could not be set");
		shmdt(tree_lock);
		shmctl(tree_lock_shmid, IPC_RMID, NULL);
		return -1;
	}

	if (pthread_mutex_init(tree_lock, &mutexattr))
	{
		error("Mutex lock could not be initialized");
		shmdt(tree_lock);
		shmctl(tree_lock_shmid, IPC_RMID, NULL);
		return -1;
	}

	shmdt(tree_lock);
	return 0;
}

int get_random_number(int min, int max)
{
	int randNum = rand() % (max - min + 1) + min;
	return randNum;
}

int init_shared_mem()
{
	nodes_array_shmid = shmget(2022, max_tree_size* sizeof(node), IPC_CREAT | 0666);
	if (nodes_array_shmid < 0) return -1;

	node *all_nodes = (node*) shmat(nodes_array_shmid, NULL, 0);
	if (all_nodes == (node*)(-1))
	{
		error("NODES-ARRAY data structure in the shared memory could not be attached to process A");
		shmctl(nodes_array_shmid, IPC_RMID, NULL);
		return -1;
	}

	memset(all_nodes, 0, max_tree_size* sizeof(node));

	///////////

	tree_shmid = shmget(2023, sizeof(tree), IPC_CREAT | 0666);
	if (tree_shmid < 0)
	{
		shmdt(all_nodes);
		shmctl(nodes_array_shmid, IPC_RMID, NULL);
		return -1;
	}

	tree *T = (tree*) shmat(tree_shmid, NULL, 0);
	if (T == (tree*)(-1))
	{
		error("TREE data structure in the shared memory could not be attached to process A");
		shmdt(all_nodes);
		shmctl(nodes_array_shmid, IPC_RMID, NULL);
		shmctl(tree_shmid, IPC_RMID, NULL);
		return -1;
	}

	memset(T, 0, sizeof(tree));

	///////////

	T->size = get_random_number(300, 500);

	for (int i = 0; i < T->size; i++)
	{
		all_nodes[i].status = jobstatus::neww;
		pthread_mutex_init(&all_nodes[i].lock, NULL);
		all_nodes[i].job_id = rand() % 100000000 + 1;
		all_nodes[i].run_time = rand() % 251;
		all_nodes[i].first_child = all_nodes[i].parent = NULL;
		all_nodes[i].next_sib = all_nodes[i].prev_sib = NULL;
	}

	for (int i = T->size; i < max_tree_size; i++)
	{
		all_nodes[i].status = jobstatus::invalid;
		all_nodes[i].first_child = all_nodes[i].parent = NULL;
		all_nodes[i].next_sib = all_nodes[i].prev_sib = NULL;
	}

	T->root = &all_nodes[0];
	T->inserted = T->size;

	int left = T->size - 1;
	int idx = 1;

	queue<node*> bfsq;
	bfsq.push(&all_nodes[0]);

	while (left)
	{
		int child_count = rand() % (left / 3 + 1) + 1;
		left -= child_count;

		node *t = bfsq.front();
		node *p = t;
		bfsq.pop();

		bfsq.push(&all_nodes[idx]);
		p->first_child = &all_nodes[idx++];
		p->first_child->prev_sib = NULL;
		p->first_child->parent = t;

		child_count--;
		p = p->first_child;

		while (child_count--)
		{
			bfsq.push(&all_nodes[idx]);
			all_nodes[idx].parent = t;
			p->next_sib = &all_nodes[idx];
			idx++;
			p->next_sib->prev_sib = p;
			p = p->next_sib;
		}

		p->next_sib = NULL;
	}

	T->leaf_count = bfsq.size();
	T->new_count = T->size;
	T->producing_count = T->running_count = 0;

	printf("\n +++ SHARED MEMORY INITIALIZATION +++ \n");
	printf("   > TREE SIZE : %d\n", T->size);
	printf("   > NO. OF LEAF NODES : %d\n", (int)(bfsq.size()));
	printf("   > MAX TREE SIZE : %d\n", max_tree_size);

	///////////

	shmdt(T);
	shmdt(all_nodes);
	return 0;
}

const char *get_status(int stat)
{
	if (stat == jobstatus::completed) return "COMPLETED";
	if (stat == jobstatus::invalid) return "INVALID";
	if (stat == jobstatus::neww) return "NEW";
	if (stat == jobstatus::running) return "RUNNING";
	if (stat == jobstatus::producing) return "PRODUCING";
	return "BAD_STATUS";
}

void *producer(void *param)
{
	int prod_id = *((int*) param);

	int rand_prod_runtime = (10 *ufactor) + rand() % (10 *ufactor + 1);

	printf("\n[ PRODUCER %2d STARTED ]\n      {      	RUNTIME : %.3f sec }\n ",
		prod_id, 1.0 *rand_prod_runtime / ufactor);

	clock_t prod_clock = clock();
	int usecs_passed;

	while (true)
	{
		usecs_passed = ufactor *(clock() - prod_clock) / CLOCKS_PER_SEC;
		if (usecs_passed >= rand_prod_runtime) break;

		////////////////////////////////////////////

		pthread_mutex_lock(tree_lock);

		if (T->inserted >= max_tree_size || T->size == 0)
		{
			pthread_mutex_unlock(tree_lock);
			break;
		}

		int new_n = 0, idx = 0;
		for (idx = 0; idx < T->inserted; idx++)
			if (all_nodes[idx].status == jobstatus::neww) new_n++;

		if (new_n == 0)
		{
			pthread_mutex_unlock(tree_lock);
			sleep(BACKOFF);
			continue;
		}

		int rand_n = rand() % new_n + 1;
		idx = 0;
		while (rand_n)
			if (all_nodes[idx++].status == jobstatus::neww) rand_n--;

		node *rand_node = &all_nodes[idx - 1];
		pthread_mutex_lock(&rand_node->lock);
		rand_node->status = jobstatus::producing;

		T->new_count--;
		T->producing_count++;

		printf("\n >>>[ PRODUCER ] <<< \n");
		printf(" +++ JOB SELECTED +++ \n");
		printf("   > ACTIVE PRODUCER : %d\n", prod_id);
		printf("   > RANDOMLY SELECTED NODE \n");
		printf("     o JOB ID : %d\n", rand_node->job_id);
		printf("     o RUNTIME : %d msec\n", rand_node->run_time);
		printf("     o STATUS : %s\n", get_status(rand_node->status));
		printf("   > DEPENDENCY TREE PROPERTIES \n");
		printf("     o SIZE : %d\n", T->size);
		printf("     o LEAVES : %d\n", T->leaf_count);
		printf("     o \"NEW\" NODES : %d\n", T->new_count);
		printf("     o \"PRODUCING\" NODES : %d\n", T->producing_count);
		printf("     o \"RUNNING\" NODES : %d\n", T->running_count);

		pthread_mutex_unlock(tree_lock);

		////////////////////////////////////////////

		usecs_passed = ufactor *(clock() - prod_clock) / CLOCKS_PER_SEC;
		if (usecs_passed >= rand_prod_runtime)
		{
			pthread_mutex_lock(tree_lock);
			T->new_count++;
			T->producing_count--;
			rand_node->status = jobstatus::neww;
			pthread_mutex_unlock(tree_lock);
			pthread_mutex_unlock(&rand_node->lock);
			break;
		}

		////////////////////////////////////////////

		pthread_mutex_lock(tree_lock);

		T->new_count++;
		T->producing_count--;

		rand_node->status = jobstatus::neww;

		node *old_first = rand_node->first_child;

		all_nodes[T->inserted].status = jobstatus::neww;
		T->new_count++;
		all_nodes[T->inserted].job_id = rand() % 100000000 + 1;
		all_nodes[T->inserted].run_time = rand() % 251;
		pthread_mutex_init(&all_nodes[T->inserted].lock, NULL);

		rand_node->first_child = &all_nodes[T->inserted];
		rand_node->first_child->prev_sib = NULL;
		rand_node->first_child->next_sib = old_first;
		if (old_first)
		{
			old_first->prev_sib = rand_node->first_child;
			T->leaf_count++;
		}
		rand_node->first_child->parent = rand_node;
		rand_node->first_child->first_child = NULL;

		T->size++;

		printf("\n >>>[ PRODUCER ] <<< \n");
		printf(" +++ JOB INSERTED +++ \n");
		printf("   > ACTIVE PRODUCER : %d\n", prod_id);
		printf("   > RANDOMLY SELECTED NODE \n");
		printf("     o JOB ID : %d\n", rand_node->job_id);
		printf("     o STATUS : %s\n", get_status(rand_node->status));
		printf("   > INSERTED NODE \n");
		printf("     o JOB ID : %d\n", all_nodes[T->inserted].job_id);
		printf("     o RUNTIME : %d msec\n", all_nodes[T->inserted].run_time);
		printf("     o STATUS : %s\n", get_status(all_nodes[T->inserted].status));
		printf("   > DEPENDENCY TREE PROPERTIES \n");
		printf("     o SIZE : %d\n", T->size);
		printf("     o LEAVES : %d\n", T->leaf_count);
		printf("     o \"NEW\" NODES : %d\n", T->new_count);
		printf("     o \"PRODUCING\" NODES : %d\n", T->producing_count);
		printf("     o \"RUNNING\" NODES : %d\n", T->running_count);

		T->inserted++;

		pthread_mutex_unlock(tree_lock);
		pthread_mutex_unlock(&rand_node->lock);

		////////////////////////////////////////////

		usecs_passed = ufactor *(clock() - prod_clock) / CLOCKS_PER_SEC;
		if (usecs_passed >= rand_prod_runtime) break;

		int usecs_left = rand_prod_runtime - usecs_passed;
		int rand_wait_usecs = get_random_number(200 *mfactor, 500 *mfactor);
		if (rand_wait_usecs > usecs_left) rand_wait_usecs = usecs_left;

		usleep(rand_wait_usecs);
	}

	printf("\n [ PRODUCER %2d TERMINATED ]\n ", prod_id);
	pthread_exit(NULL);
}

void *consumer(void *param)
{
	int cons_id = *((int*) param);
	printf("\n[ CONSUMER %2d STARTED ]\n", cons_id);

	while (true)
	{

		////////////////////////////////////////////

		pthread_mutex_lock(tree_lock);
		if (T->size == 0)
		{
			pthread_mutex_unlock(tree_lock);
			break;
		}

		int idx = 0;
		for (idx = 0; idx < T->inserted; idx++)
			if (all_nodes[idx].status == jobstatus::neww &&
				!all_nodes[idx].first_child) break;

		if (idx == T->inserted)
		{
			pthread_mutex_unlock(tree_lock);
			sleep(BACKOFF);
			continue;
		}

		node *rand_node = &all_nodes[idx];

		pthread_mutex_lock(&rand_node->lock);
		rand_node->status = jobstatus::running;

		T->new_count--;
		T->running_count++;

		printf("\n >>>[ CONSUMER ] <<< \n");
		printf(" +++ JOB STARTED +++ \n");
		printf("   > ACTIVE CONSUMER : %d\n", cons_id);
		printf("   > RANDOMLY SELECTED JOB \n");
		printf("     o JOB ID : %d\n", rand_node->job_id);
		printf("     o RUNTIME : %d msec\n", rand_node->run_time);
		printf("     o STATUS : %s\n", get_status(jobstatus::running));
		printf("   > DEPENDENCY TREE PROPERTIES \n");
		printf("     o SIZE : %d\n", T->size);
		printf("     o LEAVES : %d\n", T->leaf_count);
		printf("     o \"NEW\" NODES : %d\n", T->new_count);
		printf("     o \"PRODUCING\" NODES : %d\n", T->producing_count);
		printf("     o \"RUNNING\" NODES : %d\n", T->running_count);

		pthread_mutex_unlock(tree_lock);

		////////////////////////////////////////////

		usleep(1000 *rand_node->run_time);

		////////////////////////////////////////////

		pthread_mutex_lock(tree_lock);

		rand_node->status = jobstatus::completed;
		T->running_count--;

		T->size--;
		T->leaf_count--;
		if (T->size == 0)
		{
			T->root = NULL;
		}
		else
		{
			if (!rand_node->prev_sib)
			{
				if (!rand_node->next_sib)
				{
					if (rand_node->parent)
					{
						T->leaf_count++;
						rand_node->parent->first_child = NULL;
					}
				}
				else
				{
					if (rand_node->parent) 
                        rand_node->parent->first_child = rand_node->next_sib;
					rand_node->next_sib->prev_sib = NULL;
				}
			}
			else
			{
				node *prev = rand_node->prev_sib;
				node *next = rand_node->next_sib;
				prev->next_sib = next;
				if (next) next->prev_sib = prev;
			}
		}

		printf("\n >>>[ CONSUMER ] <<< \n");
		printf(" +++ JOB FINISHED +++ \n");
		printf("   > ACTIVE CONSUMER : %d\n", cons_id);
		printf("   > JOB PROPERTIES \n");
		printf("     o JOB ID : %d\n", rand_node->job_id);
		printf("     o STATUS : %s\n", get_status(rand_node->status));
		printf("   > DEPENDENCY TREE PROPERTIES \n");
		printf("     o SIZE : %d\n", T->size);
		printf("     o LEAVES : %d\n", T->leaf_count);
		printf("     o \"NEW\" NODES : %d\n", T->new_count);
		printf("     o \"PRODUCING\" NODES : %d\n", T->producing_count);
		printf("     o \"RUNNING\" NODES : %d\n", T->running_count);

		pthread_mutex_unlock(tree_lock);
		pthread_mutex_unlock(&rand_node->lock);

		////////////////////////////////////////////
	}

	printf("\n [ CONSUMER %2d TERMINATED ]\n ", cons_id);
	pthread_exit(NULL);
}

void clean_and_exit(int sig)
{
	shmctl(tree_shmid, IPC_RMID, NULL);
	shmctl(nodes_array_shmid, IPC_RMID, NULL);
	shmctl(tree_lock_shmid, IPC_RMID, NULL);
	kill(cons_proc_id_gbl, SIGKILL);
	signal(SIGINT, def_signal_handler);
	raise(SIGINT);
}

int take_input(int *P, int *y)
{
	printf("\n +++ PARAMETERS +++ \n");
	printf("   > NO. OF PRODUCER THREADS (P) : ");
	scanf("%d", P);
	printf("   > NO. OF CONSUMER THREADS (y) : ");
	scanf("%d", y);
	if ((*P) < 1 || (*y) < 1) return -1;
	return 0;
}

void init_env()
{
	printf("\n\n");
	def_signal_handler = signal(SIGINT, clean_and_exit);
	srand(time(0));
}

int main()
{
	init_env();

	int P, y;
	if (take_input(&P, &y) < 0)
	{
		error("Values of both P and y must be positive");
		exit(-1);
	}

	max_tree_size = 500 + 101 * P;
	if (init_shared_mem() < 0)
	{
		error("Error in initializing shared memory for the data structures");
		exit(-1);
	}

	if (init_shared_lock() < 0)
	{
		error("Error in initializing shared memory for the mutex lock");
		shmctl(tree_shmid, IPC_RMID, NULL);
		shmctl(nodes_array_shmid, IPC_RMID, NULL);
		exit(-1);
	}

	////////////

	T = (tree*) shmat(tree_shmid, NULL, 0);
	if (T == (tree*)(-1))
	{
		error("TREE data structure in the shared-memory could not be attached to process A");
		shmctl(tree_shmid, IPC_RMID, NULL);
		shmctl(nodes_array_shmid, IPC_RMID, NULL);
		exit(-1);
	}

	all_nodes = (node*) shmat(nodes_array_shmid, NULL, 0);
	if (all_nodes == (node*)(-1))
	{
		error("NODES-ARRAY data structure in the shared-memory could not be attached to process A");
		shmdt(T);
		shmctl(tree_shmid, IPC_RMID, NULL);
		shmctl(nodes_array_shmid, IPC_RMID, NULL);
		exit(-1);
	}

	tree_lock = (pthread_mutex_t*) shmat(tree_lock_shmid, NULL, 0);
	if (tree_lock == (pthread_mutex_t*)(-1))
	{
		error("MUTEX-LOCK in the shared-memory could not be attached to process A");
		shmdt(T);
		shmctl(tree_shmid, IPC_RMID, NULL);
		shmdt(all_nodes);
		shmctl(nodes_array_shmid, IPC_RMID, NULL);
		shmctl(tree_lock_shmid, IPC_RMID, NULL);
		exit(-1);
	}

	////////////

	pthread_t *producer_thread_id = (pthread_t*) calloc(P, sizeof(pthread_t));
	int *prod_ids = (int*) calloc(P, sizeof(int));

	if (!prod_ids || !producer_thread_id)
	{
		if (prod_ids) free(prod_ids);
		if (producer_thread_id) free(producer_thread_id);
		error("calloc() returned an error in process A");
		shmdt(T);
		shmdt(all_nodes);
		shmdt(tree_lock);
		shmctl(tree_shmid, IPC_RMID, NULL);
		shmctl(nodes_array_shmid, IPC_RMID, NULL);
		shmctl(tree_lock_shmid, IPC_RMID, NULL);
		exit(-1);
	}

	////////////

	for (int i = 0; i < P; i++)
	{
		prod_ids[i] = i + 1;
		pthread_create(&producer_thread_id[i], NULL, producer, &prod_ids[i]);
	}

	////////////

	int cons_proc_id = fork();
	if (cons_proc_id < 0)
	{
		error("Parent process A could not fork consumer process B");
		shmdt(T);
		shmdt(all_nodes);
		shmdt(tree_lock);
		shmctl(tree_shmid, IPC_RMID, NULL);
		shmctl(nodes_array_shmid, IPC_RMID, NULL);
		shmctl(tree_lock_shmid, IPC_RMID, NULL);
		exit(-1);
	}

	////////////

	if (cons_proc_id == 0)
	{
		T = (tree*) shmat(tree_shmid, NULL, 0);
		if (T == (tree*)(-1))
		{
			error("TREE data structure in the shared-memory could not be attached to process B");
			exit(-1);
		}

		all_nodes = (node*) shmat(nodes_array_shmid, NULL, 0);
		if (T == (tree*)(-1))
		{
			error("NODES-ARRAY data structure in the shared-memory could not be attached to process B");
			shmdt(T);
			exit(-1);
		}

		tree_lock = (pthread_mutex_t*) shmat(tree_lock_shmid, NULL, 0);
		if (tree_lock == (pthread_mutex_t*)(-1))
		{
			error("MUTEX-LOCK in the shared-memory could not be attached to process B");
			shmdt(T);
			shmdt(all_nodes);
			exit(-1);
		}

		pthread_t *consumer_thread_id = (pthread_t*) calloc(y, sizeof(pthread_t));
		int *cons_ids = (int*) calloc(y, sizeof(int));
		if (!cons_ids || !consumer_thread_id)
		{
			if (cons_ids) free(cons_ids);
			if (consumer_thread_id) free(consumer_thread_id);
			error("calloc() returned an error in process B");
			shmdt(T);
			shmdt(all_nodes);
			exit(-1);
		}

		for (int i = 0; i < y; i++)
		{
			cons_ids[i] = i + 1;
			pthread_create(&consumer_thread_id[i], NULL, consumer, &cons_ids[i]);
		}

		for (int i = 0; i < y; i++)
			pthread_join(consumer_thread_id[i], NULL);

		free(consumer_thread_id);
		free(cons_ids);
		shmdt(T);
		shmdt(all_nodes);
		shmdt(tree_lock);
		exit(0);
	}
	cons_proc_id_gbl = cons_proc_id;

	////////////

	for (int i = 0; i < P; i++)
		pthread_join(producer_thread_id[i], NULL);

	////////////

	int cons_ret_stat;
	wait(&cons_ret_stat);
	if (cons_ret_stat < 0)
		error("Consumer process returned an error");

	////////////

	free(producer_thread_id);
	free(prod_ids);
	shmdt(T);
	shmdt(all_nodes);
	shmdt(tree_lock);

	shmctl(tree_shmid, IPC_RMID, NULL);
	shmctl(nodes_array_shmid, IPC_RMID, NULL);
	shmctl(tree_lock_shmid, IPC_RMID, NULL);

	printf("\n\n");

	return 0;
}

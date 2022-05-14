#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/wait.h>

void *mult(void*);
int check_positive_integer(int);
void get_user_input(int *, int *, int *, int *);
unsigned int sizeof_dm(int, int, size_t);
void create_memory_segment(int *, const int, const int);
void attach_memory_segment(const int, double **, double ***, const int, const int);
void detach_memory_segment(void*);
void destroy_memory_segment(int);
void fill_up_matrix(double ***, const int, const int);
void print_matrix(double **, const int, const int);

// A[r1][c1] and B[r2][c2] 
// c1 = r2
// C[r1][c2]
// compute A[i,:] *B[:,j] and store in C[i,j]
// veclen: c1 / r2
// i, j: cell to compute
typedef struct _process_data
{
	double **A;
	double **B;
	double **C;
	int veclen, i, j;
}
ProcessData;

void *mult(void *arg)
{
	ProcessData *PD = (ProcessData*) arg;

	double **A = PD->A;
	double **B = PD->B;
	double **C = PD->C;

	int r = PD->i;
	int c = PD->j;

	C[r][c] = 0;

	for (int i = 0; i < PD->veclen; i++)
	{
		C[r][c] += A[r][i] *B[i][c];
	}

	return NULL;
}

int check_positive_integer(int num)
{
	if (num > 0)
	{
		return 1;
	}
	return 0;
}

void get_user_input(int *r1, int *c1, int *r2, int *c2)
{
	int done = 0;
	printf(" Enter the number of rows and columns in Matrix A ---\n");
	while (!done)
	{
		printf(" Enter the number of rows r1: ");
		scanf(" %d", r1);
		if (!check_positive_integer(*r1))
		{
			printf(" [ INVALID INPUT. Please try again. ]\n");
			fflush(stdin);
			continue;
		}
		done = 1;
	}

	fflush(stdin);

	done = 0;
	while (!done)
	{
		printf(" Enter the number of columns c1: ");
		scanf(" %d", c1);
		if (!check_positive_integer(*c1))
		{
			printf(" [ INVALID INPUT. Please try again. ]\n");
			fflush(stdin);
			continue;
		}
		done = 1;
	}

	printf(" ============================================\n");
	printf(" Number of columns in A = Number of rows in B\n");
	*r2 = *c1;
	printf(" Hence r2 = c1 = %d\n", *c1);
	printf(" ============================================\n");

	fflush(stdin);

	printf(" Enter the number of columns in Matrix B ---\n");
	done = 0;
	while (!done)
	{
		printf(" Enter the number of columns c2: ");
		scanf(" %d", c2);
		if (!check_positive_integer(*c2))
		{
			printf(" [ INVALID INPUT. Please try again. ]\n");
			fflush(stdin);
			continue;
		}
		done = 1;
	}

	fflush(stdin);
}

unsigned int sizeof_dm(int rows, int cols, size_t sizeElement)
{
	size_t size = rows *(sizeof(void*) + (cols *sizeElement));
	return size;
}

void create_memory_segment(int *shmid, const int r, const int c)
{
	// Create the shared memory segment
	size_t size = sizeof_dm(r, c, sizeof(double));

	// The key can also be IPC_PRIVATE, means, running processes as server 
	// and client (parent and child relationship) i.e., inter-related process 
	// communiation. If the client wants to use shared memory with this key, 
	// then it must be a child process of the server. Also, the child process 
	// needs to be created after the parent has obtained a shared memory.
	(*shmid) = shmget(IPC_PRIVATE, size, IPC_CREAT | 0644);

	if ((*shmid) < 0)
	{
		perror("Shared Memory Creation Failed: ");
		exit(-1);
	}
}

void attach_memory_segment(const int shmid, double **shmp, double ***M, const int r, const int c)
{
	// Attach to the segment to get a pointer to it.
	*shmp = (double*) shmat(shmid, NULL, 0);
	if ((*shmp) == (double*)(-1))
	{
		printf(" Shared memory could not be attached.\n");
		shmctl(shmid, IPC_RMID, NULL);
		exit(-1);
	}

	*M = malloc(r* sizeof((*M)[0]));
	if ((*M) == NULL)
	{
		printf(" Not enough memory.\n");
		shmdt(*shmp);
		shmctl(shmid, IPC_RMID, NULL);
		exit(-1);
	}

	for (int i = 0; i < r; i++)
	{
		(*M)[i] = (*shmp) + i * c;
	}
}

void detach_memory_segment(void *shmp)
{
	if (shmdt(shmp) == -1)
	{
		perror("Error while detaching shared memory: ");
		exit(-1);
	}
}

void destroy_memory_segment(int shmid)
{
	if (shmctl(shmid, IPC_RMID, 0) == -1)
	{
		perror("Error while marking shared memory for destruction: ");
		exit(-1);
	}
}

void fill_up_matrix(double ***M, const int r, const int c)
{
	for (int i = 0; i < r; i++)
	{
		for (int j = 0; j < c; j++)
		{
			scanf(" %lf", &((*M)[i][j]));
		}
	}

	fflush(stdin);
}

void print_matrix(double **M, const int r, const int c)
{
	for (int i = 0; i < r; i++)
	{
		for (int j = 0; j < c; j++)
		{
			printf(" %10.6lf", M[i][j]);
		}
		printf("\n");
	}
}

int main()
{
	printf("\n   +++ MATRIX MULTIPLICATION USING SHARED MEMORY +++\n\n");
	int r1, c1, r2, c2;
	get_user_input(&r1, &c1, &r2, &c2);

	// Print Info about the Matrices
	printf(" ============================================\n");
	printf(" Matrix A has dimensions: %d x %d\n", r1, c1);
	printf(" Matrix B has dimensions: %d x %d\n", r2, c2);
	printf(" ============================================\n");

	int shmid1, shmid2, shmid3;
	create_memory_segment(&shmid1, r1, c1);
	create_memory_segment(&shmid2, r2, c2);
	create_memory_segment(&shmid3, r1, c2);

	double *shmp1, *shmp2, *shmp3;
	double **A, **B, **C;
	attach_memory_segment(shmid1, &shmp1, &A, r1, c1);
	attach_memory_segment(shmid2, &shmp2, &B, r2, c2);
	attach_memory_segment(shmid3, &shmp3, &C, r1, c2);

	// Fill up the matrices A and B
	printf(" Enter the data into matrix A (row major fashion): \n");
	fill_up_matrix(&A, r1, c1);
	printf(" ============================================\n");
	printf(" Enter the data into matrix B (row major fashion): \n");
	fill_up_matrix(&B, r2, c2);

	// Print the matrices
	printf(" ============================================\n");
	printf(" MATRIX A\n");
	print_matrix(A, r1, c1);
	printf(" ============================================\n");
	printf(" MATRIX B\n");
	print_matrix(B, r2, c2);
	printf(" ============================================\n");

	ProcessData PD = { A, B, C, c1, 0, 0 };

	for (int i = 0; i < r1; i++)
	{
		for (int j = 0; j < c2; j++)
		{
			PD.i = i;
			PD.j = j;
			if (fork() == 0)
			{
				mult(&PD);
				exit(0);
			}
		}
	}

	int status;
	// need to wait for all children to finish, otherwise 
	// (1.) computation may not have been completed or 
	// (2.) may lead to zombie state for some children
	for (int i = 0; i < r1; i++)
		for (int j = 0; j < c2; j++)
			wait(&status);

	printf(" ============================================\n");
	printf(" MATRIX C\n");
	print_matrix(C, r1, c2);
	printf(" ============================================\n\n");

	free(A);
	free(B);
	free(C);

	detach_memory_segment(shmp1);
	detach_memory_segment(shmp2);
	detach_memory_segment(shmp3);

	destroy_memory_segment(shmid1);
	destroy_memory_segment(shmid2);
	destroy_memory_segment(shmid3);

	return 0;
}
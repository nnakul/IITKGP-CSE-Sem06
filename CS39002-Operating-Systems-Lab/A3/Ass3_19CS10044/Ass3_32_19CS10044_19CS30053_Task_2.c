
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <pthread.h>
#include <sys/types.h>
#include <time.h>
#include <sys/ipc.h>
#include <errno.h>
#define __wait__ 0
#define __new__ -1
#define __invalid__ -2
#define true 1
#define false 0
#define DIM     1000
#define HDIM    500
#define QSIZE   8
#define MAXID   100000
#define BACKOFF  0.0

int lockid ;
pthread_mutex_t * lock ;

typedef struct _job_ {
    int _prod_num ;
    int _mult_stat ;
    double _mat[DIM][DIM] ;
    int _mat_id ;
} job ;

typedef struct _buffer_ {
    int _size ;
    job _jobs[QSIZE] ;
    int _in ;
    int _out ;
    int _count ;
    int _upd ;
} buffer ;

typedef struct _segment_ {
    buffer _buf ;
    int _pending_jobs ;
    int _job_created ;
} segment ;

double RndFlt ( ) {
    double r = 1.0 * rand() / RAND_MAX ;
    return 18 * r - 9 ;
}

void InitMem ( segment * shm ) {
    shm->_job_created = 0 ;
    buffer * buf = &(shm->_buf) ;
    buf->_size = QSIZE ;
    buf->_in = buf->_out = 0 ;
    buf->_count = 0 ;
    buf->_upd = -1 ;
    
    int len = buf->_size ;
    for ( int i = 0 ; i < len ; i ++ )
        buf->_jobs[i]._mult_stat = __invalid__ ;
}

void GenJob ( job * jj , int prod_id ) {
    jj->_prod_num = prod_id ;
    for ( int i = 0 ; i < DIM ; i ++ )
        for ( int j = 0 ; j < DIM ; j ++ )
            jj->_mat[i][j] = RndFlt() ;
    
    jj->_mat_id = rand() % MAXID + 1 ;
    jj->_mult_stat = __new__ ;
}

void JobCopy ( job * dj , const job * sj ) {
    dj->_mat_id = sj->_mat_id ;
    dj->_mult_stat = __wait__ ;
    dj->_prod_num = sj->_prod_num ;
    for ( int i = 0 ; i < DIM ; i ++ )
        for ( int j = 0 ; j < DIM ; j ++ )
            dj->_mat[i][j] = sj->_mat[i][j] ;
}

int RandWaitTime ( double max_secs ) {
    int t = (int)(max_secs * 1000000) ;
    return rand() % (t + 1) ;
}

int InsertJob ( segment * shm , const job * jj ) {
    buffer * buf = &(shm->_buf) ;
    
    pthread_mutex_lock(lock) ;
    if ( shm->_pending_jobs == 0 && !pthread_mutex_unlock(lock) )  return -1 ;
    if ( buf->_count == buf->_size && !pthread_mutex_unlock(lock) )  return 0 ;

    JobCopy(&(buf->_jobs[buf->_in]), jj) ;
    buf->_in = (1 + buf->_in) % buf->_size ;
    (buf->_count) ++ ;
    (shm->_job_created) ++ ;
    (shm->_pending_jobs) -- ;

    printf("\n +++ JOB INSERTED (PRODUCER) +++\n") ;
    printf("  PRODUCER NO. : %d\n", jj->_prod_num) ;
    printf("  PROCESS ID : %d\n", getpid()) ;
    printf("  MATRIX ID : %d\n", jj->_mat_id) ;
    printf("  MULTIP. STATUS : %d\n", __wait__) ;
    pthread_mutex_unlock(lock) ;

    return 1 ;
}

void SetMatZero ( double mat[][DIM] ) {
    for ( int i = 0 ; i < DIM ; i ++ )
        for ( int j = 0 ; j < DIM ; j ++ )
            mat[i][j] = 0.0 ;
}

void SetHalfMatZero ( double mat[][HDIM] ) {
    for ( int i = 0 ; i < HDIM ; i ++ )
        for ( int j = 0 ; j < HDIM ; j ++ )
            mat[i][j] = 0.0 ;
}

int Work ( segment * shm , int wid ) {
    buffer * buf = &(shm->_buf) ;

    pthread_mutex_lock(lock) ;
    if ( buf->_count < 2 && !pthread_mutex_unlock(lock) ) return 0 ;

    int out1 = buf->_out ;
    int out2 = (buf->_out + 1) % buf->_size ;
    job * job1 = &(buf->_jobs[out1]) ;
    job * job2 = &(buf->_jobs[out2]) ;

    int stat1 = job1->_mult_stat ;
    int stat2 = job2->_mult_stat ;

    int status_mismatch = (stat1 != stat2) ;
    int status_unfit = (stat1 < 0 || stat1 > 7) ;
    int mult_in_process = (stat1 == 0 && buf->_upd != -1) ;

    int ret = (status_mismatch || status_unfit || mult_in_process) ;
    if ( ret && !pthread_mutex_unlock(lock) )   return 0 ;

    const int status = stat1 ;

    const int binj = status % 2 ;
    const int bink = (status / 2) % 2 ;
    const int bini = (status / 4) % 2 ;
    const char blkA[] = { 'A' , '_' , '0' + bini , '0' + bink , 0 } ;
    const char blkB[] = { 'B' , '_' , '0' + bink , '0' + binj , 0 } ;
    const char blkD[] = { 'D' , '_' , '0' + bini , '0' + binj , '0' + bink , 0 } ;
    
    printf("\n +++ WORKER FETCHING +++\n") ;
    printf("  ACTIVE WORKER NO. : %d\n", wid) ;
    printf("  PROCESS ID : %d\n", getpid()) ;
    printf("  BLOCK (COMPUTING) : %s = %s x %s\n", blkD, blkA, blkB) ;
    printf("  [ JOB 01 ]\n") ;
    printf("     > PRODUCER NO. : %d\n", job1->_prod_num) ;
    printf("     > MATRIX ID : %d\n", job1->_mat_id) ;
    printf("     > MULTIP. STATUS : %d\n", job1->_mult_stat) ;
    printf("     > BLOCK (USING) : %s\n", blkA) ;
    printf("  [ JOB 02 ]\n") ;
    printf("     > PRODUCER NO. : %d\n", job2->_prod_num) ;
    printf("     > MATRIX ID : %d\n", job2->_mat_id) ;
    printf("     > MULTIP. STATUS : %d\n", job2->_mult_stat) ;
    printf("     > BLOCK (USING) : %s\n", blkB) ;
    printf("  [ SHARED MEMORY STATE ]\n") ;
    printf("     > BUFFER SIZE : %d\n", buf->_count) ;
    printf("     > TOTAL JOBS CREATED : %d\n", shm->_job_created) ;

    double local_mat[HDIM][HDIM] ;
    SetHalfMatZero(local_mat) ;
    for ( int i = 0 ; i < HDIM ; i ++ )
        for ( int j = 0 ; j < HDIM ; j ++ )
            for ( int k = 0 ; k < HDIM ; k ++ ) {
                if ( status == 0 )  local_mat[i][j] += job1->_mat[i][k] * job2->_mat[k][j] ;
                else if ( status == 1 )  local_mat[i][j] += job1->_mat[i][k] * job2->_mat[k][j+HDIM] ;
                else if ( status == 2 )  local_mat[i][j] += job1->_mat[i][k+HDIM] * job2->_mat[k+HDIM][j] ;
                else if ( status == 3 )  local_mat[i][j] += job1->_mat[i][k+HDIM] * job2->_mat[k+HDIM][j+HDIM] ;
                else if ( status == 4 )  local_mat[i][j] += job1->_mat[i+HDIM][k] * job2->_mat[k][j] ;
                else if ( status == 5 )  local_mat[i][j] += job1->_mat[i+HDIM][k] * job2->_mat[k][j+HDIM] ;
                else if ( status == 6 )  local_mat[i][j] += job1->_mat[i+HDIM][k+HDIM] * job2->_mat[k+HDIM][j] ;
                else if ( status == 7 )  local_mat[i][j] += job1->_mat[i+HDIM][k+HDIM] * job2->_mat[k+HDIM][j+HDIM] ;
            }

    if ( status == 7 ) {
        job1->_mult_stat = __invalid__ ;
        job2->_mult_stat = __invalid__ ;
        buf->_out = (buf->_out + 2) % buf->_size ;
        (buf->_count) -= 2 ;
    }
    else {
        (job1->_mult_stat) ++ ;
        (job2->_mult_stat) ++ ;
    }

    pthread_mutex_unlock(lock) ;



    pthread_mutex_lock(lock) ;

    job * target ;
    if ( buf->_upd == -1 ) {
        while ( buf->_count == buf->_size ) usleep(RandWaitTime(BACKOFF)) ;

        buf->_upd = buf->_in ;
        buf->_in = (1 + buf->_in) % buf->_size ;
        (buf->_count) ++ ;
        (shm->_job_created) ++ ;

        target = &(buf->_jobs[buf->_upd]) ;
        target->_mat_id = rand() % MAXID + 1 ;
        target->_mult_stat = 100 ;
        target->_prod_num = -1 * wid ;
        SetMatZero(target->_mat) ;

        printf("\n +++ JOB INSERTED (WORKER) +++\n") ;
        printf("  WORKER NO. : %d\n", wid) ;
        printf("  PROCESS ID : %d\n", getpid()) ;
        printf("  MATRIX ID : %d\n", target->_mat_id) ;
        printf("  MULTIP. STATUS : 100\n") ;
    }
    else {
        target = &(buf->_jobs[buf->_upd]) ;
    }

    pthread_mutex_unlock(lock) ;



    pthread_mutex_lock(lock) ;

    printf("\n +++ WORKER UPDATING +++\n") ;
    printf("  ACTIVE WORKER NO. : %d\n", wid) ;
    printf("  PROCESS ID : %d\n", getpid()) ;
    printf("  BLOCK (UPDATING) : C_%d%d\n", bini, binj) ;
    printf("  BLOCK (ADDING) : %s\n", blkD) ;
    printf("  [ TARGET JOB STATE ]\n") ;
    printf("     > BUFFER POSITION : %d\n", buf->_upd) ;
    printf("     > PRODUCER NO. : %d\n", target->_prod_num) ;
    printf("     > MATRIX ID : %d\n", target->_mat_id) ;
    printf("     > MULTIP. STATUS : %d\n", target->_mult_stat) ;
    printf("  [ SHARED MEMORY STATE ]\n") ;
    printf("     > BUFFER SIZE : %d\n", buf->_count) ;
    printf("     > TOTAL JOBS CREATED : %d\n", shm->_job_created) ;

    for ( int i = 0 ; i < HDIM ; i ++ )
        for ( int j = 0 ; j < HDIM ; j ++ ) {
                if ( status == 0 || status == 2 )       target->_mat[i][j] += local_mat[i][j] ;
                else if ( status == 1 || status == 3 )  target->_mat[i][j+HDIM] += local_mat[i][j] ;
                else if ( status == 4 || status == 6 )  target->_mat[i+HDIM][j] += local_mat[i][j] ;
                else if ( status == 5 || status == 7 )  target->_mat[i+HDIM][j+HDIM] += local_mat[i][j] ;
            }
    
    if ( target->_mult_stat == 107 ) {
        target->_mult_stat = __wait__ ;
        buf->_upd = -1 ;
    }
    else    (target->_mult_stat) ++ ;
    
    pthread_mutex_unlock(lock) ;
    return 0 ;
}

void Producer ( int shmid , int id ) {
    srand(getpid()) ;

    segment * SHM = (segment *)shmat(shmid, NULL, 0) ;
    if ( SHM == (segment *)(-1) ) {
        printf(" ERROR : Producer %d could not access shared memory segment.\n\n", id) ;
        exit(-1) ;
    }
    
    lock = (pthread_mutex_t *)shmat(lockid, NULL, 0) ;
    if ( lock == (pthread_mutex_t *)(-1) ) {
        printf(" ERROR : Producer %d could not access shared memory segment.\n\n", id) ;
        shmdt(SHM) ;
        exit(-1) ;
    }

    while ( true ) {
        job jj ;
        GenJob(&jj, id) ;
        usleep(RandWaitTime(3.0)) ;
        
        int produced = 0 ;
        while ( ! (produced = InsertJob(SHM, &jj)) )
            usleep(RandWaitTime(BACKOFF)) ;
        
        if ( produced == -1 ) break ;
    }

    if ( shmdt(SHM) == -1 )  {
		printf(" ERROR (%d) : Shared memory could not be detatched.\n\n", errno) ;
        shmdt(lock) ;
		exit(-1) ;
	}

    if ( shmdt(lock) == -1 )  {
		printf(" ERROR (%d) : Shared memory could not be detatched.\n\n", errno) ;
        exit(-1) ;
	}
    
    exit(0) ;
}

void Worker ( int shmid , int id ) {
    srand(getpid()) ;

    segment * SHM = (segment *)shmat(shmid, NULL, 0) ;
    if ( SHM == (segment *)(-1) ) {
        printf(" ERROR : Worker %d could not access shared memory segment.\n\n", id) ;
        exit(-1) ;
    }
    
    lock = (pthread_mutex_t *)shmat(lockid, NULL, 0) ;
    if ( lock == (pthread_mutex_t *)(-1) ) {
        printf(" ERROR : Worker %d could not access shared memory segment.\n\n", id) ;
        shmdt(SHM) ;
        exit(-1) ;
    }

    while ( true ) {
        usleep(RandWaitTime(3.0)) ;
        Work(SHM, id) ;
    }

    if ( shmdt(SHM) == -1 )  {
		printf(" ERROR (%d) : Shared memory could not be detatched.\n\n", errno) ;
        shmdt(lock) ;
		exit(-1) ;
	}

    if ( shmdt(lock) == -1 )  {
		printf(" ERROR (%d) : Shared memory could not be detatched.\n\n", errno) ;
        exit(-1) ;
	}

    exit(0) ;
}

int main ( ) {
    int np, nw, nm ;
    printf("\n Enter no. of producers (NP) : ") ;
    scanf("%d", &np) ;
    if ( np < 1 ) {
        printf(" ERROR : Number of producers must be positive.\n\n") ;
        return 0 ;
    }

    printf(" Enter no. of consumers (NW) : ") ;
    scanf("%d", &nw) ;
    if ( nw < 1 ) {
        printf(" ERROR : Number of workers must be positive.\n\n") ;
        return 0 ;
    }

    printf(" Enter no. of matrices : ") ;
    scanf("%d", &nm) ;
    if ( nm < 1 ) {
        printf(" ERROR : Number of matrices must be positive.\n\n") ;
        return 0 ;
    }
    

    int shmid = shmget(11, sizeof(segment), 0666|IPC_CREAT) ;
    if ( shmid < 0 ) {
        printf(" ERROR (%d) : Shared memory could not be created.\n\n", errno) ;
        exit(-1) ;
    }
    segment * SHM = (segment *)shmat(shmid, (void*)0, 0) ;
    if ( SHM == (segment *)(-1) ) {
        printf(" ERROR (%d) : Shared memory could not be attached.\n\n", errno) ;
        shmctl(shmid, IPC_RMID, NULL) ;
        exit(-1) ;
    }

    InitMem(SHM) ;
    SHM->_pending_jobs = nm ;

    
    lockid = shmget(12, sizeof(pthread_mutex_t), 0666|IPC_CREAT) ;
    if ( lockid < 0 ) {
        printf(" ERROR (%d) : Shared memory could not be created.\n\n", errno) ;
        shmdt(SHM) ;    shmctl(shmid, IPC_RMID, NULL) ;
        exit(-1) ;
    }
    lock = (pthread_mutex_t *)shmat(lockid, (void*)0, 0) ;
    if ( lock == (pthread_mutex_t *)(-1) ) {
        printf(" ERROR (%d) : Shared memory could not be attached.\n\n", errno) ;
        shmdt(SHM) ;    shmctl(shmid, IPC_RMID, NULL) ;
        shmctl(lockid, IPC_RMID, NULL) ;
        exit(-1) ;
    }

    pthread_mutexattr_t mutexattr ;
    if ( pthread_mutexattr_init(&mutexattr) ) {
        printf(" ERROR (%d) : Mutex attribute could not be initialized.\n\n", errno) ;
        shmdt(SHM) ;    shmctl(shmid, IPC_RMID, NULL) ;
        shmdt(lock) ;   shmctl(lockid, IPC_RMID, NULL) ;
        exit(-1) ;
    }
    if ( pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED) ) {
        printf(" ERROR (%d) : Mutex attribute could not be set.\n\n", errno) ;
        shmdt(SHM) ;    shmctl(shmid, IPC_RMID, NULL) ;
        shmdt(lock) ;   shmctl(lockid, IPC_RMID, NULL) ;
        exit(-1) ;
    }
    if ( pthread_mutex_init(lock, &mutexattr) ) {
        printf(" ERROR (%d) : Mutex lock could not be initialized.\n\n", errno) ;
        shmdt(SHM) ;    shmctl(shmid, IPC_RMID, NULL) ;
        shmdt(lock) ;   shmctl(lockid, IPC_RMID, NULL) ;
        exit(-1) ;
    }

    
    int nproc = np + nw ;
    int max_jobs_created = 2*nm - 1 ;
    int * all_children = (int *) calloc (nproc, sizeof(int)) ;
    int custom_id = 0 ;
    int counter = 0 ;
    clock_t start_time = clock() ;

    while ( np -- ) {
        custom_id ++ ;
        int pid = fork() ;
        if ( pid == 0 ) {
            Producer(shmid, custom_id) ;
            exit(0) ;
        }
        all_children[counter ++] = pid ;
    }

    custom_id = 0 ;
    while ( nw -- ) {
        custom_id ++ ;
        int pid = fork() ;
        if ( pid == 0 ) {
            Worker(shmid, custom_id) ;
            exit(0) ;
        }
        all_children[counter ++] = pid ;
    }


    while ( ! ( SHM->_job_created == max_jobs_created && 
                SHM->_buf._count == 1 &&
                SHM->_buf._jobs[SHM->_buf._out]._mult_stat == __wait__ ) ) ;


    double trace = 0.0 ;
    pthread_mutex_lock(lock) ;
    clock_t end_time = clock() ;
    for ( int d = 0 ; d < DIM ; d ++ )
        trace += SHM->_buf._jobs[SHM->_buf._out]._mat[d][d] ;
    pthread_mutex_unlock(lock) ;
    

    for ( int i = 0 ; i < nproc ; i ++ )    kill(all_children[i], SIGKILL) ;
    free(all_children) ;

    pthread_mutex_destroy(lock) ;

    if ( shmdt(SHM) == -1 )  {
		printf(" ERROR (%d) : Shared memory could not be detatched.\n\n", errno) ;
        shmdt(lock) ;
		exit(-1) ;
	}

    if ( shmdt(lock) == -1 )  {
		printf(" ERROR (%d) : Shared memory could not be detatched.\n\n", errno) ;
        exit(-1) ;
	}
    
    if ( shmctl(shmid, IPC_RMID, NULL) == -1 )  {
		printf(" ERROR (%d) : Shared memory could not be marked for destruction.\n\n", errno) ;
        shmctl(lockid, IPC_RMID, NULL) ;
		exit(-1) ;
	}

    if ( shmctl(lockid, IPC_RMID, NULL) == -1 )  {
		printf(" ERROR (%d) : Shared memory could not be marked for destruction.\n\n", errno) ;
        exit(-1) ;
	}


    printf("\n  TRACE OF PRODUCT OF ALL MATRICES : %lf", trace) ;
    double time_spent = (double)(end_time - start_time) / CLOCKS_PER_SEC ;
    printf("\n  TOTAL COMPUTATION TIME : %.3lf sec\n\n", time_spent) ;
    
    return 0 ;
}

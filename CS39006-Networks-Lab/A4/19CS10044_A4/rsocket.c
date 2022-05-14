
// CS39006 - Networks Laboratory
// Assignment 04 | 14 March 2022
// [ Nakul Aggarwal - 19CS10044 ]
// [ Hritaban Ghosh - 19CS30053 ]

#include "rsocket.h"

int msg_seq_no = 0 ;
unack_msg * unack_msg_table ;
recvd_msg_queue * recvd_msg_table ;
pthread_mutex_t lock_unack ;
pthread_mutex_t lock_recvd ;
pthread_t r_tid, s_tid ;
int fd_gbl = -1 ;

void * thread_handler_R ( void * fdv ) {
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL) ;
    int fd = fd_gbl ;

    while ( 1 ) {
        recvd_msg new_entry ;

        void * buff = malloc(RECV_SIZE) ;
        new_entry.size = recvfrom(fd, buff, RECV_SIZE, 0, 
                            &new_entry.sender_addr, &new_entry.sender_addr_len) ;
        
        if ( new_entry.size < 4 || dropMessage(p) ) {
            free(buff) ;
            continue ;
        }

        new_entry.msg = malloc(new_entry.size) ;
        memcpy(new_entry.msg, buff, new_entry.size) ;
        free(buff) ;
        
        int seq_no ;
        memcpy(&seq_no, new_entry.msg, 4) ;

        if ( new_entry.size == 7 && memcmp(new_entry.msg + 4, "ACK", 3) == 0 ) {
            free(new_entry.msg) ;
            
            for ( int k = 0 ; k < TABLE_SIZE ; k ++ ) { 
                pthread_mutex_lock(&lock_unack) ;

                if ( unack_msg_table[k].msg_seq_no != seq_no
                     && ! pthread_mutex_unlock(&lock_unack) )   continue ;
                
                if ( ! unack_msg_table[k].valid 
                     && ! pthread_mutex_unlock(&lock_unack) )   continue ;
                
                free(unack_msg_table[k].msg) ;
                unack_msg_table[k].valid = 0 ;

                pthread_mutex_unlock(&lock_unack) ;
                break ;
            }
            continue ;
        }
        

        pthread_mutex_lock(&lock_recvd) ;

        if ( recvd_msg_table->size == TABLE_SIZE && 
             ! pthread_mutex_unlock(&lock_recvd) ) {
            free(new_entry.msg) ;
            continue ;
        }
        
        if ( recvd_msg_table->size == 0 ) {
            recvd_msg_table->back = 0 ;
            recvd_msg_table->front = 0 ;
        }
        else
            recvd_msg_table->back = (recvd_msg_table->back + 1) % TABLE_SIZE ;

        (recvd_msg_table->size) ++ ;
        recvd_msg_table->table[recvd_msg_table->back] = new_entry ;

        pthread_mutex_unlock(&lock_recvd) ;


        char ack[7] ;
        memcpy(ack, &seq_no, 4) ;
        memcpy(ack+4, "ACK", 3) ;
        sendto(fd, ack, 7, 0, &new_entry.sender_addr, sizeof(new_entry.sender_addr)) ;
    }
}

void * thread_handler_S ( void * fdv ) {
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL) ;
    int fd = fd_gbl ;
    
    while ( 1 ) {
        sleep(T) ;

        for ( int k = 0 ; k < TABLE_SIZE ; k ++ ) {
            pthread_mutex_lock(&lock_unack) ;

            if ( ! unack_msg_table[k].valid && 
                 ! pthread_mutex_unlock(&lock_unack) )  continue ;
            if ( unack_msg_table[k].sent_time + 2 * T > time(NULL) &&
                 ! pthread_mutex_unlock(&lock_unack) )    continue ;
            
            unack_msg * entry = &unack_msg_table[k] ;
            
            int addrlen = sizeof(*(entry->dest_addr)) ;
            sendto(fd, entry->msg, entry->size, 0, entry->dest_addr, addrlen) ;
            unack_msg_table[k].sent_time = time(NULL) ;

            pthread_mutex_unlock(&lock_unack) ;
        }
    }
}

int r_socket (int __domain, int __type, int __protocol) {
    if ( __type != SOCK_MRP )   return -1 ;
    if ( pthread_mutex_init(&lock_recvd, NULL) ) return -1 ;
    if ( pthread_mutex_init(&lock_unack, NULL) ) return -1 ;

    pthread_attr_t attr ;
    if ( pthread_attr_init(&attr) ) return -1 ;

    srand(time(NULL)) ;
    memset(&r_tid, 0, sizeof(r_tid)) ;
    memset(&s_tid, 0, sizeof(s_tid)) ;

    msg_seq_no = 0 ;

    int sock = socket(__domain, SOCK_DGRAM, __protocol) ;
    if ( sock < 0 ) return sock ;

    recvd_msg_table = (recvd_msg_queue*) malloc (sizeof(recvd_msg_queue)) ;
    if ( ! recvd_msg_table ) {
        close(sock) ;
        return -1 ;
    }

    unack_msg_table = (unack_msg *) malloc (TABLE_SIZE * sizeof(unack_msg)) ;
    if ( ! unack_msg_table ) {
        free(recvd_msg_table) ;
        close(sock) ;
        return -1 ;
    }

    recvd_msg_table->front = 0 ;
    recvd_msg_table->back = 0 ;
    recvd_msg_table->size = 0 ;

    for ( int k = 0 ; k < TABLE_SIZE ; k ++ )
        unack_msg_table[k].valid = 0 ;
    
    fd_gbl = sock ;
    
    if ( pthread_create(&r_tid, &attr, thread_handler_R, NULL) ) {
        free(recvd_msg_table) ;
        free(unack_msg_table) ;
        close(sock) ;
        fd_gbl = -1 ;
        memset(&r_tid, 0, sizeof(r_tid)) ;
        return -1 ;
    }
    
    if ( pthread_create(&s_tid, &attr, thread_handler_S, NULL) ) {
        pthread_kill(r_tid, SIGKILL) ;
        free(recvd_msg_table) ;
        free(unack_msg_table) ;
        close(sock) ;
        fd_gbl = -1 ;
        memset(&r_tid, 0, sizeof(r_tid)) ;
        memset(&s_tid, 0, sizeof(s_tid)) ;
        return -1 ;
    }

    return sock ;
}

int r_bind (int __fd, const struct sockaddr *__addr, socklen_t __len) {
    return bind(__fd, __addr, __len) ;
}

ssize_t r_sendto (int __fd, const void *__buf, size_t __n, int __flags, 
                  const struct sockaddr *__addr, socklen_t __addr_len) {

    int seq = msg_seq_no ; 
    msg_seq_no ++ ;
    if ( msg_seq_no < 0 )   msg_seq_no = 0 ;
    
    void * msg = malloc(__n + 4) ;
    memset(msg, 0, __n + 4) ;
    memcpy(msg, &seq, 4) ;
    memcpy(msg+4, __buf, __n) ;

    int idx = 0 ;

    pthread_mutex_lock(&lock_unack) ;

    while ( idx < TABLE_SIZE && unack_msg_table[idx].valid )    idx ++ ;
    if ( idx == TABLE_SIZE ) {
        pthread_mutex_unlock(&lock_unack) ;
        free(msg) ; return -1 ;
    }

    pthread_mutex_unlock(&lock_unack) ;

    unack_msg * entry = &(unack_msg_table[idx]) ;
    entry->valid = 1 ;
    entry->msg_seq_no = seq ;
    entry->msg = msg ;
    entry->dest_addr = __addr ;
    entry->size = __n + 4 ;
    
    int ret = sendto(__fd, msg, __n + 4, __flags, __addr, __addr_len) ;
    entry->sent_time = time(NULL) ;
    return ret ;
}

ssize_t r_recvfrom (int __fd, void * __buf, size_t __n, int __flags, 
                    struct sockaddr * __addr, socklen_t * __addr_len) {
    
    if ( __n == 0 ) return 0 ;

    while ( 1 ) {
        pthread_mutex_lock(&lock_recvd) ;

        if ( recvd_msg_table->size )  {
            recvd_msg * rec = &recvd_msg_table->table[recvd_msg_table->front] ;
            *__addr = rec->sender_addr ;
            *__addr_len = rec->sender_addr_len ;

            int read_len = __n ;
            if ( read_len > rec->size ) read_len = rec->size ;

            memcpy(__buf, rec->msg, read_len) ;
            free(rec->msg) ;
            (recvd_msg_table->size) -- ;
            recvd_msg_table->front = (recvd_msg_table->front + 1) % TABLE_SIZE ;
            
            pthread_mutex_unlock(&lock_recvd) ;
            return read_len ;
        }
        else    {
            pthread_mutex_unlock(&lock_recvd) ;
            sleep(RECV_BLOCK_T) ;
        }
    }
}

int r_close (int __fd) {
    pthread_cancel(r_tid) ;
    pthread_cancel(s_tid) ;

    for ( int k = 0 ; k < TABLE_SIZE ; k ++ ) {
        if ( unack_msg_table[k].valid )
            free(unack_msg_table[k].msg) ;
    }
    free(unack_msg_table) ;

    if ( recvd_msg_table->size ) {
        int k = recvd_msg_table->front ;
        while ( 1 ) {
            free(recvd_msg_table->table[k].msg) ;
            if ( k == recvd_msg_table->back )   break ;
            k = (k + 1) % TABLE_SIZE ;
        }
    }
    free(recvd_msg_table) ;

    int ret ;
    if ( (ret = close(__fd)) )  return ret ;
    if ( (ret = pthread_mutex_destroy(&lock_recvd)) )   return ret ;
    if ( (ret = pthread_mutex_destroy(&lock_unack)) )   return ret ;

    return 0 ;
}

int dropMessage ( float __p ) {
    float r = 1.0 * rand() / RAND_MAX ;
    if ( r < __p )  return 1 ;
    return 0 ;
}


#include "memlab.hpp"

#define GC_ENABLE 				1
#define LEAST_HOLES_PERCENT 	0.25 		// 25%
#define EVALUATE    			1
#define TRACK_WORD_ALIGN        1


uint32_t __physical_memory_wsize__ = 0 ;
uint32_t __local_address__ = 0 ;
uint32_t __page_table_entr_count__ ;
uint8_t __logical_memory_space_filled__ = 0 ;

void * __physical_memory__ = NULL ;
List_U32 __holes__ ;
Stack __global_stack__ ;

pthread_mutex_t __compaction_lock__ ;
pthread_mutex_t __marknsweep_lock__ ;
pthread_t __gc_thread__ ;

struct timeval __last_compaction_timestamp__ ;
struct timeval __last_marknsweep_timestamp__ ;

sighandler_t __def_signal_handler__ ;

uint32_t __gc_auto_sleep_t__ ;
uint32_t __gc_compaction_gap__ ;
uint32_t __gc_marknsweep_gap__ ;

uint32_t __memory_footprints_sample_count__ = 0 ;
uint64_t __memory_footprints_sum_total__ = 0 ;
uint32_t __marknsweep_durations_sample_count__ = 0 ;
double __marknsweep_durations_sum_total__ = 0.0 ;
uint32_t __compaction_durations_sample_count__ = 0 ;
double __compaction_durations_sum_total__ = 0.0 ;


void initializeBookKeepingStructures ( uint32_t y );
void setEnv ( uint32_t slp , uint32_t comp_gap , uint32_t mns_gap );

void evaluate ( );
void freeMem ( );
void handleInterrupt ( int32_t sig );
void error ( const char * type , const char * msg );
__suseconds_t usecsSince ( const struct timeval * ts );

void* gc_thread (void * __);
void gc_initialize ( );
void compaction ( uint8_t flag );

uint8_t isValidIdentifier ( const char * sym );
const char * dataType ( uint8_t data_type_num );
uint32_t stackReferenceInScope ( const char * sym );
uint32_t stackReferenceVisible ( const char * sym );

void writePageTable ( uint32_t k , uint32_t frame_no , uint8_t data_type );
void writeKthWord ( uint32_t k , uint32_t val );
void writeKthWord ( uint32_t k , int32_t val );
void readKthWord ( uint32_t k , void * val );
void clearPageTableEntry ( uint32_t k );


void initializeBookKeepingStructures ( uint32_t y ) {
    __global_stack__.init((y << 1)) ;
    __holes__.init(y) ;
    for ( uint32_t f = __page_table_entr_count__ ; f < __physical_memory_wsize__ ; f ++ ) 
        __holes__.push_front(f) ;
    
    if ( EVALUATE ) {
        __memory_footprints_sample_count__ ++ ;
        __memory_footprints_sum_total__ += __holes__.size() ;
    }

    printf("\n +++ INITIALIZE BOOK-KEEPING DATA STRUCTURES +++\n") ;
    printf("    > SCOPE : initializeBookKeepingStructures\n") ;
    printf("    > GLOBAL STACK MEMORY : %ld bytes\n", sizeof(Stack) + (y << 1) * sizeof(stack_obj_t)) ;
    printf("    > GLOBAL STACK MAX WIDTH : %d elements\n", (y << 1)) ;
    printf("    > HOLES LIST RESERVED MEMORY : %ld bytes\n", sizeof(List_U32) + y * sizeof(list_uint32_t)) ;
    printf("    > HOLES LIST MAX WIDTH : %d elements\n", y) ;
}

void setEnv ( uint32_t slp , uint32_t comp_gap , uint32_t mns_gap ) {
    __def_signal_handler__ = signal(SIGINT, handleInterrupt);
    gettimeofday(&__last_compaction_timestamp__, NULL) ;
    gettimeofday(&__last_marknsweep_timestamp__, NULL) ;

    __gc_auto_sleep_t__ = slp ;
    __gc_compaction_gap__ = comp_gap ;
    __gc_marknsweep_gap__ = mns_gap ;

    if ( GC_ENABLE )    gc_initialize() ;

    printf("\n +++ SET ENVIRONMENT +++\n") ;
    printf("    > SCOPE : setEnv\n") ;
    printf("    > Timestamps initialized.\n") ;
    printf("    > Garbage collector initialized.\n") ;
    printf("    > Signal handlers redefined.\n") ;
    printf("    > __gc_auto_sleep_t__ = %d us\n", __gc_auto_sleep_t__) ;
    printf("    > __gc_compaction_gap__ = %d us\n", __gc_compaction_gap__) ;
    printf("    > __gc_marknsweep_gap__ = %d us\n", __gc_marknsweep_gap__) ;
}


void evaluate ( ) {
    double avg_memory_footprint = 1.0 * __memory_footprints_sum_total__ / __memory_footprints_sample_count__ ;
    avg_memory_footprint = (__physical_memory_wsize__ - __page_table_entr_count__) - avg_memory_footprint ;

    printf("\n +++ EVALUATION +++\n") ;
    printf("    > SCOPE : evaluate\n") ;
    printf("    > AVG. MEMORY FOOTPRINT : %.4f bytes\n", 4 * avg_memory_footprint) ;

    if ( __marknsweep_durations_sample_count__ ) {
        printf("    > AVG. MARK-AND-SWEEP GC DURATION : %.4f secs\n", 
            1.0 * __marknsweep_durations_sum_total__ / __marknsweep_durations_sample_count__) ;
    }
    else    printf("    > AVG. MARK-AND-SWEEP GC DURATION : NIL\n") ;

    if ( __compaction_durations_sample_count__ ) {
        printf("    > AVG. COMPACTION DURATION : %.4f secs\n", 
            1.0 * __compaction_durations_sum_total__ / __compaction_durations_sample_count__) ;
    }
    else    printf("    > AVG. COMPACTION DURATION : NIL\n") ;
}

void freeMem ( ) {
    if ( ! __physical_memory__ )    free(__physical_memory__) ;
    __global_stack__.destroy() ;
    __holes__.clear() ;
    printf("\n +++ FREE MEMORY +++\n") ;
    printf("    > SCOPE : freeMem\n") ;
    printf("    > Physical memory (created by createMem) freed.\n") ;
    printf("    > Memory reserved for all book-keeping structures freed.\n") ;
}

void stopMMU ( ) {
    pthread_cancel(__gc_thread__) ;
    freeMem() ;
    pthread_mutex_destroy(&__compaction_lock__) ;
    pthread_mutex_destroy(&__marknsweep_lock__) ;
    if ( EVALUATE ) evaluate() ;

    printf("\n +++ STOP MMU +++\n") ;
    printf("    > SCOPE : stopMMU\n") ;
    printf("    > Garbage collection thread cancelled.\n") ;
    printf("    > All allocated memory freed.\n") ;
    printf("    > MMU evaluation results printed.\n\n\n") ;
    
    return ;
}

void cleanAndExit ( ) {
    stopMMU() ;
    printf("\n +++ FORCED EXIT +++\n") ;
    printf("    > SCOPE : cleanAndExit\n\n\n") ;
    exit(-1) ;
}

void handleInterrupt ( int32_t sig ) {
    stopMMU();
    printf("\n +++ KEYBOARD INTERRUPT +++\n") ;
    printf("    > SCOPE : handleInterrupt\n") ;
    printf("    > MMU stopped successully.\n") ;
    printf("    > Default interrupt-handler will continue handling.\n\n\n") ;
    signal(SIGINT, __def_signal_handler__);
	raise(SIGINT);
}

void error ( const char * type , const char * msg ) {
    printf("\n\n [ %s : %s ]\n\n", type, msg) ;
    return ;
}

__suseconds_t usecsSince ( const struct timeval * ts ) {
    struct timeval now ;
    gettimeofday(&now, NULL) ;
    __suseconds_t diff = (now.tv_usec - ts->tv_usec) + 1000000 * (now.tv_sec - ts->tv_sec) ;
    return diff ;
}


uint8_t isValidIdentifier ( const char * sym ) {
    if ( ! sym || ! strlen(sym) ) {
        error("(isValidIdentifier) LENGTH ERROR", "Length of identifier must be non-zero.") ;
        return 0 ;
    }

    if ( strlen(sym) > 30 ) {
        error("(isValidIdentifier) LENGTH ERROR", "This MMU does not allow identifiers with length > 30.") ;
        return 0 ;
    }
    
    if ( sym[0] >= '0' && sym[0] <= '9' ) {
        error("(isValidIdentifier) SYNTAX ERROR", "Identifier must start with an underscore or an alphabet.") ;
        return 0 ;
    }

    uint32_t len = strlen(sym) ;
    while ( len -- ) {
        if ( sym[len] == '_' )  continue ;
        if ( sym[len] >= '0' && sym[len] <= '9' )   continue ;
        if ( sym[len] >= 'a' && sym[len] <= 'z' )   continue ;
        if ( sym[len] >= 'A' && sym[len] <= 'Z' )   continue ;
        error("(isValidIdentifier) SYNTAX ERROR", "Identifier can only consist underscores or alphanumeric symbols.") ;
        return 0 ;
    }
    return 1 ;
}

const char * dataType ( uint8_t data_type_num ) {
    if ( data_type_num == 0 )   return "int" ;
    if ( data_type_num == 1 )   return "med_int" ;
    if ( data_type_num == 2 )   return "char" ;
    if ( data_type_num == 3 )   return "bool" ;
    return NULL ;
}

uint32_t stackReferenceInScope ( const char * sym ) {
    uint32_t stack_size = __global_stack__.size() ;
    uint32_t reference ;

    for ( reference = stack_size - 1 ; reference >= 0 ; reference -- ) {
        if ( ! strcmp(__global_stack__[reference]._sym_name, "$scope$end$") )   break ;
        if ( ! strcmp(__global_stack__[reference]._sym_name, "$scope$end$local$") )   return 0 ;
        if ( ! strcmp(__global_stack__[reference]._sym_name, sym) ) return reference ;
    }

    if ( __global_stack__[reference]._arrel_flag_param_count == 0 )   return 0 ;
    uint8_t param_count = __global_stack__[reference]._arrel_flag_param_count ;

    reference -- ;
    while ( param_count -- ) {
        if ( ! strcmp(__global_stack__[reference]._sym_name, sym) ) return reference ;
        if ( ! param_count )    break ;
        
        if ( __global_stack__[reference]._arrel_flag_param_count ) {
            char arr_name[31] ; 
            strcpy(arr_name, __global_stack__[reference]._sym_name) ;
            while ( ! strcmp(__global_stack__[reference]._sym_name, arr_name) )  reference -- ;
        }
        else    reference -- ;
    }

    return 0 ;
}

uint32_t stackReferenceVisible ( const char * sym ) {
    uint32_t stack_size = __global_stack__.size() ;
    uint32_t reference ;

    for ( reference = stack_size - 1 ; reference >= 0 ; reference -- ) {
        if ( ! strcmp(__global_stack__[reference]._sym_name, "$scope$end$") )   break ;
        if ( ! strcmp(__global_stack__[reference]._sym_name, "$scope$end$local$") )   continue ;
        if ( ! strcmp(__global_stack__[reference]._sym_name, sym) ) return reference ;
    }

    if ( __global_stack__[reference]._arrel_flag_param_count == 0 )   return 0 ;
    uint8_t param_count = __global_stack__[reference]._arrel_flag_param_count ;

    reference -- ;
    while ( param_count -- ) {
        if ( ! strcmp(__global_stack__[reference]._sym_name, sym) ) return reference ;
        if ( ! param_count )    break ;
        
        if ( __global_stack__[reference]._arrel_flag_param_count ) {
            char arr_name[31] ; 
            strcpy(arr_name, __global_stack__[reference]._sym_name) ;
            while ( ! strcmp(__global_stack__[reference]._sym_name, arr_name) )  reference -- ;
        }
        else    reference -- ;
    }

    return 0 ;
}


void gc_run ( uint8_t flag ) {

    if ( usecsSince(&__last_marknsweep_timestamp__) < __gc_marknsweep_gap__ )  return ;

    uint32_t hashtable_init_size = __physical_memory_wsize__ - __page_table_entr_count__ ;
    Unordered_Set_U32 marked_frames(hashtable_init_size) ;

    
    pthread_mutex_lock(&__marknsweep_lock__) ;


    gettimeofday(&__last_marknsweep_timestamp__, NULL) ;

    printf("\n +++ MARK-AND-SWEEP GARBAGE COLLECTION +++\n") ;
    printf("    > SCOPE : gc_run\n") ;
    printf("    > STEP : Mark phase start\n") ;
    if ( flag == GC_AUTOMATIC ) printf("    > ACTIVATION : Automatic\n") ;
    else    printf("    > ACTIVATION : Voluntary\n") ;


    for ( uint32_t page_no = 0 ; page_no < __page_table_entr_count__ ; page_no ++ ) {
        uint32_t page_table_entry ;
        readKthWord(page_no, &page_table_entry) ;

        uint32_t frame_no = ((page_table_entry << 2) >> 2) ;
        if ( frame_no == 0 )    break ;

        marked_frames.insert(frame_no) ;
    }


    uint32_t holes_size_bef = __holes__.size() ;
    uint32_t marked_size = marked_frames.size() ;

    printf("\n +++ MARK-AND-SWEEP GARBAGE COLLECTION +++\n") ;
    printf("    > SCOPE : gc_run\n") ;
    printf("    > STEP : Mark phase end\n") ;
    printf("    > MARKED FRAMES : %d\n", marked_size) ;

    printf("\n +++ MARK-AND-SWEEP GARBAGE COLLECTION +++\n") ;
    printf("    > SCOPE : gc_run\n") ;
    printf("    > STEP : Sweep phase start\n") ;
    printf("    > FREE FRAMES (BEFORE SWEEP) : %d\n", holes_size_bef) ;


    __holes__.clear() ;
    for ( uint32_t frame = __page_table_entr_count__ ; frame < __physical_memory_wsize__ ; frame ++ ) {
        if ( ! marked_frames.find(frame) )  __holes__.push_front(frame) ;
    }
    marked_frames.clear() ;

    
    printf("\n +++ MARK-AND-SWEEP GARBAGE COLLECTION +++\n") ;
    printf("    > SCOPE : gc_run\n") ;
    printf("    > STEP : Sweep phase end\n") ;
    printf("    > FREE FRAMES (AFTER SWEEP) : %d\n", __holes__.size()) ;
    uint32_t garbage_frames_count = __holes__.size() - holes_size_bef ;
    printf("    > GARBAGE FRAMES : %d\n", garbage_frames_count) ;

    if ( EVALUATE && garbage_frames_count ) {
        __memory_footprints_sample_count__ ++ ;
        __memory_footprints_sum_total__ += __holes__.size() ;
    }

    double duration_mns = usecsSince(&__last_marknsweep_timestamp__) / 1000000.0 ;
    printf("    > TIME TAKEN : %.5f secs\n",  duration_mns) ;

    if ( EVALUATE ) {
        __marknsweep_durations_sample_count__ ++ ;
        __marknsweep_durations_sum_total__ += duration_mns ;
    }
    
    gettimeofday(&__last_marknsweep_timestamp__, NULL) ;


    pthread_mutex_unlock(&__marknsweep_lock__) ;

    
    
    if ( garbage_frames_count )
        compaction(flag) ;

    return ;
}

void* gc_thread (void * __) {
    printf("\n +++ GARBAGE COLLECTION THREAD STARTED +++\n") ;
    printf("    > SCOPE : gc_thread\n") ;
    printf("    > Thread will periodically run the GC for memory management.\n") ;

    pthread_detach(pthread_self());
    while ( 1 ) {
        gc_run(GC_AUTOMATIC) ;
        usleep(__gc_auto_sleep_t__) ;
    }
    pthread_exit(NULL);
}

void gc_initialize ( ) {
    printf("\n +++ GARBAGE COLLECTIOR INITIALIZER +++\n") ;
    printf("    > SCOPE : gc_initialize\n") ;
    printf("    > To spawn a thread in the same process for garbage collection.\n") ;
    pthread_create(&__gc_thread__, NULL, &gc_thread, NULL);
}

void compaction ( uint8_t flag ) {
    double holes_percent = 1.0 * __holes__.size() / (__physical_memory_wsize__ - __page_table_entr_count__) ;
    
    if ( holes_percent < LEAST_HOLES_PERCENT )  return ;
    if ( usecsSince(&__last_compaction_timestamp__) < __gc_compaction_gap__ )  return ;


    pthread_mutex_lock(&__compaction_lock__) ;
    pthread_mutex_lock(&__marknsweep_lock__) ;

    gettimeofday(&__last_compaction_timestamp__, NULL) ;

    printf("\n +++ COMPACTION +++\n") ;
    printf("    > SCOPE : gc_run\n") ;
    if ( flag == GC_AUTOMATIC ) printf("    > ACTIVATION : Automatic\n") ;
    else    printf("    > ACTIVATION : Voluntary\n") ;
    printf("    > STATUS : Starting\n") ;
    
    
    uint32_t last_page_no = 0 ;
    while ( last_page_no < __page_table_entr_count__ ) {
        uint32_t page_table_entry ;
        readKthWord(last_page_no, &page_table_entry) ;
        uint32_t frame_no = ((page_table_entry << 2) >> 2) ;

        if ( ! frame_no )   break ;
        last_page_no ++ ;
    }

    if ( ! last_page_no ) {
        double duration = usecsSince(&__last_compaction_timestamp__) / 1000000.0 ;

        printf("\n +++ COMPACTION (END) +++\n") ;
        printf("    > SCOPE : gc_run\n") ;
        printf("    > STATUS : Finished\n") ;
        printf("    > TIME TAKEN : %.5f secs\n", duration) ;

        if ( EVALUATE ) {
            __compaction_durations_sample_count__ ++ ;
            __compaction_durations_sum_total__ += duration ;
        }
        
        gettimeofday(&__last_compaction_timestamp__, NULL) ;

        pthread_mutex_unlock(&__marknsweep_lock__) ;
        pthread_mutex_unlock(&__compaction_lock__) ;
        return ;
    }

    last_page_no -- ;

    Min_Heap_U32 holes_heap(__holes__) ;
    __holes__.clear() ;

    uint32_t compacted_phys_seg_end_frame_no = __page_table_entr_count__ + last_page_no ;

    printf("\n +++ COMPACTION +++\n") ;
    printf("    > SCOPE : gc_run\n") ;
    printf("    > STATUS : Reallocating frames in a contiguous fashion\n") ;
    
    for ( uint32_t virt_add = 0 ; virt_add <= last_page_no ; virt_add ++ ) {
        uint32_t page_table_entry ;
        readKthWord(virt_add, &page_table_entry) ;

        uint32_t old_frame_no = ((page_table_entry << 2) >> 2) ;
        
        if ( ! old_frame_no )   break ;
        if ( old_frame_no <= compacted_phys_seg_end_frame_no )  continue ;

        uint8_t data_type = (page_table_entry >> 30) ;
        uint32_t new_frame_no = holes_heap.top() ;

        writePageTable(virt_add, new_frame_no, data_type) ;

        uint32_t frame_content ;
        readKthWord(old_frame_no, &frame_content) ;
        writeKthWord(new_frame_no, frame_content) ;

        holes_heap.pop() ;
        holes_heap.push(old_frame_no) ;
    }

    printf("\n +++ COMPACTION +++\n") ;
    printf("    > SCOPE : gc_run\n") ;
    printf("    > STATUS : Updating the holes-list with the new pool of free frames\n") ;

    while ( ! holes_heap.empty() ) {
        __holes__.push_front(holes_heap.top()) ;
        holes_heap.pop() ;
    }
    holes_heap.clear() ;

    double duration = usecsSince(&__last_compaction_timestamp__) / 1000000.0 ;
    
    printf("\n +++ COMPACTION +++\n") ;
    printf("    > SCOPE : gc_run\n") ;
    printf("    > STATUS : Finished\n") ;
    printf("    > TIME TAKEN : %.5f secs\n", duration) ;

    if ( EVALUATE ) {
        __compaction_durations_sample_count__ ++ ;
        __compaction_durations_sum_total__ += duration ;
    }

    gettimeofday(&__last_compaction_timestamp__, NULL) ;
    

    pthread_mutex_unlock(&__marknsweep_lock__) ;
    pthread_mutex_unlock(&__compaction_lock__) ;


    return ;
}


void createMem ( uint32_t words , uint32_t slp , uint32_t comp_gap , uint32_t mns_gap ) {
    if ( words > (1 << 30) ) {
        printf("\n\n [ ERROR : User process demanded more than 4GB of memory ]\n\n") ;
        exit(-1) ;
    }
    if ( words < 40 ) {
        printf("\n\n [ ERROR : Atleast 160 bytes of memory will be needed by the MMU ]\n\n") ;
        exit(-1) ;
    }

    uint32_t y = (words - 10) / 30 ;
    __physical_memory_wsize__ = (y << 1) ;

    __physical_memory__ = malloc((__physical_memory_wsize__ << 2)) ;
    if ( ! __physical_memory__ ) {
        printf("\n\n [ ERROR : MMU could not create memory ]\n\n") ;
        exit(-1) ;
    }

    memset(__physical_memory__, 0, (__physical_memory_wsize__ << 2)) ;

    
    __local_address__ = 0 ;
    __page_table_entr_count__ = y ;


    printf("\n +++ CREATED MEMORY +++\n") ;
    printf("    > SCOPE : createMem\n") ;
    printf("    > PHYSICAL MEMORY SIZE : %d bytes\n", (__physical_memory_wsize__ << 2)) ;
    printf("    > PAGE TABLE LENGTH : %d entries\n", __page_table_entr_count__) ;
    printf("    > PAGE TABLE SIZE : %d bytes\n", (__page_table_entr_count__ << 2)) ;
    printf("    > FREE FRAMES (HOLES) : %d\n", __physical_memory_wsize__ - __page_table_entr_count__) ;

    initializeBookKeepingStructures(y) ;
    setEnv(slp, comp_gap, mns_gap) ;
}

void writePageTable ( uint32_t k , uint32_t frame_no , uint8_t data_type ) {
    if ( k < __page_table_entr_count__ ) {
        uint32_t pte = frame_no ;
        if ( data_type % 2 )   pte = (pte | (1 << 30)) ;
        if ( data_type > 1 )   pte = (pte | (1 << 31)) ;
        memcpy((char*)__physical_memory__ + (k << 2), &pte, 4) ;
    }
    else {
        error("(writePageTable) OUT OF BOUND", "Word index must be less than the number of entries in the page table.") ;
        cleanAndExit() ;
    }

    if ( TRACK_WORD_ALIGN ) {
        printf("\n +++ WORD ALIGNMENT +++\n") ;
        printf("    > SCOPE : writePageTable\n") ;
        printf("    > PURPOSE : Update the k-th page table entry.\n") ;

        printf("\n +++ UPDATING PAGE TABLE ENTRY +++\n") ;
        printf("    > SCOPE : writePageTable\n") ;
        printf("    > WORD INDEX : %d\n", k) ;
        printf("    > PAGE TABLE INDEX : %d\n", k) ;
        printf("    > (UPDATED FIELDS -> [31:30]) : Data type %d\n", data_type) ;
        printf("    > (UPDATED FIELDS -> [29:00]) : Frame no. %d\n", frame_no) ;
    }

    return ;
}

void writeKthWord ( uint32_t k , uint32_t val ) {
    if ( k < __physical_memory_wsize__ ) 
        memcpy((char*)__physical_memory__ + (k << 2), &val, 4) ;
    else {
        error("(writeKthWord) OUT OF BOUND", "Word index must be less than the number of words in the memory.") ;
        cleanAndExit() ;
    }

    if ( TRACK_WORD_ALIGN ) {
        printf("\n +++ WORD ALIGNMENT +++\n") ;
        printf("    > SCOPE : writeKthWord\n") ;
        printf("    > PURPOSE : Write the k-th word of the physical memory.\n") ;
        printf("    > WORD INDEX : %d\n", k) ;
        printf("    > WRITTEN VALUE : %d\n", val) ;
    }

    return ;
}

void writeKthWord ( uint32_t k , int32_t val ) {
    if ( k < __physical_memory_wsize__ ) 
        memcpy((char*)__physical_memory__ + (k << 2), &val, 4) ;
    else {
        error("(writeKthWord) OUT OF BOUND", "Word index must be less than the number of words in the memory.") ;
        cleanAndExit() ;
    }

    if ( TRACK_WORD_ALIGN ) {
        printf("\n +++ WORD ALIGNMENT +++\n") ;
        printf("    > SCOPE : writeKthWord\n") ;
        printf("    > PURPOSE : Write the k-th word of the physical memory.\n") ;
        printf("    > WORD INDEX : %d\n", k) ;
        printf("    > WRITTEN VALUE : %d\n", val) ;
    }

    return ;
}

void readKthWord ( uint32_t k , void * val ) {
    if ( k < __physical_memory_wsize__ ) 
        memcpy(val, (char*)__physical_memory__ + (k << 2), 4) ;
    else {
        error("(readKthWord) OUT OF BOUND", "Word index must be less than the number of words in the memory.") ;
        cleanAndExit() ;
    }

    if ( TRACK_WORD_ALIGN ) {
        printf("\n +++ WORD ALIGNMENT +++\n") ;
        printf("    > SCOPE : readKthWord\n") ;
        printf("    > PURPOSE : Read the k-th word of the physical memory.\n") ;
        printf("    > WORD INDEX : %d\n", k) ;
        printf("    > READ VALUE : %d\n", *(uint32_t*)(val)) ;
    }

    return ;
}

void clearPageTableEntry ( uint32_t k ) {
    if ( k < __page_table_entr_count__ )
        memset((char*)__physical_memory__ + (k << 2), 0, 4) ;
    else {
        error("(clearPageTableEntry) OUT OF BOUND", "Word index must be less than the number of entries in the page table.") ;
        cleanAndExit() ;
    }

    if ( TRACK_WORD_ALIGN ) {
        printf("\n +++ WORD ALIGNMENT +++\n") ;
        printf("    > SCOPE : clearPageTableEntry\n") ;
        printf("    > PURPOSE : Clear/reset the k-th page table entry.\n") ;

        printf("\n +++ UPDATING PAGE TABLE ENTRY +++\n") ;
        printf("    > SCOPE : writePageTable\n") ;
        printf("    > WORD INDEX : %d\n", k) ;
        printf("    > PAGE TABLE INDEX : %d\n", k) ;
    }

    return ;
}


void initStackFrame ( uint8_t param_count , uint8_t is_fn_scope ) {
    if ( is_fn_scope == FUNCTION_SCOPE ) {
        stack_obj_t new_stack_obj ;
        strcpy(new_stack_obj._sym_name, "$scope$end$") ;
        new_stack_obj._arrel_flag_param_count = param_count ;
        __global_stack__.push_back(&new_stack_obj) ;
        
        printf("\n +++ SET STACK BASE-POINTER +++\n") ;
        printf("    > SCOPE : initStackFrame\n") ;
        printf("    > BASE-POINTER TYPE : function scope\n") ;
        printf("    > GLOBAL STACK SIZE : %d\n", __global_stack__.size()) ;
        printf("    > FUNCTION PARAM COUNT : %d\n", param_count) ;
    }
    else if ( is_fn_scope == LOCAL_SCOPE) {
        if ( param_count ) {
            error("(initStackFrame) SCOPE TYPE", "Parameters can be passed only to a FUNCTION_SCOPE.") ;
            cleanAndExit() ;
        }

        stack_obj_t new_stack_obj ;
        strcpy(new_stack_obj._sym_name, "$scope$end$local$") ;
        new_stack_obj._arrel_flag_param_count = 0 ;
        __global_stack__.push_back(&new_stack_obj) ;

        printf("\n +++ SET STACK BASE-POINTER +++\n") ;
        printf("    > SCOPE : initStackFrame\n") ;
        printf("    > BASE-POINTER TYPE : local scope\n") ;
        printf("    > GLOBAL STACK SIZE : %d\n", __global_stack__.size()) ;
    }
    else {
        error("(initStackFrame) SCOPE TYPE", "MMU provides only FUNCTION_SCOPE and LOCAL_SCOPE types of scopes.") ;
        cleanAndExit() ;
    }
}

void deinitStackFrame ( ) {
    if ( ! __global_stack__.size() ) {
        error("(deinitStackFrame) STACK ERROR", "Global stack is empty.") ;
        cleanAndExit() ;
    }

    if ( __global_stack__.back()._sym_name[0] == '$' ) {
        printf("\n +++ POP STACK BASE-POINTER +++\n") ;
        printf("    > SCOPE : deinitStackFrame\n") ;

        if ( strlen(__global_stack__.back()._sym_name) > 12 )
            printf("    > BASE-POINTER TYPE : local scope\n") ;
        else    printf("    > BASE-POINTER TYPE : function scope\n") ;
        
        __global_stack__.pop_back() ;
        printf("    > GLOBAL STACK SIZE : %d\n", __global_stack__.size()) ;
    }
    
    else {
        error("(deinitStackFrame) STACK ERROR", "Can only de-initialize a base-pointer.") ;
        cleanAndExit() ;
    }
    return ;
}


void createVar ( const char * type , const char * sym ) {
    if ( ! type || ! strlen(type) )  return ;
    if ( ! isValidIdentifier(sym) )  {
        error("(createVar) IDENTIFIER ERROR", "Identifier is incompatible to the MMU.") ;
        cleanAndExit() ;
    }

    uint8_t data_type ;
    if ( strcmp(type, "int") == 0 ) data_type = 0 ;
    else if ( strcmp(type, "med_int") == 0 ) data_type = 1 ;
    else if ( strcmp(type, "char") == 0 ) data_type = 2 ;
    else if ( strcmp(type, "bool") == 0 ) data_type = 3 ;
    else {
        error("(createVar) TYPE ERROR", "Invalid data type.") ;
        cleanAndExit() ;
    }

    if ( stackReferenceInScope(sym) ) {
        error("(createVar) DECLARATION ERROR", "Variable cannot be re-declared.") ;
        cleanAndExit() ;
    }

    if ( __logical_memory_space_filled__ ) {
        error("(createVar) MEMORY ERROR", "Memory space is full.") ;
        cleanAndExit() ;
    }


    pthread_mutex_lock(&__compaction_lock__) ;
    pthread_mutex_lock(&__marknsweep_lock__) ;

    if ( ! __holes__.size() ) {
        error("(createVar) MEMORY ERROR", "No free frames left in the physical memory.") ;
        pthread_mutex_unlock(&__marknsweep_lock__) ;
        pthread_mutex_unlock(&__compaction_lock__) ;
        cleanAndExit() ;
    }

    uint32_t virtual_address = (__local_address__ >> 2) ;
    uint32_t first_fit_frame = __holes__.back() ;

    __holes__.pop_back() ;

    if ( EVALUATE ) {
        __memory_footprints_sample_count__ ++ ;
        __memory_footprints_sum_total__ += __holes__.size() ;
    }
    
    writePageTable(virtual_address, first_fit_frame, data_type) ;

    printf("\n +++ VARIABLE CREATION (FRAME ALLOCATION + PAGE-TABLE UPDATE) +++\n") ;
    printf("    > SCOPE : createVar\n") ;
    printf("    > VARIABLE TYPE : %s\n", type) ;
    printf("    > VARIABLE NAME : %s\n", sym) ;
    printf("    > LOCAL ADDRESS : %d\n", __local_address__) ;
    printf("    > VIRTUAL ADDRESS : %d\n", virtual_address) ;
    printf("    > PHYSICAL FRAME NO. : %d\n", first_fit_frame) ;
    printf("    > FREE FRAMES LEFT : %d\n", __holes__.size()) ;

    pthread_mutex_unlock(&__marknsweep_lock__) ;
    pthread_mutex_unlock(&__compaction_lock__) ;


    stack_obj_t new_stack_obj ;
    new_stack_obj._local_add = __local_address__ ;
    strcpy(new_stack_obj._sym_name, sym) ;
    new_stack_obj._arrel_flag_param_count = 0 ;
    __global_stack__.push_back(&new_stack_obj) ;

    if ( __local_address__ == ((__page_table_entr_count__ - 1) << 2) ) 
        __logical_memory_space_filled__ = 1 ;

    __local_address__ += 4 ;

    printf("\n +++ VARIABLE CREATION (GLOBAL-STACK UPDATE) +++\n") ;
    printf("    > SCOPE : createVar\n") ;
    printf("    > VARIABLE TYPE : %s\n", type) ;
    printf("    > VARIABLE NAME : %s\n", sym) ;
    printf("    > VIRTUAL ADDRESS : %d\n", virtual_address) ;
    printf("    > PHYSICAL FRAME NO. : %d\n", first_fit_frame) ;
    printf("    > GLOBAL STACK SIZE : %d\n", __global_stack__.size()) ;

    return ;
}

void assignVar ( const char * sym , const char * value ) {
    if ( ! value || ! strlen(value) )  return ;
    if ( ! isValidIdentifier(sym) )  {
        error("(assignVar) IDENTIFIER ERROR", "Identifier is incompatible to the MMU.") ;
        cleanAndExit() ;
    }
    
    uint32_t reference = stackReferenceVisible(sym) ;

    if ( ! reference ) {
        error("(assignVar) REFERENCE ERROR", "Identifier is not bound to any variable in the (visible) scope.") ;
        cleanAndExit() ;
    }

    if ( __global_stack__[reference]._arrel_flag_param_count ) {
        error("(assignVar) USE ERROR", "assignVar function cannot be used to assign array elements.") ;
        cleanAndExit() ;
    }

    uint32_t local_add = __global_stack__[reference]._local_add ;
    uint32_t virt_add = (local_add >> 2) ;
    uint32_t word ;

    pthread_mutex_lock(&__compaction_lock__) ;

    readKthWord(virt_add, &word) ;

    uint8_t data_type = (word >> 30) ;
    uint32_t frame_no = ((word << 2) >> 2) ;

    if ( data_type == 0 ) {
        uint32_t len = strlen(value) ;
        if ( len > 11 || (len == 11 && value[0] != '-') ) {
            error("(assignVar) ASSIGNMENT ERROR", "Variable with int data-type is not assigned correctly.") ;
            pthread_mutex_unlock(&__compaction_lock__) ;
            cleanAndExit() ;
        }

        for ( uint32_t it = 1 ; it < len ; it ++ ) {
            if ( value[it] > '9' || value[it] < '0' ) {
                error("(assignVar) ASSIGNMENT ERROR", "Variable with int data-type is not assigned correctly.") ;
                pthread_mutex_unlock(&__compaction_lock__) ;
                cleanAndExit() ;
            }
        }

        if ( (value[0] > '9' || value[0] < '0') && value[0] != '-' ) {
            error("(assignVar) ASSIGNMENT ERROR", "Variable with int data-type is not assigned correctly.") ;
            pthread_mutex_unlock(&__compaction_lock__) ;
            cleanAndExit() ;
        }
        if ( value[0] == '-' && len == 1 ) {
            error("(assignVar) ASSIGNMENT ERROR", "Variable with int data-type is not assigned correctly.") ;
            cleanAndExit() ;
        }

        int64_t test_numeric_value = atoll(value) ;
        if ( test_numeric_value < INT32_MIN || test_numeric_value > INT32_MAX ) {
            error("(assignVar) RANGE ERROR", "Variable with int data-type is not assigned with a 32-bit integer.") ;
            pthread_mutex_unlock(&__compaction_lock__) ;
            cleanAndExit() ;
        }

        int32_t numeric_value = atoi(value) ;
        writeKthWord(frame_no, numeric_value) ;

        printf("\n +++ VARIABLE ASSIGNMENT +++\n") ;
        printf("    > SCOPE : assignVar\n") ;
        printf("    > VARIABLE TYPE : %s\n", dataType(data_type)) ;
        printf("    > VARIABLE NAME : %s\n", sym) ;
        printf("    > VARIABLE VALUE : %d\n", numeric_value) ;
        printf("    > LOCAL ADDRESS : %d\n", local_add) ;
        printf("    > VIRTUAL ADDRESS : %d\n", virt_add) ;
        printf("    > PHYSICAL FRAME NO. : %d\n", frame_no) ;

        pthread_mutex_unlock(&__compaction_lock__) ;
        return ;
    }

    else if ( data_type == 1 ) {
        uint32_t len = strlen(value) ;
        if ( len > 8 || (len == 8 && value[0] != '-') ) {
            error("(assignVar) ASSIGNMENT ERROR", "Variable with med_int data-type is not assigned correctly.") ;
            pthread_mutex_unlock(&__compaction_lock__) ;
            cleanAndExit() ;
        }

        for ( uint32_t it = 1 ; it < len ; it ++ ) {
            if ( value[it] > '9' || value[it] < '0' ) {
                error("(assignVar) ASSIGNMENT ERROR", "Variable with med_int data-type is not assigned correctly.") ;
                pthread_mutex_unlock(&__compaction_lock__) ;
                cleanAndExit() ;
            }
        }

        if ( (value[0] > '9' || value[0] < '0') && value[0] != '-' ) {
            error("(assignVar) ASSIGNMENT ERROR", "Variable with med_int data-type is not assigned correctly.") ;
            pthread_mutex_unlock(&__compaction_lock__) ;
            cleanAndExit() ;
        }
        if ( value[0] == '-' && len == 1 ) {
            error("(assignVar) ASSIGNMENT ERROR", "Variable with med_int data-type is not assigned correctly.") ;
            pthread_mutex_unlock(&__compaction_lock__) ;
            cleanAndExit() ;
        }

        int32_t numeric_value = atoi(value) ;
        int32_t max_limit_3B = (1 << 23) ;
        if ( numeric_value < (-1 * max_limit_3B) || numeric_value > max_limit_3B - 1 ) {
            error("(assignVar) RANGE ERROR", "Variable with med_int data-type is not assigned with a 24-bit integer.") ;
            pthread_mutex_unlock(&__compaction_lock__) ;
            cleanAndExit() ;
        }
        
        writeKthWord(frame_no, numeric_value) ;

        printf("\n +++ VARIABLE ASSIGNMENT +++\n") ;
        printf("    > SCOPE : assignVar\n") ;
        printf("    > VARIABLE TYPE : %s\n", dataType(data_type)) ;
        printf("    > VARIABLE NAME : %s\n", sym) ;
        printf("    > VARIABLE VALUE : %d\n", numeric_value) ;
        printf("    > LOCAL ADDRESS : %d\n", local_add) ;
        printf("    > VIRTUAL ADDRESS : %d\n", virt_add) ;
        printf("    > PHYSICAL FRAME NO. : %d\n", frame_no) ;

        pthread_mutex_unlock(&__compaction_lock__) ;
        return ;
    }

    else if ( data_type == 2 ) {
        if ( strlen(value) > 1 ) {
            error("(assignVar) ASSIGNMENT ERROR", "Variable with char data-type is not assigned correctly.") ;
            pthread_mutex_unlock(&__compaction_lock__) ;
            cleanAndExit() ;
        }

        writeKthWord(frame_no, value[0]) ;

        printf("\n +++ VARIABLE ASSIGNMENT +++\n") ;
        printf("    > SCOPE : assignVar\n") ;
        printf("    > VARIABLE TYPE : %s\n", dataType(data_type)) ;
        printf("    > VARIABLE NAME : %s\n", sym) ;
        printf("    > VARIABLE VALUE : %c\n", value[0]) ;
        printf("    > LOCAL ADDRESS : %d\n", local_add) ;
        printf("    > VIRTUAL ADDRESS : %d\n", virt_add) ;
        printf("    > PHYSICAL FRAME NO. : %d\n", frame_no) ;

        pthread_mutex_unlock(&__compaction_lock__) ;
        return ;
    }

    else if ( data_type == 3 ) {
        if ( strcmp(value, "true") && strcmp(value, "false") ) {
            error("(assignVar) ASSIGNMENT ERROR", "Variable with bool data-type is not assigned correctly.") ;
            pthread_mutex_unlock(&__compaction_lock__) ;
            cleanAndExit() ;
        }

        if ( strcmp(value, "true") == 0 )    writeKthWord(frame_no, 1) ;
        else    writeKthWord(frame_no, 0) ;

        printf("\n +++ VARIABLE ASSIGNMENT +++\n") ;
        printf("    > SCOPE : assignVar\n") ;
        printf("    > VARIABLE TYPE : %s\n", dataType(data_type)) ;
        printf("    > VARIABLE NAME : %s\n", sym) ;

        if ( strcmp(value, "true") == 0 )   printf("    > VARIABLE VALUE : 1\n") ;
        else    printf("    > VARIABLE VALUE : 0\n") ;

        printf("    > LOCAL ADDRESS : %d\n", local_add) ;
        printf("    > VIRTUAL ADDRESS : %d\n", virt_add) ;
        printf("    > PHYSICAL FRAME NO. : %d\n", frame_no) ;

        pthread_mutex_unlock(&__compaction_lock__) ;
        return ;
    }

    else {
        error("(assignVar) MEMORY FAULT", "Page table got corrupted.") ;
        pthread_mutex_unlock(&__compaction_lock__) ;
        cleanAndExit() ;
    }
}


void createArr ( const char * type , const char * sym , uint32_t size ) {
    if ( ! type || ! strlen(type) )  return ;
    if ( ! isValidIdentifier(sym) )  {
        error("(createArr) IDENTIFIER ERROR", "Identifier is incompatible to the MMU.") ;
        cleanAndExit() ;
    }
    if ( size == 0 ) {
        error("(createArr) SIZE ERROR", "Size of an array must be greater than zero.") ;
        cleanAndExit() ;
    }

    uint8_t data_type ;
    if ( strcmp(type, "int") == 0 ) data_type = 0 ;
    else if ( strcmp(type, "med_int") == 0 ) data_type = 1 ;
    else if ( strcmp(type, "char") == 0 ) data_type = 2 ;
    else if ( strcmp(type, "bool") == 0 ) data_type = 3 ;
    else {
        error("(createArr) TYPE ERROR", "Invalid data type.") ;
        cleanAndExit() ;
    }

    if ( stackReferenceInScope(sym) ) {
        error("(createArr) DECLARATION ERROR", "Variable cannot be re-declared.") ;
        cleanAndExit() ;
    }

    uint32_t first_local_add = __local_address__ ;
    uint32_t first_virt_add = (__local_address__ >> 2) ;
    uint32_t last_local_add ;
    uint32_t last_virt_add ;
    uint32_t holes_size_before = __holes__.size() ;

    for ( uint32_t idx = 0 ; idx < size ; idx ++ ) {
        if ( __logical_memory_space_filled__ ) {
            error("(createArr) MEMORY ERROR", "Memory space is full.") ;
            cleanAndExit() ;
        }

        uint32_t virtual_address = (__local_address__ >> 2) ;
        last_virt_add = virtual_address ;
        last_local_add = __local_address__ ;

        pthread_mutex_lock(&__compaction_lock__) ;
        pthread_mutex_lock(&__marknsweep_lock__) ;

        if ( __holes__.size() == 0 ) {
            error("(createArr) MEMORY ERROR", "No free frames left in the physical memory.") ;
            pthread_mutex_unlock(&__marknsweep_lock__) ;
            pthread_mutex_unlock(&__compaction_lock__) ;
            cleanAndExit() ;
        }

        uint32_t first_fit_frame = __holes__.back() ;
        __holes__.pop_back() ;

        if ( EVALUATE ) {
            __memory_footprints_sample_count__ ++ ;
            __memory_footprints_sum_total__ += __holes__.size() ;
        }
        
        writePageTable(virtual_address, first_fit_frame, data_type) ;

        pthread_mutex_unlock(&__marknsweep_lock__) ;
        pthread_mutex_unlock(&__compaction_lock__) ;

        stack_obj_t new_stack_obj ;
        new_stack_obj._local_add = __local_address__ ;
        strcpy(new_stack_obj._sym_name, sym) ;
        new_stack_obj._array_idx = idx ;
        new_stack_obj._arrel_flag_param_count = 1 ;
        __global_stack__.push_back(&new_stack_obj) ;

        if ( __local_address__ == ((__page_table_entr_count__ - 1) << 2) ) 
            __logical_memory_space_filled__ = 1 ;
        
        __local_address__ += 4 ;
    }

    printf("\n +++ ARRAY CREATION +++\n") ;
    printf("    > SCOPE : createArr\n") ;
    printf("    > VARIABLE TYPE : %s\n", type) ;
    printf("    > VARIABLE NAME : %s\n", sym) ;
    printf("    > FIRST/LAST LOCAL ADDRESS : %d / %d\n", first_local_add, last_local_add) ;
    printf("    > FIRST/LAST VIRTUAL ADDRESS : %d / %d\n", first_virt_add, last_virt_add) ;
    printf("    > GLOBAL STACK SIZE : %d\n", __global_stack__.size()) ;
    printf("    > FREE FRAMES (BEFORE) : %d\n", holes_size_before) ;
    printf("    > FREE FRAMES (AFTER) : %d\n", __holes__.size()) ;

    return ;
}

void assignArr ( const char * sym , uint32_t idx , const char * value ) {
    if ( ! value || ! strlen(value) )  return ;
    if ( ! isValidIdentifier(sym) )  {
        error("(assignArr) IDENTIFIER ERROR", "Identifier is incompatible to the MMU.") ;
        cleanAndExit() ;
    }

    uint32_t last_elem_ref = stackReferenceVisible(sym) ;
    if ( ! last_elem_ref ) {
        error("(assignArr) REFERENCE ERROR", "Identifier is not bound to any variable in the (visible) scope.") ;
        cleanAndExit() ;
    }
    if ( ! __global_stack__[last_elem_ref]._arrel_flag_param_count ) {
        error("(assignArr) USE ERROR", "assignArr function cannot be used to assign non-array elements.") ;
        cleanAndExit() ;
    }
    if ( __global_stack__[last_elem_ref]._array_idx < idx ) {
        error("(assignArr) INDEX ERROR", "Index of the array is not less than its size.") ;
        cleanAndExit() ;
    }

    uint32_t reference = last_elem_ref - (__global_stack__[last_elem_ref]._array_idx - idx) ;
    uint32_t local_add = __global_stack__[reference]._local_add ;
    uint32_t virt_add = (local_add >> 2) ;

    uint32_t word ;


    pthread_mutex_lock(&__compaction_lock__) ;

    readKthWord(virt_add, &word) ;

    uint8_t data_type = (word >> 30) ;
    uint32_t frame_no = ((word << 2) >> 2) ;

    if ( data_type == 0 ) {
        uint32_t len = strlen(value) ;
        if ( len > 11 || (len == 11 && value[0] != '-') ) {
            error("(assignArr) ASSIGNMENT ERROR", "Variable with int data-type is not assigned correctly.") ;
            pthread_mutex_unlock(&__compaction_lock__) ;
            cleanAndExit() ;
        }

        for ( uint32_t it = 1 ; it < len ; it ++ ) {
            if ( value[it] > '9' || value[it] < '0' ) {
                error("(assignArr) ASSIGNMENT ERROR", "Variable with int data-type is not assigned correctly.") ;
                pthread_mutex_unlock(&__compaction_lock__) ;
                cleanAndExit() ;
            }
        }

        if ( (value[0] > '9' || value[0] < '0') && value[0] != '-' ) {
            error("(assignArr) ASSIGNMENT ERROR", "Variable with int data-type is not assigned correctly.") ;
            pthread_mutex_unlock(&__compaction_lock__) ;
            cleanAndExit() ;
        }
        if ( value[0] == '-' && len == 1 ) {
            error("(assignArr) ASSIGNMENT ERROR", "Variable with int data-type is not assigned correctly.") ;
            pthread_mutex_unlock(&__compaction_lock__) ;
            cleanAndExit() ;
        }

        int64_t test_numeric_value = atoll(value) ;
        if ( test_numeric_value < INT32_MIN || test_numeric_value > INT32_MAX ) {
            error("(assignArr) RANGE ERROR", "Variable with int data-type is not assigned with a 32-bit integer.") ;
            pthread_mutex_unlock(&__compaction_lock__) ;
            cleanAndExit() ;
        }

        int32_t numeric_value = atoi(value) ;
        writeKthWord(frame_no, numeric_value) ;

        printf("\n +++ ARRAY-ELEMENT ASSIGNMENT +++\n") ;
        printf("    > SCOPE : assignArr\n") ;
        printf("    > VARIABLE TYPE : %s\n", dataType(data_type)) ;
        printf("    > VARIABLE NAME : %s[%d]\n", sym, idx) ;
        printf("    > VARIABLE VALUE : %d\n", numeric_value) ;
        printf("    > LOCAL ADDRESS : %d\n", local_add) ;
        printf("    > VIRTUAL ADDRESS : %d\n", virt_add) ;
        printf("    > PHYSICAL FRAME NO. : %d\n", frame_no) ;

        pthread_mutex_unlock(&__compaction_lock__) ;
        return ;
    }

    else if ( data_type == 1 ) {
        uint32_t len = strlen(value) ;
        if ( len > 8 || (len == 8 && value[0] != '-') ) {
            error("(assignArr) ASSIGNMENT ERROR", "Variable with med_int data-type is not assigned correctly.") ;
            pthread_mutex_unlock(&__compaction_lock__) ;
            cleanAndExit() ;
        }

        for ( uint32_t it = 1 ; it < len ; it ++ ) {
            if ( value[it] > '9' || value[it] < '0' ) {
                error("(assignArr) ASSIGNMENT ERROR", "Variable with med_int data-type is not assigned correctly.") ;
                pthread_mutex_unlock(&__compaction_lock__) ;
                cleanAndExit() ;
            }
        }

        if ( (value[0] > '9' || value[0] < '0') && value[0] != '-' ) {
            error("(assignArr) ASSIGNMENT ERROR", "Variable with med_int data-type is not assigned correctly.") ;
            pthread_mutex_unlock(&__compaction_lock__) ;
            cleanAndExit() ;
        }
        if ( value[0] == '-' && len == 1 ) {
            error("(assignArr) ASSIGNMENT ERROR", "Variable with med_int data-type is not assigned correctly.") ;
            pthread_mutex_unlock(&__compaction_lock__) ;
            cleanAndExit() ;
        }

        int32_t numeric_value = atoi(value) ;
        int32_t max_limit_3B = (1 << 23) ;
        if ( numeric_value < (-1 * max_limit_3B) || numeric_value > max_limit_3B - 1 ) {
            error("(assignArr) RANGE ERROR", "Variable with med_int data-type is not assigned with a 24-bit integer.") ;
            pthread_mutex_unlock(&__compaction_lock__) ;
            cleanAndExit() ;
        }
        
        writeKthWord(frame_no, numeric_value) ;

        printf("\n +++ ARRAY-ELEMENT ASSIGNMENT +++\n") ;
        printf("    > SCOPE : assignArr\n") ;
        printf("    > VARIABLE TYPE : %s\n", dataType(data_type)) ;
        printf("    > VARIABLE NAME : %s[%d]\n", sym, idx) ;
        printf("    > VARIABLE VALUE : %d\n", numeric_value) ;
        printf("    > LOCAL ADDRESS : %d\n", local_add) ;
        printf("    > VIRTUAL ADDRESS : %d\n", virt_add) ;
        printf("    > PHYSICAL FRAME NO. : %d\n", frame_no) ;

        pthread_mutex_unlock(&__compaction_lock__) ;
        return ;
    }

    else if ( data_type == 2 ) {
        if ( strlen(value) > 1 ) {
            error("(assignArr) ASSIGNMENT ERROR", "Variable with char data-type is not assigned correctly.") ;
            pthread_mutex_unlock(&__compaction_lock__) ;
            cleanAndExit() ;
        }

        writeKthWord(frame_no, value[0]) ;

        printf("\n +++ ARRAY-ELEMENT ASSIGNMENT +++\n") ;
        printf("    > SCOPE : assignArr\n") ;
        printf("    > VARIABLE TYPE : %s\n", dataType(data_type)) ;
        printf("    > VARIABLE NAME : %s[%d]\n", sym, idx) ;
        printf("    > VARIABLE VALUE : %c\n", value[0]) ;
        printf("    > LOCAL ADDRESS : %d\n", local_add) ;
        printf("    > VIRTUAL ADDRESS : %d\n", virt_add) ;
        printf("    > PHYSICAL FRAME NO. : %d\n", frame_no) ;

        pthread_mutex_unlock(&__compaction_lock__) ;
        return ;
    }

    else if ( data_type == 3 ) {
        if ( strcmp(value, "true") && strcmp(value, "false") ) {
            error("(assignArr) ASSIGNMENT ERROR", "Variable with bool data-type is not assigned correctly.") ;
            pthread_mutex_unlock(&__compaction_lock__) ;
            cleanAndExit() ;
        }

        if ( strcmp(value, "true") == 0 )    writeKthWord(frame_no, 1) ;
        else    writeKthWord(frame_no, 0) ;

        printf("\n +++ ARRAY-ELEMENT ASSIGNMENT +++\n") ;
        printf("    > SCOPE : assignArr\n") ;
        printf("    > VARIABLE TYPE : %s\n", dataType(data_type)) ;
        printf("    > VARIABLE NAME : %s[%d]\n", sym, idx) ;
        
        if ( strcmp(value, "true") == 0 )   printf("    > VARIABLE VALUE : 1\n") ;
        else    printf("    > VARIABLE VALUE : 0\n") ;

        printf("    > LOCAL ADDRESS : %d\n", local_add) ;
        printf("    > VIRTUAL ADDRESS : %d\n", virt_add) ;
        printf("    > PHYSICAL FRAME NO. : %d\n", frame_no) ;

        pthread_mutex_unlock(&__compaction_lock__) ;
        return ;
    }

    else {
        error("(assignArr) MEMORY FAULT", "Page table got corrupted.") ;
        pthread_mutex_unlock(&__compaction_lock__) ;
        cleanAndExit() ;
    }

    return ;
}

void burstAssignArr ( const char * sym , const char * value ) {
    if ( ! value || ! strlen(value) )  return ;
    if ( ! isValidIdentifier(sym) )  {
        error("(burstAssignArr) IDENTIFIER ERROR", "Identifier is incompatible to the MMU.") ;
        cleanAndExit() ;
    }
    
    uint32_t reference = stackReferenceVisible(sym) ;
    if ( ! reference ) {
        error("(burstAssignArr) REFERENCE ERROR", "Identifier is not bound to any variable in the (visible) scope.") ;
        cleanAndExit() ;
    }
    if ( ! __global_stack__[reference]._arrel_flag_param_count ) {
        error("(burstAssignArr) USE ERROR", "burstAssignArr function cannot be used to assign non-array elements.") ;
        cleanAndExit() ;
    }
    
    bool filled = false ;
    int32_t numeric_value ;
    uint8_t data_type ;
    uint32_t first_local_add = __global_stack__[reference]._local_add ;
    uint32_t first_virt_add = (first_local_add >> 2) ;
    uint32_t last_local_add ;
    uint32_t last_virt_add ;

    while ( ! strcmp(__global_stack__[reference]._sym_name, sym) ) {
        uint32_t local_add = __global_stack__[reference]._local_add ;
        uint32_t virt_add = (local_add >> 2) ;
        uint32_t word ;

        last_local_add = local_add ;
        last_virt_add = virt_add ;


        pthread_mutex_lock(&__compaction_lock__) ;

        readKthWord(virt_add, &word) ;

        data_type = (word >> 30) ;
        uint32_t frame_no = ((word << 2) >> 2) ;

        if ( data_type == 0 ) {
            if ( filled ) {
                writeKthWord(frame_no, numeric_value) ;
                pthread_mutex_unlock(&__compaction_lock__) ;
                reference -- ;
                continue ;
            }

            uint32_t len = strlen(value) ;
            if ( len > 11 || (len == 11 && value[0] != '-') ) {
                error("(burstAssignArr) ASSIGNMENT ERROR", "Variable with int data-type is not assigned correctly.") ;
                pthread_mutex_unlock(&__compaction_lock__) ;
                cleanAndExit() ;
            }

            for ( uint32_t it = 1 ; it < len ; it ++ ) {
                if ( value[it] > '9' || value[it] < '0' ) {
                    error("(burstAssignArr) ASSIGNMENT ERROR", "Variable with int data-type is not assigned correctly.") ;
                    pthread_mutex_unlock(&__compaction_lock__) ;
                    cleanAndExit() ;
                }
            }

            if ( (value[0] > '9' || value[0] < '0') && value[0] != '-' ) {
                error("(burstAssignArr) ASSIGNMENT ERROR", "Variable with int data-type is not assigned correctly.") ;
                pthread_mutex_unlock(&__compaction_lock__) ;
                cleanAndExit() ;
            }
            if ( value[0] == '-' && len == 1 ) {
                error("(burstAssignArr) ASSIGNMENT ERROR", "Variable with int data-type is not assigned correctly.") ;
                pthread_mutex_unlock(&__compaction_lock__) ;
                cleanAndExit() ;
            }

            int64_t test_numeric_value = atoll(value) ;
            if ( test_numeric_value < INT32_MIN || test_numeric_value > INT32_MAX ) {
                error("(burstAssignArr) RANGE ERROR", "Variable with int data-type is not assigned with a 32-bit integer.") ;
                pthread_mutex_unlock(&__compaction_lock__) ;
                cleanAndExit() ;
            }

            numeric_value = atoi(value) ;
            writeKthWord(frame_no, numeric_value) ;
        }

        else if ( data_type == 1 ) {
            if ( filled ) {
                writeKthWord(frame_no, numeric_value) ;
                pthread_mutex_unlock(&__compaction_lock__) ;
                reference -- ;
                continue ;
            }

            uint32_t len = strlen(value) ;
            if ( len > 8 || (len == 8 && value[0] != '-') ) {
                error("(burstAssignArr) ASSIGNMENT ERROR", "Variable with med_int data-type is not assigned correctly.") ;
                pthread_mutex_unlock(&__compaction_lock__) ;
                cleanAndExit() ;
            }

            for ( uint32_t it = 1 ; it < len ; it ++ ) {
                if ( value[it] > '9' || value[it] < '0' ) {
                    error("(burstAssignArr) ASSIGNMENT ERROR", "Variable with med_int data-type is not assigned correctly.") ;
                    pthread_mutex_unlock(&__compaction_lock__) ;
                    cleanAndExit() ;
                }
            }

            if ( (value[0] > '9' || value[0] < '0') && value[0] != '-' ) {
                error("(burstAssignArr) ASSIGNMENT ERROR", "Variable with med_int data-type is not assigned correctly.") ;
                pthread_mutex_unlock(&__compaction_lock__) ;
                cleanAndExit() ;
            }
            if ( value[0] == '-' && len == 1 ) {
                error("(burstAssignArr) ASSIGNMENT ERROR", "Variable with med_int data-type is not assigned correctly.") ;
                pthread_mutex_unlock(&__compaction_lock__) ;
                cleanAndExit() ;
            }

            numeric_value = atoi(value) ;
            int32_t max_limit_3B = (1 << 23) ;
            if ( numeric_value < (-1 * max_limit_3B) || numeric_value > max_limit_3B - 1 ) {
                error("(burstAssignArr) RANGE ERROR", "Variable with med_int data-type is not assigned with a 24-bit integer.") ;
                pthread_mutex_unlock(&__compaction_lock__) ;
                cleanAndExit() ;
            }
            
            writeKthWord(frame_no, numeric_value) ;
        }

        else if ( data_type == 2 ) {
            if ( ! filled && strlen(value) > 1 ) {
                error("(burstAssignArr) ASSIGNMENT ERROR", "Variable with char data-type is not assigned correctly.") ;
                pthread_mutex_unlock(&__compaction_lock__) ;
                cleanAndExit() ;
            }
            writeKthWord(frame_no, value[0]) ;
        }

        else if ( data_type == 3 ) {
            if ( ! filled ) {
                if ( strcmp(value, "true") == 0 )       numeric_value = 1 ;
                else if ( strcmp(value, "false") == 0 ) numeric_value = 0 ;
                else {
                    error("(burstAssignArr) ASSIGNMENT ERROR", "Variable with bool data-type is not assigned correctly.") ;
                    pthread_mutex_unlock(&__compaction_lock__) ;
                    cleanAndExit() ;
                }
            }
            writeKthWord(frame_no, numeric_value) ;
        }

        else {
            error("(burstAssignArr) MEMORY FAULT", "Page table got corrupted.") ;
            pthread_mutex_unlock(&__compaction_lock__) ;
            cleanAndExit() ;
        }

        pthread_mutex_unlock(&__compaction_lock__) ;

        
        filled = true ;
        reference -- ;
    }

    printf("\n +++ ARRAY ASSIGNMENT +++\n") ;
    printf("    > SCOPE : burstAssignArr\n") ;
    printf("    > VARIABLE TYPE : %s\n", dataType(data_type)) ;
    printf("    > VARIABLE NAME : %s\n", sym) ;
    printf("    > VARIABLE VALUE : %d\n", numeric_value) ;
    printf("    > FIRST/LAST LOCAL ADDRESS : %d / %d\n", first_local_add, last_local_add) ;
    printf("    > FIRST/LAST VIRTUAL ADDRESS : %d / %d\n", first_virt_add, last_virt_add) ;

    return ;
}


void freeElem ( const char * sym ) {
    if ( ! isValidIdentifier(sym) )  {
        error("(freeElem) IDENTIFIER ERROR", "Identifier is incompatible to the MMU.") ;
        cleanAndExit() ;
    }

    if ( ! __global_stack__.size() ) {
        error("(freeElem) STACK ERROR", "Global stack is empty.") ;
        cleanAndExit() ;
    }
    if ( strcmp(__global_stack__.back()._sym_name, sym) ) {
        error("(freeElem) STACK ERROR", "Stack frame can be cleared only from top to bottom.") ;
        cleanAndExit() ;
    }

    uint32_t gbl_stack_size_bef = __global_stack__.size() ;

    while ( __global_stack__.size() && ! strcmp(__global_stack__.back()._sym_name, sym) ) {
        uint32_t local_add = __global_stack__.back()._local_add ;
        __global_stack__.pop_back() ;

        
        pthread_mutex_lock(&__compaction_lock__) ;
        pthread_mutex_lock(&__marknsweep_lock__) ;
        
        clearPageTableEntry((local_add >> 2)) ;
        __local_address__ -= 4 ;
        
        pthread_mutex_unlock(&__marknsweep_lock__) ;
        pthread_mutex_unlock(&__compaction_lock__) ;
    }

    __logical_memory_space_filled__ = 0 ;

    printf("\n +++ FREE ELEMENT +++\n") ;
    printf("    > SCOPE : freeElem\n") ;
    printf("    > VARIABLE NAME : %s\n", sym) ;
    printf("    > GLOBAL STACK SIZE (BEFORE) : %d\n", gbl_stack_size_bef) ;
    printf("    > GLOBAL STACK SIZE (AFTER) : %d\n", __global_stack__.size()) ;
    
    return ;
}


void createParam ( const char * type , const char * sym ) {
    printf("\n +++ CREATE FUNCTION PARAMETER +++\n") ;
    printf("    > SCOPE : createParam\n") ;
    printf("    > DEFINITION : %s %s\n", type, sym) ;
    printf("    > A wrapper that calls createVar to create a scalar parameter.\n") ;
    createVar(type, sym) ;
    return ;
}

void assignParam ( const char * sym , const char * value ) {
    printf("\n +++ ASSIGN FUNCTION PARAMETER +++\n") ;
    printf("    > SCOPE : assignParam\n") ;
    printf("    > DEFINITION : %s = %s\n", sym, value) ;
    printf("    > A wrapper that calls assignVar to assign value to a scalar parameter.\n") ;
    assignVar(sym, value) ;
    return ;
}


void createArrParam ( const char * type , const char * sym , uint32_t size ) {
    printf("\n +++ CREATE FUNCTION PARAMETER +++\n") ;
    printf("    > SCOPE : createArrParam\n") ;
    printf("    > DEFINITION : %s %s[%d]\n", type, sym, size) ;
    printf("    > A wrapper that calls createArr to create a vector parameter.\n") ;
    createArr(type, sym, size) ;
    return ;
}


int32_t getInt ( const char * sym ) {
    if ( ! isValidIdentifier(sym) )  {
        error("(getInt) IDENTIFIER ERROR", "Identifier is incompatible to the MMU.") ;
        cleanAndExit() ;
    }
    
    uint32_t reference = stackReferenceVisible(sym) ;
    if ( ! reference ) {
        error("(getInt) REFERENCE ERROR", "Identifier is not bound to any variable in the (visible) scope.") ;
        cleanAndExit() ;
    }
    if ( __global_stack__[reference]._arrel_flag_param_count ) {
        error("(getInt) USE ERROR", "get<type> function cannot be used to get values of array elements.") ;
        cleanAndExit() ;
    }

    uint32_t local_add = __global_stack__[reference]._local_add ;
    uint32_t virt_add = (local_add >> 2) ;
    uint32_t word ;

    pthread_mutex_lock(&__compaction_lock__) ;

    readKthWord(virt_add, &word) ;
    uint8_t data_type = (word >> 30) ;

    if ( data_type != 0 ) {
        error("(getInt) TYPE ERROR", "getInt cannot be used to get the value of a non-int data type.") ;
        pthread_mutex_unlock(&__compaction_lock__) ;
        cleanAndExit() ;
    }

    uint32_t frame_no = ((word << 2) >> 2) ;
    int32_t value ;
    readKthWord(frame_no, &value) ;

    printf("\n +++ GET VARIABLE VALUE +++\n") ;
    printf("    > SCOPE : getInt\n") ;
    printf("    > VARIABLE TYPE : int\n") ;
    printf("    > VARIABLE NAME : %s\n", sym) ;
    printf("    > RETURN VALUE : %d\n", value) ;
    
    pthread_mutex_unlock(&__compaction_lock__) ;

    return value ;
}

int32_t getMedInt ( const char * sym ) {
    if ( ! isValidIdentifier(sym) )  {
        error("(getMedInt) IDENTIFIER ERROR", "Identifier is incompatible to the MMU.") ;
        cleanAndExit() ;
    }
    
    uint32_t reference = stackReferenceVisible(sym) ;
    if ( ! reference ) {
        error("(getMedInt) REFERENCE ERROR", "Identifier is not bound to any variable in the (visible) scope.") ;
        cleanAndExit() ;
    }
    if ( __global_stack__[reference]._arrel_flag_param_count ) {
        error("(getMedInt) USE ERROR", "get<type> function cannot be used to get values of array elements.") ;
        cleanAndExit() ;
    }

    uint32_t local_add = __global_stack__[reference]._local_add ;
    uint32_t virt_add = (local_add >> 2) ;
    uint32_t word ;

    pthread_mutex_lock(&__compaction_lock__) ;

    readKthWord(virt_add, &word) ;
    uint8_t data_type = (word >> 30) ;

    if ( data_type != 1 ) {
        error("(getMedInt) TYPE ERROR", "getMedInt cannot be used to get the value of a non-med-int data type.") ;
        pthread_mutex_unlock(&__compaction_lock__) ;
        cleanAndExit() ;
    }

    uint32_t frame_no = ((word << 2) >> 2) ;
    int32_t value ;
    readKthWord(frame_no, &value) ;

    printf("\n +++ GET VARIABLE VALUE +++\n") ;
    printf("    > SCOPE : getMedInt\n") ;
    printf("    > VARIABLE TYPE : med_int\n") ;
    printf("    > VARIABLE NAME : %s\n", sym) ;
    printf("    > RETURN VALUE : %d\n", value) ;
    
    pthread_mutex_unlock(&__compaction_lock__) ;

    return value ;
}

uint8_t getChar ( const char * sym ) {
    if ( ! isValidIdentifier(sym) )  {
        error("(getChar) IDENTIFIER ERROR", "Identifier is incompatible to the MMU.") ;
        cleanAndExit() ;
    }
    
    uint32_t reference = stackReferenceVisible(sym) ;
    if ( ! reference ) {
        error("(getChar) REFERENCE ERROR", "Identifier is not bound to any variable in the (visible) scope.") ;
        cleanAndExit() ;
    }
    if ( __global_stack__[reference]._arrel_flag_param_count ) {
        error("(getChar) USE ERROR", "get<type> function cannot be used to get values of array elements.") ;
        cleanAndExit() ;
    }

    uint32_t local_add = __global_stack__[reference]._local_add ;
    uint32_t virt_add = (local_add >> 2) ;
    uint32_t word ;

    pthread_mutex_lock(&__compaction_lock__) ;

    readKthWord(virt_add, &word) ;
    uint8_t data_type = (word >> 30) ;

    if ( data_type != 2 ) {
        error("(getChar) TYPE ERROR", "getChar cannot be used to get the value of a non-char data type.") ;
        pthread_mutex_unlock(&__compaction_lock__) ;
        cleanAndExit() ;
    }

    uint32_t frame_no = ((word << 2) >> 2) ;
    uint32_t word_value ;
    readKthWord(frame_no, &word_value) ;

    printf("\n +++ GET VARIABLE VALUE +++\n") ;
    printf("    > SCOPE : getChar\n") ;
    printf("    > VARIABLE TYPE : char\n") ;
    printf("    > VARIABLE NAME : %s\n", sym) ;
    printf("    > RETURN VALUE : %d\n", word_value) ;
    
    pthread_mutex_unlock(&__compaction_lock__) ;

    uint8_t value = (uint8_t)word_value ;
    return value ;
}

uint8_t getBool ( const char * sym ) {
    if ( ! isValidIdentifier(sym) )  {
        error("(getBool) IDENTIFIER ERROR", "Identifier is incompatible to the MMU.") ;
        cleanAndExit() ;
    }
    
    uint32_t reference = stackReferenceVisible(sym) ;
    if ( ! reference ) {
        error("(getBool) REFERENCE ERROR", "Identifier is not bound to any variable in the (visible) scope.") ;
        cleanAndExit() ;
    }
    if ( __global_stack__[reference]._arrel_flag_param_count ) {
        error("(getBool) USE ERROR", "get<type> function cannot be used to get values of array elements.") ;
        cleanAndExit() ;
    }

    uint32_t local_add = __global_stack__[reference]._local_add ;
    uint32_t virt_add = (local_add >> 2) ;
    uint32_t word ;

    pthread_mutex_lock(&__compaction_lock__) ;

    readKthWord(virt_add, &word) ;
    uint8_t data_type = (word >> 30) ;

    if ( data_type != 3 ) {
        error("(getBool) TYPE ERROR", "getBool cannot be used to get the value of a non-bool data type.") ;
        pthread_mutex_unlock(&__compaction_lock__) ;
        cleanAndExit() ;
    }

    uint32_t frame_no = ((word << 2) >> 2) ;
    uint32_t word_value ;
    readKthWord(frame_no, &word_value) ;

    printf("\n +++ GET VARIABLE VALUE +++\n") ;
    printf("    > SCOPE : getBool\n") ;
    printf("    > VARIABLE TYPE : bool\n") ;
    printf("    > VARIABLE NAME : %s\n", sym) ;
    printf("    > RETURN VALUE : %d\n", word_value) ;
    
    pthread_mutex_unlock(&__compaction_lock__) ;

    uint8_t value = (uint8_t)word_value ;
    return value ;
}


int32_t getArrInt ( const char * sym , uint32_t idx ) {
    if ( ! isValidIdentifier(sym) )  {
        error("(getArrInt) IDENTIFIER ERROR", "Identifier is incompatible to the MMU.") ;
        cleanAndExit() ;
    }
    
    uint32_t last_elem_ref = stackReferenceVisible(sym) ;
    if ( ! last_elem_ref ) {
        error("(getArrInt) REFERENCE ERROR", "Identifier is not bound to any variable in the (visible) scope.") ;
        cleanAndExit() ;
    }
    if ( ! __global_stack__[last_elem_ref]._arrel_flag_param_count ) {
        error("(getArrInt) USE ERROR", "getArr<type> function cannot be used to get values of non-array elements.") ;
        cleanAndExit() ;
    }
    if ( __global_stack__[last_elem_ref]._array_idx < idx ) {
        error("(getArrInt) INDEX ERROR", "Index of the array is not less than its size.") ;
        cleanAndExit() ;
    }

    uint32_t reference = last_elem_ref - (__global_stack__[last_elem_ref]._array_idx - idx) ;
    uint32_t local_add = __global_stack__[reference]._local_add ;
    uint32_t virt_add = (local_add >> 2) ;
    uint32_t word ;


    pthread_mutex_lock(&__compaction_lock__) ;

    readKthWord(virt_add, &word) ;
    uint8_t data_type = (word >> 30) ;

    if ( data_type != 0 ) {
        error("(getArrInt) TYPE ERROR", "getArrInt cannot be used to get the value of a non-int data type.") ;
        pthread_mutex_unlock(&__compaction_lock__) ;
        cleanAndExit() ;
    }

    uint32_t frame_no = ((word << 2) >> 2) ;
    int32_t value ;
    readKthWord(frame_no, &value) ;

    printf("\n +++ GET VARIABLE VALUE +++\n") ;
    printf("    > SCOPE : getArrInt\n") ;
    printf("    > VARIABLE TYPE : array(int)\n") ;
    printf("    > VARIABLE NAME : %s\n", sym) ;
    printf("    > ARRAY INDEX : %d\n", idx) ;
    printf("    > RETURN VALUE : %d\n", value) ;
    
    pthread_mutex_unlock(&__compaction_lock__) ;

    return value ;
}

int32_t getArrMedInt ( const char * sym , uint32_t idx ) {
    if ( ! isValidIdentifier(sym) )  {
        error("(getArrMedInt) IDENTIFIER ERROR", "Identifier is incompatible to the MMU.") ;
        cleanAndExit() ;
    }
    
    uint32_t last_elem_ref = stackReferenceVisible(sym) ;
    if ( ! last_elem_ref ) {
        error("(getArrMedInt) REFERENCE ERROR", "Identifier is not bound to any variable in the (visible) scope.") ;
        cleanAndExit() ;
    }
    if ( ! __global_stack__[last_elem_ref]._arrel_flag_param_count ) {
        error("(getArrMedInt) USE ERROR", "getArr<type> function cannot be used to get values of non-array elements.") ;
        cleanAndExit() ;
    }
    if ( __global_stack__[last_elem_ref]._array_idx < idx ) {
        error("(getArrMedInt) INDEX ERROR", "Index of the array is not less than its size.") ;
        cleanAndExit() ;
    }

    uint32_t reference = last_elem_ref - (__global_stack__[last_elem_ref]._array_idx - idx) ;
    uint32_t local_add = __global_stack__[reference]._local_add ;
    uint32_t virt_add = (local_add >> 2) ;
    uint32_t word ;

    pthread_mutex_lock(&__compaction_lock__) ;

    readKthWord(virt_add, &word) ;
    uint8_t data_type = (word >> 30) ;

    if ( data_type != 1 ) {
        error("(getArrMedInt) TYPE ERROR", "getArrMedInt cannot be used to get the value of a non-med-int data type.") ;
        pthread_mutex_unlock(&__compaction_lock__) ;
        cleanAndExit() ;
    }

    uint32_t frame_no = ((word << 2) >> 2) ;
    int32_t value ;
    readKthWord(frame_no, &value) ;

    printf("\n +++ GET VARIABLE VALUE +++\n") ;
    printf("    > SCOPE : getArrMedInt\n") ;
    printf("    > VARIABLE TYPE : array(med_int)\n") ;
    printf("    > VARIABLE NAME : %s\n", sym) ;
    printf("    > ARRAY INDEX : %d\n", idx) ;
    printf("    > RETURN VALUE : %d\n", value) ;
    
    pthread_mutex_unlock(&__compaction_lock__) ;

    return value ;
}

uint8_t getArrChar ( const char * sym , uint32_t idx ) {
    if ( ! isValidIdentifier(sym) )  {
        error("(getArrChar) IDENTIFIER ERROR", "Identifier is incompatible to the MMU.") ;
        cleanAndExit() ;
    }
    
    uint32_t last_elem_ref = stackReferenceVisible(sym) ;
    if ( ! last_elem_ref ) {
        error("(getArrChar) REFERENCE ERROR", "Identifier is not bound to any variable in the (visible) scope.") ;
        cleanAndExit() ;
    }
    if ( ! __global_stack__[last_elem_ref]._arrel_flag_param_count ) {
        error("(getArrChar) USE ERROR", "getArr<type> function cannot be used to get values of non-array elements.") ;
        cleanAndExit() ;
    }
    if ( __global_stack__[last_elem_ref]._array_idx < idx ) {
        error("(getArrChar) INDEX ERROR", "Index of the array is not less than its size.") ;
        cleanAndExit() ;
    }

    uint32_t reference = last_elem_ref - (__global_stack__[last_elem_ref]._array_idx - idx) ;
    uint32_t local_add = __global_stack__[reference]._local_add ;
    uint32_t virt_add = (local_add >> 2) ;
    uint32_t word ;


    pthread_mutex_lock(&__compaction_lock__) ;

    readKthWord(virt_add, &word) ;
    uint8_t data_type = (word >> 30) ;

    if ( data_type != 2 ) {
        error("(getArrChar) TYPE ERROR", "getArrChar cannot be used to get the value of a non-char data type.") ;
        pthread_mutex_unlock(&__compaction_lock__) ;
        cleanAndExit() ;
    }

    uint32_t frame_no = ((word << 2) >> 2) ;
    uint32_t word_value ;
    readKthWord(frame_no, &word_value) ;
    word_value = (word_value & ((1 << 8) - 1)) ;

    printf("\n +++ GET VARIABLE VALUE +++\n") ;
    printf("    > SCOPE : getArrChar\n") ;
    printf("    > VARIABLE TYPE : array(char)\n") ;
    printf("    > VARIABLE NAME : %s\n", sym) ;
    printf("    > ARRAY INDEX : %d\n", idx) ;
    printf("    > RETURN VALUE : %d\n", word_value) ;
    
    pthread_mutex_unlock(&__compaction_lock__) ;

    uint8_t value = (uint8_t)word_value ;
    return value ;
}

uint8_t getArrBool ( const char * sym , uint32_t idx ) {
    if ( ! isValidIdentifier(sym) )  {
        error("(getArrBool) IDENTIFIER ERROR", "Identifier is incompatible to the MMU.") ;
        cleanAndExit() ;
    }
    
    uint32_t last_elem_ref = stackReferenceVisible(sym) ;
    if ( ! last_elem_ref ) {
        error("(getArrBool) REFERENCE ERROR", "Identifier is not bound to any variable in the (visible) scope.") ;
        cleanAndExit() ;
    }
    if ( ! __global_stack__[last_elem_ref]._arrel_flag_param_count ) {
        error("(getArrBool) USE ERROR", "getArr<type> function cannot be used to get values of non-array elements.") ;
        cleanAndExit() ;
    }
    if ( __global_stack__[last_elem_ref]._array_idx < idx ) {
        error("(getArrBool) INDEX ERROR", "Index of the array is not less than its size.") ;
        cleanAndExit() ;
    }

    uint32_t reference = last_elem_ref - (__global_stack__[last_elem_ref]._array_idx - idx) ;
    uint32_t local_add = __global_stack__[reference]._local_add ;
    uint32_t virt_add = (local_add >> 2) ;
    uint32_t word ;
    

    pthread_mutex_lock(&__compaction_lock__) ;

    readKthWord(virt_add, &word) ;
    uint8_t data_type = (word >> 30) ;

    if ( data_type != 2 ) {
        error("(getArrBool) TYPE ERROR", "getArrBool cannot be used to get the value of a non-bool data type.") ;
        pthread_mutex_unlock(&__compaction_lock__) ;
        cleanAndExit() ;
    }

    uint32_t frame_no = ((word << 2) >> 2) ;
    uint32_t word_value ;
    readKthWord(frame_no, &word_value) ;
    word_value = (word_value & ((1 << 8) - 1)) ;

    printf("\n +++ GET VARIABLE VALUE +++\n") ;
    printf("    > SCOPE : getArrBool\n") ;
    printf("    > VARIABLE TYPE : array(bool)\n") ;
    printf("    > VARIABLE NAME : %s\n", sym) ;
    printf("    > ARRAY INDEX : %d\n", idx) ;
    printf("    > RETURN VALUE : %d\n", word_value) ;
    
    pthread_mutex_unlock(&__compaction_lock__) ;

    uint8_t value = (uint8_t)word_value ;
    return value ;
}

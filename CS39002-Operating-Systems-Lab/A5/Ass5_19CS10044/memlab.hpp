
#ifndef __MEMLAB_H__
#define __MEMLAB_H__

#include <cstdlib>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <iostream>

#define FUNCTION_SCOPE  			1
#define LOCAL_SCOPE 				0
#define EMPTY 						0

#define GC_VOLUNTARY				1
#define GC_AUTOMATIC				0

#define	LOG_STRUCTURE_UPDATES		1

#define GC_AUTO_SLEEP_T 			10000   	// 0.01 sec
#define GC_AUTO_COMPAC_GAP  		100000 	   	// 0.1 sec
#define GC_AUTO_MAS_GAP 			100000      // 0.1 sec


using namespace std ;


class Stack ;
class List_U32 ;
class Min_Heap_U32 ;
class Unordered_Set_U32 ;
class hashnode_uint32_t ;


void createMem(uint32_t, uint32_t = GC_AUTO_SLEEP_T, uint32_t = GC_AUTO_COMPAC_GAP, uint32_t = GC_AUTO_MAS_GAP);

void cleanAndExit();
void stopMMU();

void initStackFrame(uint8_t, uint8_t);
void deinitStackFrame();

void createVar(const char *, const char *);
void assignVar(const char *, const char *);

void createArr(const char *, const char *, uint32_t);
void assignArr(const char *, uint32_t, const char *);
void burstAssignArr(const char *, const char *);

void freeElem(const char *);

void gc_run(uint8_t);

void createParam(const char *, const char *);
void assignParam(const char *, const char *);

void createArrParam(const char *, const char *, uint32_t);

int32_t getInt(const char *);
uint8_t getChar(const char *);
int32_t getMedInt(const char *);
uint8_t getBool(const char *);

int32_t getArrInt(const char *, uint32_t);
uint8_t getArrChar(const char *, uint32_t);
int32_t getArrMedInt(const char *, uint32_t);
uint8_t getArrBool(const char *, uint32_t);


typedef struct _stack_obj
{
	char _sym_name [31];
	uint32_t _local_add;
	uint32_t _array_idx;
	uint8_t _arrel_flag_param_count;
}
stack_obj_t;

class Stack {
	private :
		void error ( const char * type , const char * msg ) {
			printf("\n\n [ %s : %s ]\n\n", type, msg) ;
    		return ;
		}

		uint32_t _max_size ;
		uint32_t _size ;
		stack_obj_t * _stack ;

	public :
		Stack ( ) : _max_size(0) , _size(0) , _stack(NULL) {
			if ( LOG_STRUCTURE_UPDATES ) {
				printf("\n { (Stack::Stack) Global stack created. } \n") ;
			}
		}

		void init ( uint32_t max_size ) {
			_max_size = max_size ;
			_stack = (stack_obj_t *) malloc (max_size * sizeof(stack_obj_t)) ;
			if ( ! _stack ) {
				error("(Stack::init) MEMORY ERROR", "Memory could not be allocated for the stack.") ;
				cleanAndExit() ;
			}
			if ( LOG_STRUCTURE_UPDATES ) {
				printf("\n { (Stack::init) %ld bytes of memory allocated for the global stack. } \n", 
					max_size * sizeof(stack_obj_t)) ;
			}
		}

		void clear ( ) { 
			if ( ! _stack ) {
				error("(Stack::clear) STACK ERROR", "Stack not initialized.") ;
				cleanAndExit() ;
			}
			if ( LOG_STRUCTURE_UPDATES && _size ) {
				printf("\n { (Stack::clear) Global stack cleared. } \n") ;
			}
			_size = 0 ; 
		}

		uint32_t size ( ) const { return _size ; }

		stack_obj_t & back ( ) {
			if ( ! _stack ) {
				error("(Stack::back) STACK ERROR", "Stack not initialized.") ;
				cleanAndExit() ;
			}
			if ( _size == 0 ) {
				error("(Stack::back) STACK ERROR", "Stack is empty.") ;
				cleanAndExit() ;
			}
			return _stack[_size - 1] ; 
		}

		void pop_back ( ) { 
			if ( ! _stack ) {
				error("(Stack::pop_back) STACK ERROR", "Stack not initialized.") ;
				cleanAndExit() ;
			}
			if ( _size == 0 ) {
				error("(Stack::pop_back) STACK ERROR", "Stack is empty.") ;
				cleanAndExit() ;
			}
			_size -- ;
			if ( LOG_STRUCTURE_UPDATES ) {
				printf("\n { (Stack::pop_back) An element popped off the top of the global stack. } \n") ;
			}
		}

		void push_back ( const stack_obj_t * obj ) {
			if ( ! _stack ) {
				error("(Stack::push_back) STACK ERROR", "Stack not initialized.") ;
				cleanAndExit() ;
			}
			if ( _size == _max_size ) {
				error("(Stack::push_back) MEMORY ERROR", "Stack is full.") ;
				cleanAndExit() ;
			}
			_stack[_size]._array_idx = obj->_array_idx ;
			_stack[_size]._arrel_flag_param_count = obj->_arrel_flag_param_count ;
			_stack[_size]._local_add = obj->_local_add ;
			strcpy(_stack[_size]._sym_name, obj->_sym_name) ;
			_size ++ ;
			if ( LOG_STRUCTURE_UPDATES ) {
				printf("\n { (Stack::push_back) An element pushed on top of the global stack. } \n") ;
			}
		}

		stack_obj_t & operator[] ( uint32_t i ) { 
			if ( ! _stack ) {
				error("(Stack::operator[]) STACK ERROR", "Stack not initialized.") ;
				cleanAndExit() ;
			}
			if ( i >= _size ) {
				error("(Stack::operator[]) INDEX ERROR", "Index of the stack is not less than its size.") ;
				cleanAndExit() ;
			}
			return _stack[i]; 
		}

		void destroy ( ) { 
			if ( _stack ) {
				free(_stack) ;
				if ( LOG_STRUCTURE_UPDATES ) 
					printf("\n { (Stack::destroy) Global stack memory deallocated. } \n") ;
			}
			_stack = NULL ;
		}

		~Stack ( ) { destroy() ; }
} ;

typedef struct _list_uint32
{
	uint32_t _frame_no ;
	struct _list_uint32 * _prev ;
	struct _list_uint32 * _next ;
}
list_uint32_t;

class List_U32 {
	private :
		void error ( const char * type , const char * msg ) const {
			printf("\n\n [ %s : %s ]\n\n", type, msg) ;
    		return ;
		}

		uint32_t _max_size ;
		uint32_t _size ;
		list_uint32_t * _first ;
		list_uint32_t * _last ;
	
	public :
		List_U32 ( ) : _max_size(0) , _size(0) , _first(NULL) , _last(NULL) {
			if ( LOG_STRUCTURE_UPDATES ) {
				printf("\n { (List_U32::List_U32) List of free frames created. } \n") ;
			}
		}

		void init ( uint32_t max_size ) {
			_max_size = max_size ;
			if ( LOG_STRUCTURE_UPDATES ) {
				printf("\n { (List_U32::init) %ld bytes of memory reserved for the list of free frames. } \n", 
					max_size * sizeof(list_uint32_t)) ;
			}
		}

		void clear ( ) { 
			while ( _first ) {
				list_uint32_t * temp = _first->_next ;
				free(_first) ;
				_first = temp ;
			}
			_size = 0 ;
			if ( LOG_STRUCTURE_UPDATES && _last ) {
				printf("\n { (List_U32::clear) List of free frames cleared. } \n") ;
			}
			_last = NULL ;
		}

		uint32_t size ( ) const { return _size ; }

		const list_uint32_t * first ( ) const { return _first ; }

		uint32_t back ( ) const {
			if ( _size == 0 ) {
				error("(List_U32::back) LIST ERROR", "List is empty.") ;
				cleanAndExit() ;
			}
			return _last->_frame_no ;
		}

		void pop_back ( ) {
			if ( _size == 0 ) {
				error("(List_U32::pop_back) LIST ERROR", "List is empty.") ;
				cleanAndExit() ;
			}
			if ( _size == 1 ) {
				free(_first) ;
				_first = _last = NULL ;
			}
			else {
				list_uint32_t * temp = _last->_prev ;
				temp->_next = NULL ;
				free(_last) ;
				_last = temp ;
			}
			_size -- ;
			if ( LOG_STRUCTURE_UPDATES ) {
				printf("\n { (List_U32::pop_back) An element popped off the back of the list. } \n") ;
			}
		}

		void push_front ( uint32_t val ) {
			if ( _size == _max_size ) {
				error("(List_U32::push_front) MEMORY ERROR", "List is full.") ;
				cleanAndExit() ;
			}

			list_uint32_t * new_node = (list_uint32_t *) malloc (sizeof(list_uint32_t)) ;
			if ( ! new_node ) {
				error("(List_U32::push_front) MEMORY ERROR", "Memory could not be allocated.") ;
				cleanAndExit() ;
			}

			if ( _size == 0 ) {
				_first = _last = new_node ;
				_first->_prev = _first->_next = NULL ;
			}
			else {
				new_node->_prev = NULL ;
				new_node->_next = _first ;
				_first->_prev = new_node ;
				_first = new_node ;
			}

			_first->_frame_no = val ;
			_size ++ ;

			if ( LOG_STRUCTURE_UPDATES ) {
				printf("\n { (List_U32::push_front) An element pushed at the front of the list. } \n") ;
			}
		}
		
		~List_U32 ( ) { clear() ; }
} ;

class Min_Heap_U32 {
    private :
		uint32_t max_size ;
		uint32_t size ;
        uint32_t * heap ;

		void error ( const char * type , const char * msg ) const {
			printf("\n\n [ %s : %s ]\n\n", type, msg) ;
    		return ;
		}

        void MinHeapify ( uint32_t current_node )	{
            uint32_t left_child = 2 * current_node ;
            uint32_t right_child = left_child + 1 ;

            if ( left_child > size )    return ;
            if ( right_child > size ) {
                if ( heap[current_node] <= heap[left_child] ) return ;
                uint32_t temp = heap[current_node] ;
                heap[current_node] = heap[left_child] ;
                heap[left_child] = temp ;
                MinHeapify(left_child) ;
                return ;
            }

            if ( heap[current_node] <= heap[left_child] && 
                 heap[current_node] <= heap[right_child] )
                return ;
            
            if ( heap[current_node] > heap[left_child] && 
                 heap[current_node] > heap[right_child] )
            {
                if ( heap[left_child] < heap[right_child] ) {
                    uint32_t temp = heap[current_node] ;
                    heap[current_node] = heap[left_child] ;
                    heap[left_child] = temp ;
                    MinHeapify(left_child) ;
                    return ;
                }
                else {
                    uint32_t temp = heap[current_node] ;
                    heap[current_node] = heap[right_child] ;
                    heap[right_child] = temp ;
                    MinHeapify(right_child) ;
                    return ;
                }
            }

            if ( heap[current_node] > heap[left_child] ) {
                uint32_t temp = heap[current_node] ;
                heap[current_node] = heap[left_child] ;
                heap[left_child] = temp ;
                MinHeapify(left_child) ;
                return ;
            }
            else {
                uint32_t temp = heap[current_node] ;
                heap[current_node] = heap[right_child] ;
                heap[right_child] = temp ;
                MinHeapify(right_child) ;
                return ;
            }
        }

        void BuildHeap ( ) {
			if ( LOG_STRUCTURE_UPDATES ) 
				printf("\n { (Min_Heap_U32::BuildHeap) Building the min heap ... } \n") ;
            for ( uint32_t current_node = size / 2 ; current_node > 0 ; current_node-- ) 
                MinHeapify(current_node) ;
        }

    public :
        Min_Heap_U32 ( const List_U32 & vals ) 
            : max_size(vals.size()) , size(max_size) , heap(new uint32_t[max_size+1])	{
			if ( ! heap ) {
				error("(Min_Heap_U32::Min_Heap_U32) MEMORY ERROR", "Memory could not be allocated for the heap.") ;
				cleanAndExit() ;
			}
            heap[0] = 0 ;
			const list_uint32_t * head = vals.first() ;
            for ( int i = 1 ; i <= size ; i++ ) {
				heap[i] = head->_frame_no ;
				head = head->_next ;
			}
            BuildHeap() ;
			if ( LOG_STRUCTURE_UPDATES ) 
				printf("\n { (Min_Heap_U32::Min_Heap_U32) Min heap created and initialized. } \n") ;
        }
		
        void push ( uint32_t element ) {
			if ( size == max_size ) {
				error("(Min_Heap_U32::push) MEMORY ERROR", "Heap is full.") ;
				cleanAndExit() ;
			}

            heap[++size] = element ;
            int current_node = size ;
            while ( current_node > 1 ) {
                if ( heap[current_node] >= heap[current_node/2] )   break ;
                uint32_t temp = heap[current_node] ;
                heap[current_node] = heap[current_node/2] ;
                heap[current_node/2] = temp ;
                current_node = current_node/2 ;
            }

			if ( LOG_STRUCTURE_UPDATES ) 
				printf("\n { (Min_Heap_U32::push) An element pushed inside the heap. } \n") ;
        }

        void pop ( ) {
			if ( size == 0 ) {
				error("(Min_Heap_U32::pop) HEAP ERROR", "Heap is empty.") ;
				cleanAndExit() ;
			}

            heap[1] = heap[size] ;
            size -- ;
            MinHeapify(1) ;

			if ( LOG_STRUCTURE_UPDATES ) 
				printf("\n { (Min_Heap_U32::pop) Least element popped off the heap. } \n") ;
        }

		uint32_t top ( ) const {
			if ( size == 0 ) {
				error("(Min_Heap_U32::top) HEAP ERROR", "Heap is empty.") ;
				cleanAndExit() ;
			}	
			return heap[1] ;
		}
        
		uint8_t empty ( ) const {
			if ( size == 0 )	return 1 ;
			return 0 ;
		}

		void clear ( ) {
			if ( heap )	{
				delete [] heap ;
				if ( LOG_STRUCTURE_UPDATES ) 
					printf("\n { (Min_Heap_U32::clear) Heap memory deallocated. } \n") ;
			}
			heap = NULL ;
		}

        ~Min_Heap_U32 ( ) { clear() ; }
} ;

class hashnode_uint32_t {
    private :
        uint32_t key ;
		uint8_t avail ;
    public :
        hashnode_uint32_t ( uint32_t v = 0 ) : key(v) , avail(1) { }
        friend class Unordered_Set_U32 ;
} ;

class Unordered_Set_U32 {
    private :
        uint32_t _size ;
		uint32_t _occ ;
        hashnode_uint32_t * _table ;

		void error ( const char * type , const char * msg ) const {
			printf("\n\n [ %s : %s ]\n\n", type, msg) ;
    		return ;
		}

		void ReHash ( hashnode_uint32_t * new_table ) {
			if ( LOG_STRUCTURE_UPDATES )
				printf("\n { (Unordered_Set_U32::ReHash) Rehashing the smaller hashtable into the larger one. } \n") ;
			
			uint32_t new_size = (_size << 1) ;
			for ( uint32_t i = 0 ; i < _size ; i ++ ) {
				if ( _table[i].avail )	continue ;
				uint32_t hash = _table[i].key % new_size ;
				while ( ! new_table[hash].avail )
					hash = (hash + 1) % new_size ;
				new_table[hash].avail = 0 ;
				new_table[hash].key = _table[i].key ;
			}
		}

    public :
        Unordered_Set_U32 ( uint32_t table_size ) : _size(table_size) , _occ(0) {
            _table = new hashnode_uint32_t[_size] ;
			if ( ! _table ) {
				error("(Unordered_Set_U32::Unordered_Set_U32) MEMORY ERROR", 
					  "Memory could not be allocated for the hash-table.") ;
				cleanAndExit() ;
			}
			if ( LOG_STRUCTURE_UPDATES )
				printf("\n { (Unordered_Set_U32::Unordered_Set_U32) Memory allocated for the hashtable. } \n") ;
        }

		void insert ( uint32_t val ) {
			if ( find(val) )	return ;

			if ( _occ == _size ) {
				hashnode_uint32_t * new_table = new hashnode_uint32_t[(_size << 1)] ;
				if ( ! new_table ) {
					error("(Unordered_Set_U32::Unordered_Set_U32) MEMORY ERROR", 
						"Memory could not be allocated for the (expanded) hash-table.") ;
					cleanAndExit() ;
				}

				ReHash(new_table) ;
				_occ = _size ;
				_size = (_size << 1) ;

				delete [] _table ;
				_table = new_table ;
			}

			uint32_t hash = val % _size ;
			while ( ! _table[hash].avail )
				hash = (hash + 1) % _size ;
			
			_table[hash].key = val ;
			_table[hash].avail = 0 ;
			_occ ++ ;

			if ( LOG_STRUCTURE_UPDATES )
				printf("\n { (Unordered_Set_U32::insert) An element inserted into the hash-table. } \n") ;
		}

		uint8_t find ( uint32_t val ) const {
			uint32_t hash = val % _size ;
			uint32_t probes = 0 ;
			while ( ! _table[hash].avail && probes < _size ) {
				if ( _table[hash].key == val )	return 1 ;
				hash = (hash + 1) % _size ;
				probes ++ ;
			}
			return 0 ;
		}

		void clear ( ) {
			if ( _table ) {
				delete [] _table ;
				if ( LOG_STRUCTURE_UPDATES ) 
					printf("\n { (Unordered_Set_U32::clear) Hashtable memory deallocated. } \n") ;
			}
			_table = NULL ;
		}

		uint32_t size ( ) const { return _occ ; }

		~Unordered_Set_U32 ( ) { clear() ; }
} ;

#endif

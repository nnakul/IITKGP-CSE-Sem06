
#include "memlab.hpp"
#define MEM_SIZE_MB			0.01

void TC1 ( ) {
    printf("\n\n [ Test with a wrong identifier format (1) ] \n") ;
    createVar("int", "4th_demo_file") ;
}

void TC2 ( ) {
    printf("\n\n [ Test with a wrong identifier format (2) ] \n") ;
    createVar("int", "demo_file_#4") ;
}

void TC3 ( ) {
    printf("\n\n [ Test with a wrong identifier format (3) ] \n") ;
    createVar("int", "") ;
}

void TC4 ( ) {
    printf("\n\n [ Test with a wrong data type (float) ] \n") ;
    createVar("float", "myvar") ;
}

void TC5 ( ) {
    printf("\n\n [ Test with a wrong data type (Int) ] \n") ;
    createVar("Int", "myvar") ;
}

void TC6 ( ) {
    printf("\n\n [ Test assignment to a variable not in scope ] \n") ;
    createVar("int", "myvar") ;
    assignVar("myVar", "-123") ;
}

void TC7 ( ) {
    printf("\n\n [ Test freeing a variable not in scope ] \n") ;
    createVar("int", "myvar") ;
    initStackFrame(EMPTY, LOCAL_SCOPE) ;
    freeElem("myvar") ;
    deinitStackFrame() ;
}

void TC8 ( ) {
    printf("\n\n [ Test by assigning an incorrect type value to a data-type (int <- -123.0) ] \n") ;
    createVar("int", "myvar") ;
    assignVar("myvar", "-123.0") ;
}

void TC9 ( ) {
    printf("\n\n [ Test by assigning an incorrect type value to a data-type (bool <- True) ] \n") ;
    createVar("bool", "myvar") ;
    assignVar("myvar", "True") ;
}

void TC10 ( ) {
    printf("\n\n [ Test assignment to a variable not created ] \n") ;
    assignVar("myvar", "-123") ;
}

void TC11 ( ) {
    printf("\n\n [ Test by assigning a value that is out of range (med_int type) - 1 ] \n") ;
    createVar("med_int", "myvar") ;
    assignVar("myvar", "8388608") ;
}

void TC12 ( ) {
    printf("\n\n [ Test by assigning a value that is out of range (med_int type) - 2 ] \n") ;
    createVar("med_int", "myvar") ;
    assignVar("myvar", "-8388610") ;
}

void TC13 ( ) {
    printf("\n\n [ Test by assigning a value that is out of range (int type) - 1 ] \n") ;
    createVar("int", "myvar") ;
    assignVar("myvar", "2147483648") ;
}

void TC14 ( ) {
    printf("\n\n [ Test by assigning a value that is out of range (int type) - 2 ] \n") ;
    createVar("int", "myvar") ;
    assignVar("myvar", "-2147483649") ;
}

void TC15 ( ) {
    printf("\n\n [ Test by assigning a value that is of incorrect format (string to a char) ] \n") ;
    createVar("char", "myvar") ;
    assignVar("myvar", "ABC") ;
}

void TC16 ( ) {
    printf("\n\n [ Test using an array-function to assign a scalar-type ] \n") ;
    createVar("char", "myvar") ;
    assignArr("myvar", 0, "A") ;
}

void TC17 ( ) {
    printf("\n\n [ Test using a scalar-function to assign an array-type ] \n") ;
    createArr("bool", "myvar", 1) ;
    assignVar("myvar", "false") ;
}

void TC18 ( ) {
    printf("\n\n [ Test getting value of an X data-type as a Y data-type (scalars) - {bool as char} ] \n") ;
    createVar("bool", "myvar") ;
    assignVar("myvar", "true") ;
    getChar("myvar") ;
}

void TC19 ( ) {
    printf("\n\n [ Test getting value of an X data-type as a Y data-type (scalars) - {med_int as int} ] \n") ;
    createVar("med_int", "myvar") ;
    assignVar("myvar", "123") ;
    getInt("myvar") ;
}

void TC20 ( ) {
    printf("\n\n [ Test getting value of an X data-type as a Y data-type (arrays) - {char as bool} ] \n") ;
    createArr("char", "myvar", 15) ;
    assignArr("myvar", 0, "D") ;
    getArrBool("myvar", 0) ;
}

void TC21 ( ) {
    printf("\n\n [ Test getting value of an X data-type as a Y data-type (arrays) - {char as int} ] \n") ;
    createArr("char", "myvar", 15) ;
    assignArr("myvar", 0, "D") ;
    getArrInt("myvar", 0) ;
}

void TC22 ( ) {
    printf("\n\n [ Test getting value of a scalar data-type as an array data-type ] \n") ;
    createVar("bool", "myvar") ;
    assignVar("myvar", "true") ;
    getArrBool("myvar", 0) ;
}

void TC23 ( ) {
    printf("\n\n [ Test getting value of an array data-type as a scalar data-type ] \n") ;
    createArr("int", "myvar", 15) ;
    burstAssignArr("myvar", "123") ;
    getInt("myvar") ;
}

void TC24 ( ) {
    printf("\n\n [ Test assigning an array out of bounds (single element) ] \n") ;
    createArr("char", "myvar", 15) ;
    assignArr("myvar", 15, "C") ;
}

void TC25 ( ) {
    printf("\n\n [ Test getting an array element with index out of bounds ] \n") ;
    createArr("char", "myvar", 15) ;
    getArrChar("myvar", 17) ;
}

void TC26 ( ) {
    printf("\n\n [ Test accessing a freed variable ] \n") ;
    createVar("int", "myvar") ;
    freeElem("myvar") ;
    getInt("myvar") ;
}

void TC27 ( ) {
    printf("\n\n [ Test passing parameters to a local block ] \n") ;
    initStackFrame(1, LOCAL_SCOPE) ;
    deinitStackFrame() ;
}

void TC28 ( ) {
    printf("\n\n [ Test redeclaration of a variable - 1 ] \n") ;
    createVar("int", "myvar") ;
    createVar("int", "myvar") ;
}

void TC29 ( ) {
    printf("\n\n [ Test redeclaration of a variable - 2 ] \n") ;
    createVar("int", "myvar") ;
    createVar("char", "myvar") ;
}

void TC30 ( ) {
    printf("\n\n [ Test redeclaration of a variable - 3 ] \n") ;
    createArr("int", "myvar", 5) ;
    createVar("int", "myvar") ;
}

void TC31 ( ) {
    printf("\n\n [ Test creating an array of zero size ] \n") ;
    createArr("int", "myvar", 0) ;
}

int main ( int __ , char ** argv )    {
	createMem(MEM_SIZE_MB * 1000000 / 4);
	initStackFrame(0, FUNCTION_SCOPE);

    int tc = atoi(argv[1]) ;
    if ( tc == 1 )  TC1() ;
    else if ( tc == 2 )  TC2() ;
    else if ( tc == 3 )  TC3() ;
    else if ( tc == 4 )  TC4() ;
    else if ( tc == 5 )  TC5() ;
    else if ( tc == 6 )  TC6() ;
    else if ( tc == 7 )  TC7() ;
    else if ( tc == 8 )  TC8() ;
    else if ( tc == 9 )  TC9() ;
    else if ( tc == 10 )  TC10() ;
    else if ( tc == 11 )  TC11() ;
    else if ( tc == 12 )  TC12() ;
    else if ( tc == 13 )  TC13() ;
    else if ( tc == 14 )  TC14() ;
    else if ( tc == 15 )  TC15() ;
    else if ( tc == 16 )  TC16() ;
    else if ( tc == 17 )  TC17() ;
    else if ( tc == 18 )  TC18() ;
    else if ( tc == 19 )  TC19() ;
    else if ( tc == 20 )  TC20() ;
    else if ( tc == 21 )  TC21() ;
    else if ( tc == 22 )  TC22() ;
    else if ( tc == 23 )  TC23() ;
    else if ( tc == 24 )  TC24() ;
    else if ( tc == 25 )  TC25() ;
    else if ( tc == 26 )  TC26() ;
    else if ( tc == 27 )  TC27() ;
    else if ( tc == 28 )  TC28() ;
    else if ( tc == 29 )  TC29() ;
    else if ( tc == 30 )  TC30() ;
    else if ( tc == 31 )  TC31() ;

    deinitStackFrame() ;
    gc_run(GC_VOLUNTARY);
	stopMMU();
}

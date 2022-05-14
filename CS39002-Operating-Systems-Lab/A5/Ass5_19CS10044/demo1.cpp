
#include "memlab.hpp"

#define USER_ARRAY_SIZE 	50000
#define MEM_SIZE_MB			250
#define enable_gc			1

void funcInt()
{
	createArr("int", "A", USER_ARRAY_SIZE);

	createVar("int", "i");
	assignVar("i", "0");

	initStackFrame(EMPTY, LOCAL_SCOPE);
	while (getInt("i") < USER_ARRAY_SIZE)
	{
		assignArr("A", getInt("i"), to_string(rand()).c_str());
		assignVar("i", to_string(getInt("i") + 1).c_str());
	}
	deinitStackFrame();

	freeElem("i");
	freeElem("A");

	if ( enable_gc ) gc_run(GC_VOLUNTARY);
}

void funcMedInt()
{
	createArr("med_int", "A", USER_ARRAY_SIZE);

	createVar("int", "i");
	assignVar("i", "0");

	initStackFrame(EMPTY, LOCAL_SCOPE);
	while (getInt("i") < USER_ARRAY_SIZE)
	{
		assignArr("A", getInt("i"), to_string(rand() % 8388608).c_str());
		assignVar("i", to_string(getInt("i") + 1).c_str());
	}
	deinitStackFrame();

	freeElem("i");
	freeElem("A");

	if ( enable_gc ) gc_run(GC_VOLUNTARY);
}

void funcChar()
{
	createArr("char", "A", USER_ARRAY_SIZE);

	createVar("int", "i");
	assignVar("i", "0");

	initStackFrame(EMPTY, LOCAL_SCOPE);
	while (getInt("i") < USER_ARRAY_SIZE)
	{
		assignArr("A", getInt("i"), string(1, 21 + rand() % 106).c_str());
		assignVar("i", to_string(getInt("i") + 1).c_str());
	}
	deinitStackFrame();

	freeElem("i");
	freeElem("A");

	if ( enable_gc ) gc_run(GC_VOLUNTARY);
}

void funcBool()
{
	createArr("bool", "A", USER_ARRAY_SIZE);

	createVar("int", "i");
	assignVar("i", "0");

	initStackFrame(EMPTY, LOCAL_SCOPE);
	while (getInt("i") < USER_ARRAY_SIZE)
	{
		initStackFrame(EMPTY, LOCAL_SCOPE);
		if (rand() % 7 == 0)
		{
			assignArr("A", getInt("i"), "true");
		}
		else
		{
			assignArr("A", getInt("i"), "false");
		}
		deinitStackFrame();

		assignVar("i", to_string(getInt("i") + 1).c_str());
	}
	deinitStackFrame();

	freeElem("i");
	freeElem("A");

	if ( enable_gc ) gc_run(GC_VOLUNTARY);
}

int main()
{
	createMem(MEM_SIZE_MB * 1000000 / 4);

	initStackFrame(0, FUNCTION_SCOPE);

	createParam("int", "x");
	createParam("int", "y");
	initStackFrame(2, FUNCTION_SCOPE);
	funcInt();
	deinitStackFrame();
	freeElem("y");
	freeElem("x");

	createParam("med_int", "x");
	createParam("med_int", "y");
	initStackFrame(2, FUNCTION_SCOPE);
	funcMedInt();
	deinitStackFrame();
	freeElem("y");
	freeElem("x");

	createParam("char", "x");
	createParam("char", "y");
	initStackFrame(2, FUNCTION_SCOPE);
	funcChar();
	deinitStackFrame();
	freeElem("y");
	freeElem("x");

	createParam("bool", "x");
	createParam("bool", "y");
	initStackFrame(2, FUNCTION_SCOPE);
	funcBool();
	deinitStackFrame();
	freeElem("y");
	freeElem("x");

	createParam("int", "x");
	createParam("int", "y");
	initStackFrame(2, FUNCTION_SCOPE);
	funcInt();
	deinitStackFrame();
	freeElem("y");
	freeElem("x");

	createParam("med_int", "x");
	createParam("med_int", "y");
	initStackFrame(2, FUNCTION_SCOPE);
	funcMedInt();
	deinitStackFrame();
	freeElem("y");
	freeElem("x");

	createParam("char", "x");
	createParam("char", "y");
	initStackFrame(2, FUNCTION_SCOPE);
	funcChar();
	deinitStackFrame();
	freeElem("y");
	freeElem("x");

	createParam("bool", "x");
	createParam("bool", "y");
	initStackFrame(2, FUNCTION_SCOPE);
	funcBool();
	deinitStackFrame();
	freeElem("y");
	freeElem("x");

	createParam("int", "x");
	createParam("int", "y");
	initStackFrame(2, FUNCTION_SCOPE);
	funcInt();
	deinitStackFrame();
	freeElem("y");
	freeElem("x");

	createParam("med_int", "x");
	createParam("med_int", "y");
	initStackFrame(2, FUNCTION_SCOPE);
	funcMedInt();
	deinitStackFrame();
	freeElem("y");
	freeElem("x");

	deinitStackFrame();

	if ( enable_gc ) gc_run(GC_VOLUNTARY);

	stopMMU();
}

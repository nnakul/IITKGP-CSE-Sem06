
#include "memlab.hpp"

#define enable_gc		1
#define MEM_SIZE_MB		1

void fibonacci()
{
	createVar("int", "i");
	assignVar("i", "0");

	initStackFrame(EMPTY, LOCAL_SCOPE);
	while (getInt("i")<getInt("n"))
	{
		initStackFrame(EMPTY, LOCAL_SCOPE);
		if (getInt("i") == 0)
			assignArr("A", 0, "1");
		else if (getInt("i") == 1)
			assignArr("A", 1, "1");
		else
		{
			assignArr("A", getInt("i"), to_string(getArrInt("A", getInt("i")-1) + getArrInt("A", getInt("i")-2)).c_str());
		}
		deinitStackFrame();

		assignVar("i", to_string(getInt("i") + 1).c_str());
	}
	deinitStackFrame();

	freeElem("i");
	if ( enable_gc ) gc_run(GC_VOLUNTARY);
}

void fibonacciProduct()
{
	createArrParam("int", "A", getInt("k"));
	burstAssignArr("A", "0");

	createParam("int", "n");
	assignParam("n", to_string(getInt("k")).c_str());

	initStackFrame(2, FUNCTION_SCOPE);
	fibonacci();
	deinitStackFrame();

	assignVar("product", "1");

	createVar("int", "i");
	assignVar("i", "0");

	initStackFrame(EMPTY, LOCAL_SCOPE);
	while (getInt("i")<getInt("k"))
	{
		assignVar("product", to_string(getInt("product") * getArrInt("A", getInt("i"))).c_str());
		assignVar("i", to_string(getInt("i") + 1).c_str());
	}
	deinitStackFrame();

	freeElem("i");
	freeElem("n");
	freeElem("A");

	if ( enable_gc ) gc_run(GC_VOLUNTARY);
}

int main(int __ , char ** argv)
{
	createMem(MEM_SIZE_MB * 1000000 / 4, 0, 0, 0);

	initStackFrame(0, FUNCTION_SCOPE);

	createParam("int", "product");
	createParam("int", "k");
	assignParam("k", argv[1]);

	initStackFrame(2, FUNCTION_SCOPE);
	fibonacciProduct();
	deinitStackFrame();

	printf("\n [ RESULT : Product of first %d Fibonacci numbers is %d ]\n", getInt("k"), getInt("product"));

	freeElem("k");
	freeElem("product");
	deinitStackFrame();

	if ( enable_gc ) gc_run(GC_VOLUNTARY);

	stopMMU();
}
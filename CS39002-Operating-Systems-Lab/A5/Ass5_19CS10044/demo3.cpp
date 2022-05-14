
#include "memlab.hpp"

#define MEM_SIZE_MB			250
#define enable_gc			1

void gcd() {
	if (getInt("b") == 0)
	{

		printf("%d %d", getInt("a"), getInt("b"));

		assignVar("gcd", to_string(getInt("a")).c_str());

		if ( enable_gc ) gc_run(GC_VOLUNTARY);

		return;
	}

	createVar("int", "result");
	createVar("int", "x");
	createVar("int", "y");

	assignVar("x", to_string(getInt("a")).c_str());
	assignVar("y", to_string(getInt("b")).c_str());

	initStackFrame(EMPTY, LOCAL_SCOPE);

	createParam("int", "a");
	createParam("int", "b");
	createParam("int", "gcd");

	assignParam("a", to_string(getInt("y")).c_str());
	assignParam("b", to_string(getInt("x") % getInt("y")).c_str());

	initStackFrame(3, FUNCTION_SCOPE);
	gcd();
	deinitStackFrame();

	assignVar("result", to_string(getInt("gcd")).c_str());

	freeElem("gcd");
	freeElem("b");
	freeElem("a");

	deinitStackFrame();

	printf("%d %d %d", getInt("a"), getInt("b"),  getInt("result"));

	assignVar("gcd", to_string(getInt("result")).c_str());

	freeElem("y");
	freeElem("x");
	freeElem("result");

	if ( enable_gc ) gc_run(GC_VOLUNTARY);

	return;
}

int main(int __ , char ** argv)
{
	createMem(MEM_SIZE_MB * 1000000 / 4, 1000, 0, 0);

	initStackFrame(0, FUNCTION_SCOPE);

	createVar("int", "x");
	createVar("int", "y");

	assignVar("x", argv[1]);
	assignVar("y", argv[2]);

    createParam("int", "a");
	createParam("int", "b");
	createParam("int", "gcd");

	assignParam("a", argv[1]);
	assignParam("b", argv[2]);

	initStackFrame(3, FUNCTION_SCOPE);
	gcd();
	deinitStackFrame();

	printf("\n [ RESULT : GCD of %d and %d is %d ]\n", getInt("x"), getInt("y"), getInt("gcd"));

	freeElem("gcd");
	freeElem("b");
	freeElem("a");

	freeElem("y");
	freeElem("x");

	deinitStackFrame();

	if ( enable_gc ) gc_run(GC_VOLUNTARY);

	stopMMU();

}

/*
TEST CASE 1 : gcd(143, 221) = 13
TEST CASE 2 : gcd(432, 168) = 24
TEST CASE 3 : gcd(9102, 10824) = 246
*/

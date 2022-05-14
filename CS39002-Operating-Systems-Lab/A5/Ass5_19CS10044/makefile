all: demo1 demo2 demo3 error

demo1: 	libmemlab.a demo1.cpp 
		g++ -o demo1 demo1.cpp -L. -lmemlab -lpthread

demo2: 	libmemlab.a demo2.cpp
		g++ -o demo2 demo2.cpp -L. -lmemlab -lpthread

demo3: 	libmemlab.a demo3.cpp
		g++ -o demo3 demo3.cpp -L. -lmemlab -lpthread

error: 	libmemlab.a error.cpp
		g++ -o error error.cpp -L. -lmemlab -lpthread

libmemlab: libmemlab.a

libmemlab.a: 	memlab.o
				ar rcs libmemlab.a memlab.o
				
memlab.o: 	memlab.cpp memlab.hpp
			g++ -c memlab.cpp
			
clean: 
		rm -f demo1 demo2 demo3 error memlab.o libmemlab.a *.gch
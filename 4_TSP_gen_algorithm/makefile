main: main.c graph.o queue.o random_maker.o TSP.o thread_pool.o
	gcc-7 main.c graph.o queue.o random_maker.o TSP.o thread_pool.o -o run -pthread

graph.o: graph.c graph.h
	gcc-7 -c graph.c -pthread

queue.o: queue.c queue.h
	gcc-7 -c queue.c -pthread

random_maker.o: random_maker.c random_maker.h
	gcc-7 -c random_maker.c -pthread

TSP.o: TSP.c TSP.h
	gcc-7 -c TSP.c -pthread

thread_pool.o: thread_pool.c thread_pool.h
	gcc-7 -c thread_pool.c -pthread

clean:
	rm run *.o
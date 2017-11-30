#include "graph.h"

typedef struct ResPathCtx {
	size_t iterations;
	double time;
	int* shortest_path;
} ResPathCtx;

int ShortestPathFind(const graph_t* graph, size_t thread_count, size_t population_size,
                     size_t same_fitness_for, ResPathCtx *return_data);

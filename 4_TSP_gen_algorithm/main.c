#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "TSP.h"

const char* kGenerateMode = "--generate";
const char* kFileMode = "--file";

int main(int argc, char* argv[]) {
  graph_t* graph;
  size_t t;
  size_t N;
  size_t S;
  ResPathCtx result;
  FILE* stats;
  int best_fitness_value;
  
  if (argc != 6) {
    printf("Incorrect input!");
    exit(1);
  }
  sscanf(argv[1], "%lu", &t);
  sscanf(argv[2], "%lu", &N);
  sscanf(argv[3], "%lu", &S);
  
  srand(time(NULL));

  if (!strcmp(argv[4], kFileMode)) {
    graph = graph_read_file(argv[5]);
  } else if (!strcmp(argv[4], kGenerateMode)) {
    graph = graph_generate(atoi(argv[5]), 100);
//    char* output_file_name = "graph100.txt";
//    graph_dump_file(graph, output_file_name);
  }
  result.shortest_path = malloc(graph->n * sizeof(int));
  best_fitness_value = ShortestPathFind(graph, t, N, S, &result);
  stats = fopen("stats.txt", "w");
  fprintf(stats, "%lu %lu %lu %d %lu %.5lfs %d\n", t, N, S, graph->n, result.iterations, result.time, best_fitness_value);
  for (int i = 0; i < graph->n; i++) {
    fprintf(stats, "%d ", result.shortest_path[i]);
  }
  
  fprintf(stats, "\n");
  free(result.shortest_path);
  fclose(stats);
  graph_destroy(graph);
}

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "graph.h"
#include "random_maker.h"
#include "thread_pool.h"
#include "TSP.h"

typedef struct PathCtx {
  int* path_ctx;
  int fitness;
  size_t length;
} PathCtx;

int PathCompare(const void* a, const void* b) {
  const PathCtx* a_path = (const PathCtx*)a;
  const PathCtx* b_path = (const PathCtx*)b;
  return a_path->fitness - b_path->fitness;
}

typedef struct MutateJob {
  RandomMaker* maker;
  PathCtx* paths;
  const graph_t* graph;
  size_t paths_count;
} MutateJob;

typedef struct CrossoverJob {
  RandomMaker* maker;
  const PathCtx* paths;
  size_t paths_count;
  PathCtx* output;
  size_t output_count;
} CrossoverJob;

void StopTask(void* in) {
  ThreadPool* pool = in;
  ThreadPoolShutdown(pool);
}

int Fitness(const PathCtx* path_ctx, const graph_t* graph) {
  int result = 0;
  assert(path_ctx->length > 1);
  int first = 0;
  int second = 1;
  for (; second < path_ctx->length; ++first, ++second) {
    result += graph_weight(graph, path_ctx->path_ctx[first], path_ctx->path_ctx[second]);
  }
  result += graph_weight(graph, 0, path_ctx->path_ctx[path_ctx->length - 1]);
  return result;
}

int VerifyPermutation(const PathCtx* path_ctx) {
  int* used = calloc(path_ctx->length, sizeof(int));
  size_t i;
  for (i = 0; i < path_ctx->length; ++i) {
    used[path_ctx->path_ctx[i]] = 1;
  }
  for (i = 0; i < path_ctx->length; ++i) {
    if (!used[i]) {
      for (size_t j = 0; j < path_ctx->length; ++j)
        printf("%d ", used[j]);
      printf("\n");
      return 0;
    }
  }
  free(used);
  return 1;
}

void Mutate(PathCtx* path_ctx, RandomBlock* block) {
  assert(VerifyPermutation(path_ctx));
  size_t i;
  const size_t kSwapsPerMutation = 1;
  for (i = 0; i < kSwapsPerMutation; i++) {
    size_t rand1 = RandomBlockPopRandom(block);
    size_t rand2 = RandomBlockPopRandom(block);
    size_t pos1 = rand1 % path_ctx->length;
    size_t pos2 = rand2 % path_ctx->length;
    int temp = path_ctx->path_ctx[pos1];
    path_ctx->path_ctx[pos1] = path_ctx->path_ctx[pos2];
    path_ctx->path_ctx[pos2] = temp;
  }
  assert(VerifyPermutation(path_ctx));
}

void MutateTask(void* in) {
  size_t i;
  MutateJob* task = (MutateJob*)in;
  RandomBlock* block = RandomBlockCreate(task->maker);
  for (i = 0; i < task->paths_count; ++i) {
    PathCtx* path_ctx = task->paths + i;
    Mutate(path_ctx, block);
    path_ctx->fitness = Fitness(path_ctx, task->graph);
    assert(path_ctx->fitness > 0);
  }
  RandomBlockDelete(block);
  free(task);
}

void Crossover(const PathCtx* left, const PathCtx* right, PathCtx* result) {
  int* used = calloc(left->length, sizeof(int));
  size_t result_cursor;
  size_t right_cursor;
  result->length = left->length;
  assert(right->length == left->length);
  for (result_cursor = 0; result_cursor < result->length / 2; ++result_cursor) {
    result->path_ctx[result_cursor] = left->path_ctx[result_cursor];
    used[left->path_ctx[result_cursor]] = 1;
  }
  for (right_cursor = 0; right_cursor < right->length; ++right_cursor) {
    if (!used[right->path_ctx[right_cursor]]) {
      result->path_ctx[result_cursor] = right->path_ctx[right_cursor];
      used[right->path_ctx[right_cursor]] = 1;
      ++result_cursor;
    }
  }
  assert(VerifyPermutation(result));
  free(used);
}

void CrossoverTask(void* in) {
  CrossoverJob* task = (CrossoverJob*)in;
  size_t cursor;
  RandomBlock* block = RandomBlockCreate(task->maker);
  for (cursor = 0; cursor < task->output_count; ++cursor) {
    size_t rand1 = RandomBlockPopRandomLong(block) % task->paths_count;
    size_t rand2 = RandomBlockPopRandomLong(block) % task->paths_count;
    Crossover(task->paths + rand1, task->paths + rand2, task->output + cursor);
  }
  RandomBlockDelete(block);
  free(task);
}

void memswap(void* a, void* b, size_t size) {
  char* a_cast = (char*)a;
  char* b_cast = (char*)b;
  size_t i;
  for (i = 0; i < size; i++) {
    char tmp = a_cast[i];
    a_cast[i] = b_cast[i];
    b_cast[i] = tmp;
  }
}

void DumpPaths(const PathCtx* arr, size_t count) {
  size_t path_ctx;
  for (path_ctx = 0; path_ctx < count; path_ctx++) {
    size_t coord;
    for (coord = 0; coord < arr[path_ctx].length; ++coord)
      printf("%d ", arr[path_ctx].path_ctx[coord]);
    printf("%d\n", arr[path_ctx].fitness);
  }
}

double timediff(struct timeval* a, struct timeval* b) {
  return ((a->tv_sec - b->tv_sec) * 1e6 + (a->tv_usec - b->tv_usec)) / 1.0e6;
}

int ShortestPathFind(const graph_t* graph, size_t thread_count, size_t population_size,
                     size_t same_fitness_for, ResPathCtx *return_data) {
  ThreadPool thread_pool;
  int best_fitness_value = INT_MAX;
  size_t current_same_best = 0;
  size_t iterations = 0;
  const size_t kReproductionFactor = 4;
  size_t children_size = population_size * kReproductionFactor;
  PathCtx* population = malloc(population_size * sizeof(PathCtx));
  PathCtx* children = malloc(children_size * sizeof(PathCtx));
  RandomMaker* maker = RandomMakerCreate();
  struct timeval begin;
  struct timeval end;
  gettimeofday(&begin, NULL);
  ThreadPoolInit(&thread_pool, thread_count);
  {
    size_t i;
    for (i = 0; i < population_size; ++i) {
      size_t j;
      population[i].length = graph->n;
      population[i].path_ctx = (int*)malloc(sizeof(int) * graph->n);
      for (j = 0; j < graph->n; ++j) {
        population[i].path_ctx[j] = j;
      }
    }
    for (i = 0; i < children_size; ++i) {
      children[i].path_ctx = (int*)malloc(sizeof(int) * graph->n);
    }
  }
  while (current_same_best < same_fitness_for) {
    
    
    // Crossover
    {
      size_t child_offset = 0;

      while (child_offset < children_size) {
        size_t block_size;
        const size_t kPathsPerCrossoverTask = 64;

        if (children_size - child_offset < kPathsPerCrossoverTask) {
          block_size = children_size - child_offset;
        } else {
          block_size = kPathsPerCrossoverTask;
        }
        CrossoverJob* job_task = (CrossoverJob*)malloc(sizeof(CrossoverJob));
        ThreadTask* pool_task = (ThreadTask*)malloc(sizeof(ThreadTask));
        job_task->maker = maker;
        job_task->paths = population;
        job_task->paths_count = population_size;
        job_task->output = children + child_offset;
        job_task->output_count = block_size;
        child_offset += block_size;
        ThreadPoolCreateTask(pool_task, job_task, CrossoverTask);
        ThreadPoolAddTask(&thread_pool, pool_task);
      }

      ThreadPoolShutdown(&thread_pool);
      ThreadPoolStart(&thread_pool);
      ThreadPoolJoin(&thread_pool);
    }
    ThreadPoolReset(&thread_pool);
    
    
    // Mutation
    {
      size_t child_offset = 0;

      while (child_offset < children_size) {
        size_t block_size;
        const size_t kPathsPerMutationTask = 16;
        if (children_size - child_offset < kPathsPerMutationTask) {
          block_size = children_size - child_offset;
        } else {
          block_size = kPathsPerMutationTask;
        }
        MutateJob* job_task = (MutateJob*)malloc(sizeof(MutateJob));
        ThreadTask* pool_task = (ThreadTask*)malloc(sizeof(ThreadTask));
        job_task->maker = maker;
        job_task->paths = children + child_offset;
        job_task->paths_count = block_size;
        job_task->graph = graph;
        child_offset += block_size;
        ThreadPoolCreateTask(pool_task, job_task, MutateTask);
        ThreadPoolAddTask(&thread_pool, pool_task);
      }

      ThreadPoolShutdown(&thread_pool);
      ThreadPoolStart(&thread_pool);
      ThreadPoolJoin(&thread_pool);
    }
    ThreadPoolReset(&thread_pool);
    
    
    // Selection
    {
      size_t i;
      double average_fitness = 0;
      qsort(children, children_size, sizeof(PathCtx), PathCompare);
      assert(children[0].fitness == Fitness(children, graph));
      for (i = 0; i < children_size; ++i) {
        average_fitness += children[i].fitness;
      }
      average_fitness /= children_size;
      printf("Iteration %lu best: %d worst: %d average: %lf\n", iterations,
             children[0].fitness, children[children_size - 1].fitness,
             average_fitness);
      if (children[0].fitness < best_fitness_value) {
        if (return_data) {
          memcpy(return_data->shortest_path, children[0].path_ctx, sizeof(int) * graph->n);
        }
        best_fitness_value = children[0].fitness;
        current_same_best = 0;
      } else {
        ++current_same_best;
      }
      memswap(population, children, population_size * sizeof(PathCtx));
    }
    ++iterations;
  }
  RandomMakerDelete(maker);
  ThreadPoolDestroy(&thread_pool);
  {
    size_t i;
    for (i = 0; i < population_size; ++i) {
      free(population[i].path_ctx);
    }
    for (i = 0; i < children_size; ++i) {
      free(children[i].path_ctx);
    }
  }
  free(population);
  free(children);
  gettimeofday(&end, NULL);
  if (return_data) {
    return_data->iterations = iterations;
    return_data->time = timediff(&end, &begin);
  }
  return best_fitness_value;
}

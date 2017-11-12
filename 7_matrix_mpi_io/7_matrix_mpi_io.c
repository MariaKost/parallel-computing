#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

typedef struct point {
  int x;
  int y;
  int r;
} point;

typedef struct mpi_table_ctx {
  int rank;
  int size;
  int l;
  int a;
  int b;
  int N;
  point *points_arr;
} mpi_table_ctx;

void points_generating(mpi_table_ctx *ctx) {
  unsigned int seed;
  unsigned int *seeds = (unsigned int*)calloc(ctx->size, sizeof(unsigned int));
  if (ctx->rank == 0) {
    srand(time(NULL));
    for (int i = 0; i < ctx->size; ++i) {
      seeds[i] = rand();
    }
  }
  MPI_Scatter(seeds, 1, MPI_UNSIGNED, &seed, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
  free(seeds);
  srand(time(NULL));
  for (int i = 0; i < ctx->N; ++i) {
    ctx->points_arr[i].x = rand_r(&seed) % ctx->l;
    ctx->points_arr[i].y = rand_r(&seed) % ctx->l;
    ctx->points_arr[i].r = rand_r(&seed) % (ctx->a * ctx->b);
  }
}

void table_analysing(mpi_table_ctx *ctx) {
  int *table = (int*) calloc(ctx->l * ctx->l * ctx->size, sizeof(int));
  for(int i = 0; i < ctx->N; ++i) {
    int x = ctx->points_arr[i].x;
    int y = ctx->points_arr[i].y;
    int r = ctx->points_arr[i].r;
    ++table[y * ctx->l * ctx->size + x * ctx->size + r];
  }
  
  MPI_Aint intex;
  MPI_Type_extent(MPI_INT, &intex);
  
  MPI_File fh;
  MPI_File_open(MPI_COMM_WORLD, "data.bin", MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &fh);
  
  MPI_Datatype view;
  MPI_Type_vector(ctx->l, ctx->l * ctx->size, ctx->l * ctx->a * ctx->size, MPI_INT, &view);
  MPI_Type_commit(&view);
  
  int offset = ((ctx->rank % ctx->a * ctx->l) + (ctx->rank / ctx->a * ctx->a * ctx->l * ctx->l)) * ctx->size;
  offset = offset * intex;
  MPI_File_set_view(fh, offset, MPI_INT, view, "native", MPI_INFO_NULL);
  
  MPI_File_write(fh, table, ctx->l * ctx->l * ctx->size, MPI_INT, MPI_STATUS_IGNORE);
  
  MPI_Type_free(&view);
  MPI_File_close(&fh);
  free(table);
}

int main(int argc, char **argv) {
  MPI_Init(&argc, &argv);
  int rank;
  int size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (argc != 5) {
    printf("Incorrect input!\n");
  }
  int l = atoi(argv[1]);
  int a = atoi(argv[2]);
  int b = atoi(argv[3]);
  int N = atoi(argv[4]);
  point *points_arr = (point*)calloc(N, sizeof(point));
  
  mpi_table_ctx ctx = {
    .rank = rank,
    .size = size,
    .l = l,
    .a = a,
    .b = b,
    .N = N,
    .points_arr = points_arr
  };
  
  
  
  struct timeval start, finish;
  if (rank == 0) {
    gettimeofday(&start, NULL);;
  }
  points_generating(&ctx);
  table_analysing(&ctx);
  if (rank == 0) {
    gettimeofday(&finish, NULL);
    double prog_work_time = 0;
    prog_work_time = finish.tv_sec - start.tv_sec + (finish.tv_usec - start.tv_usec) / 1.e6;
    
    FILE *f = fopen("stats.txt", "a");
    if (f == NULL) {
      printf("Error opening file!\n");
      exit(1);
    }
    fprintf(f, "%d %d %d %d %.5lfs\n", l, a, b, N, prog_work_time);
    fclose(f);
  }
  
  free(points_arr);
  MPI_Finalize();
  return 0;
}

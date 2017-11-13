#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

const int threshold = 50;
const int reallocation_factor = 2;

typedef struct point {
  int x;
  int y;
  int rest_num_of_steps;
  unsigned int seed;
} point;

typedef struct rand_walk_2d_ctx {
  int rank;
  int size;
  int l;
  int a;
  int b;
  int n;
  int N;
  double p_l;
  double p_r;
  double p_u;
  double p_d;
} rand_walk_2d_ctx;

void points_generating(rand_walk_2d_ctx *ctx, point* points) {
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

  for (int i = 0; i < ctx->N; ++i) {
    (points+i)->x = rand_r(&seed) % ctx->l;
    (points+i)->y = rand_r(&seed) % ctx->l;
    (points+i)->rest_num_of_steps = ctx->n;
    (points+i)->seed = rand_r(&seed);
  }
}

void check_neighbours(rand_walk_2d_ctx *ctx, int *neighbs) {
  int left_neighb = 0;
  int right_neighb = 0;
  int up_neighb = 0;
  int down_neighb = 0;

  int x_curr_sq_coord = ctx->rank % ctx->a;
  int y_curr_sq_coord = ctx->rank / ctx->a;
  
  if (x_curr_sq_coord == 0) {
    left_neighb = ctx->rank + ctx->a - 1;
  } else {
    left_neighb = ctx->rank - 1;
  }
  if (x_curr_sq_coord == ctx->a - 1) {
    right_neighb = ctx->rank - ctx->a + 1;
  } else {
    right_neighb = ctx->rank + 1;
  }
  if (y_curr_sq_coord == 0) {
    up_neighb = ctx->size - ctx->a + ctx->rank;
  } else {
    up_neighb = ctx->rank - ctx->a;
  }
  if(y_curr_sq_coord == ctx->b - 1){
    down_neighb = ctx->rank + ctx->a - ctx->size;
  } else {
    down_neighb = ctx->rank + ctx->a;
  }
  neighbs[0] = left_neighb;
  neighbs[1] = right_neighb;
  neighbs[2] = up_neighb;
  neighbs[3] = down_neighb;
}

void make_step(rand_walk_2d_ctx *ctx, point* point) {
  int direction = -1;
  /*
   0 - left
   1 - right
   2 - up
   3 - down
   */
  double rand_num = (double)rand_r(&(point->seed));

  if (rand_num <= RAND_MAX * ctx->p_l) {
    direction = 0;
  } else if (rand_num <= RAND_MAX * (ctx->p_l + ctx->p_r)) {
    direction = 1;
  } else if(rand_num <= RAND_MAX * (ctx->p_l + ctx->p_r + ctx->p_u)) {
    direction = 2;
  } else {
    direction = 3;
  }
  
  if (direction == 0) {
    point->x -= 1;
  } else if (direction == 1) {
   point->x += 1;
  } else if (direction == 2) {
    point->y -= 1;
  } else if (direction == 3) {
    point->y += 1;
  }
  point->rest_num_of_steps -= 1;
}

int check_if_moved(rand_walk_2d_ctx *ctx, point* point) {
  /*
   0 - moved to the left square
   1 - moved to the right square
   2 - moved to the up square
   3 - moved to the down square
   4 - stay in the same square
   */
  if (point->x < 0) {
    point->x = ctx->l - 1;
    return 0;
  }
  if (point->x >= ctx->l) {
    point->x = 0;
    return 1;
  }
  if (point->y < 0) {
    point->y = ctx->l - 1;
    return 2;
  }
  if (point->y >= ctx->l) {
    point->y = 0;
    return 3;
  }
  return 4;
}

void replace_points(rand_walk_2d_ctx *ctx, int *num_of_points, point *moved_from_points, int *from_num_of_points, point* points, int *points_arr_actual_len) {
  for (int i = 0; i < *from_num_of_points; ++i) {
    if (*num_of_points >= *points_arr_actual_len) {
      *points_arr_actual_len = reallocation_factor * *points_arr_actual_len;
      points = (point*)realloc(points, *points_arr_actual_len * sizeof(point));
    }
    points[*num_of_points] = moved_from_points[i];
    *num_of_points = *num_of_points + 1;
  }
}

void move_to_square(int *moved_count, int *moved_arr_len, point *moved_points, point *points, int *num_of_points, int *index) {
  if (*moved_count >= *moved_arr_len) {
    *moved_arr_len = reallocation_factor * *moved_arr_len;
    moved_points = (point*)realloc(moved_points, *moved_arr_len * sizeof(point));
  }
  moved_points[*moved_count] = points[*index];
  points[*index] = points[*num_of_points - 1];
  *moved_count += 1;
  --*num_of_points;
  --*index;
}

void random_walk_2d(rand_walk_2d_ctx *ctx) {
  struct timeval start, finish;
  if (ctx->rank == 0) {
    gettimeofday(&start, NULL);
  }
  
  int points_arr_actual_len = reallocation_factor * ctx->N;
  point *points = (point*)malloc(points_arr_actual_len * sizeof(point));
  
  points_generating(ctx, points);
  
  point *to_left_moved = (point*)malloc(ctx->N * sizeof(point));
  point *to_right_moved = (point*)malloc(ctx->N * sizeof(point));
  point *to_up_moved = (point*)malloc(ctx->N * sizeof(point));
  point *to_down_moved = (point*)malloc(ctx->N * sizeof(point));
  int to_left_moved_arr_len = ctx->N;
  int to_right_moved_arr_len = ctx->N;
  int to_up_moved_arr_len = ctx->N;
  int to_down_moved_arr_len = ctx->N;
  
  int to_left_moved_count = 0;
  int to_right_moved_count = 0;
  int to_up_moved_count = 0;
  int to_down_moved_count = 0;
  int stopped_points_count = 0;
  int num_of_points = ctx->N;
  
  int neighbours[4];
  check_neighbours(ctx, neighbours);
  
  int flag = 0;
  while (flag != 1) {
    to_left_moved_count = 0;
    to_right_moved_count = 0;
    to_up_moved_count = 0;
    to_down_moved_count = 0;
    
    for (int k = 0; k < threshold; ++k) {
      int i = 0;
      while (i < num_of_points) {
        if (points[i].rest_num_of_steps == 0) {
          ++stopped_points_count;
          points[i] = points[num_of_points - 1];
          --num_of_points;
          --i;
        } else {
          make_step(ctx, points+i);
          int moving = -1;
          /*
           0 - moved to the left square
           1 - moved to the right square
           2 - moved to the up square
           3 - moved to the down square
           4 - stay in the same square
           */
          moving = check_if_moved(ctx, points+i);
          if (moving == 0) {
            move_to_square(&to_left_moved_count, &to_left_moved_arr_len, to_left_moved, points, &num_of_points, &i);
          } else if (moving == 1) {
            move_to_square(&to_right_moved_count, &to_right_moved_arr_len, to_right_moved, points, &num_of_points, &i);
          } else if (moving == 2) {
            move_to_square(&to_up_moved_count, &to_up_moved_arr_len, to_up_moved, points, &num_of_points, &i);
          } else if (moving == 3) {
            move_to_square(&to_down_moved_count, &to_down_moved_arr_len, to_down_moved, points, &num_of_points, &i);
          }
        }
        ++i;
      }
    }
    MPI_Request requests_to[4];
    MPI_Request requests_from[4];
    
    int from_left_moved_count = 0;
    int from_right_moved_count = 0;
    int from_up_moved_count = 0;
    int from_down_moved_count = 0;
    
    /*
     0 - left
     1 - right
     2 - up
     3 - down
     */
    MPI_Issend(&to_left_moved_count, 1, MPI_INT, neighbours[0], 0, MPI_COMM_WORLD, requests_from + 0);
    MPI_Irecv(&from_left_moved_count, 1, MPI_INT, neighbours[0], 1, MPI_COMM_WORLD, requests_to + 0);
    
    MPI_Issend(&to_right_moved_count, 1, MPI_INT, neighbours[1], 1, MPI_COMM_WORLD, requests_from + 1);
    MPI_Irecv(&from_right_moved_count, 1, MPI_INT, neighbours[1], 0, MPI_COMM_WORLD, requests_to + 1);
    
    MPI_Issend(&to_up_moved_count, 1, MPI_INT, neighbours[2], 2, MPI_COMM_WORLD, requests_from + 2);
    MPI_Irecv(&from_up_moved_count, 1, MPI_INT, neighbours[2], 3, MPI_COMM_WORLD, requests_to + 2);
    
    MPI_Issend(&to_down_moved_count, 1, MPI_INT, neighbours[3], 3, MPI_COMM_WORLD, requests_from + 3);
    MPI_Irecv(&from_down_moved_count, 1, MPI_INT, neighbours[3], 2, MPI_COMM_WORLD, requests_to + 3);
    
    MPI_Status status_to[4];
    MPI_Status status_from[4];
    
    for(int j = 0; j < 4; ++j) {
      MPI_Wait(requests_from + j, status_to + j);
      MPI_Wait(requests_to + j, status_from + j);
    }
    
    point *from_left_moved = (point*)malloc(from_left_moved_count * sizeof(point));
    point *from_right_moved = (point*)malloc(from_right_moved_count * sizeof(point));
    point *from_up_moved = (point*)malloc(from_up_moved_count * sizeof(point));
    point *from_down_moved = (point*)malloc(from_down_moved_count * sizeof(point));

    MPI_Request req_from[4];
    MPI_Request req_to[4];
    
    MPI_Status statusto[4];
    MPI_Status statusfrom[4];
    
    /*
     0 - left
     1 - right
     2 - up
     3 - down
     */
    MPI_Isend(to_left_moved, to_left_moved_count * sizeof(point), MPI_BYTE, neighbours[0], 0, MPI_COMM_WORLD, req_from);
    MPI_Irecv(from_left_moved, from_left_moved_count * sizeof(point), MPI_BYTE, neighbours[0], 1, MPI_COMM_WORLD, req_to);
    
    MPI_Isend(to_right_moved, to_right_moved_count * sizeof(point), MPI_BYTE, neighbours[1], 1, MPI_COMM_WORLD, req_from + 1);
    MPI_Irecv(from_right_moved, from_right_moved_count * sizeof(point), MPI_BYTE, neighbours[1], 0, MPI_COMM_WORLD, req_to + 1);
    
    MPI_Isend(to_up_moved, to_up_moved_count * sizeof(point), MPI_BYTE, neighbours[2], 2, MPI_COMM_WORLD, req_from + 2);
    MPI_Irecv(from_up_moved, from_up_moved_count * sizeof(point), MPI_BYTE, neighbours[2], 3, MPI_COMM_WORLD, req_to + 2);
    
    MPI_Isend(to_down_moved, to_down_moved_count * sizeof(point), MPI_BYTE, neighbours[3], 3, MPI_COMM_WORLD, req_from + 3);
    MPI_Irecv(from_down_moved, from_down_moved_count * sizeof(point), MPI_BYTE, neighbours[3], 2, MPI_COMM_WORLD, req_to + 3);
    
    for(int j = 0; j < 4; ++j) {
      MPI_Wait(req_from + j, statusto + j);
      MPI_Wait(req_to + j, statusfrom + j);
    }
    
    replace_points(ctx, &num_of_points, from_left_moved, &from_left_moved_count, points, &points_arr_actual_len);
    replace_points(ctx, &num_of_points, from_right_moved, &from_right_moved_count, points, &points_arr_actual_len);
    replace_points(ctx, &num_of_points, from_up_moved, &from_up_moved_count, points, &points_arr_actual_len);
    replace_points(ctx, &num_of_points, from_down_moved, &from_down_moved_count, points, &points_arr_actual_len);
    
    int all_stopped_points_count = 0;
    MPI_Reduce(&stopped_points_count, &all_stopped_points_count, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    MPI_Request* requests_send = (MPI_Request*)malloc(sizeof(MPI_Request) * ctx->size);
    MPI_Request* requests_recv = (MPI_Request*)malloc(sizeof(MPI_Request) * ctx->size);
    
    if (ctx->rank == 0) {
      if (all_stopped_points_count == ctx->size * ctx->N) {
        flag = 1;
      } else {
        flag = 0;
      }
      for (int j = 0; j < ctx->size; ++j) {
        MPI_Isend(&flag, 1 ,MPI_INT, j, 0, MPI_COMM_WORLD, requests_send + j);
      }
    }
    MPI_Irecv(&flag, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, requests_recv + ctx->rank);
    MPI_Barrier(MPI_COMM_WORLD);
    
    free(requests_send);
    free(requests_recv);
    free(from_left_moved);
    free(from_right_moved);
    free(from_up_moved);
    free(from_down_moved);
  }
  if (ctx->rank == 0) {
    gettimeofday(&finish, NULL);
  }
  
  int *result = (int*)calloc(ctx->size * 2, sizeof(int));
  int rank_and_count[2];
  rank_and_count[0] = ctx->rank;
  rank_and_count[1] = stopped_points_count;
  
  MPI_Gather(rank_and_count, 2, MPI_INT, result, 2, MPI_INT, 0, MPI_COMM_WORLD);

  if (ctx->rank == 0) {
    double prog_work_time = 0;
    prog_work_time = finish.tv_sec - start.tv_sec + (finish.tv_usec - start.tv_usec) / 1.e6;
    FILE *stats;
    stats = fopen("stats.txt", "w");
    fprintf(stats, "%d %d %d %d %d %.2lf %.2lf %.2lf %.2lf %.5lfs\n", ctx->l, ctx->a, ctx->b, ctx->n, ctx->N, ctx->p_l, ctx->p_r, ctx->p_u, ctx->p_d, prog_work_time);
    
    for (int i = 0; i < 2 * ctx->size; i = i + 2){
      fprintf(stats, "%d: %d\n", result[i], result[i+1]);
    }
    fclose(stats);
  }
  
  free(result);
  free(points);
  free(to_left_moved);
  free(to_right_moved);
  free(to_up_moved);
  free(to_down_moved);
}

int main(int argc, char **argv) {
  MPI_Init(&argc, &argv);
  int rank;
  int size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  
  if (argc != 10) {
    printf("Incorrect input!\n");
  }
  int l = atoi(argv[1]);
  int a = atoi(argv[2]);
  int b = atoi(argv[3]);
  int n = atoi(argv[4]);
  int N = atoi(argv[5]);
  double p_l = atof(argv[6]);
  double p_r = atof(argv[7]);
  double p_u = atof(argv[8]);
  double p_d = atof(argv[9]);
  rand_walk_2d_ctx ctx = {
    .rank = rank,
    .size = size,
    .l = l,
    .a = a,
    .b = b,
    .n = n,
    .N = N,
    .p_l = p_l,
    .p_r = p_r,
    .p_u = p_u,
    .p_d = p_d,
  };
  random_walk_2d(&ctx);
  MPI_Finalize();
  return 0;
}

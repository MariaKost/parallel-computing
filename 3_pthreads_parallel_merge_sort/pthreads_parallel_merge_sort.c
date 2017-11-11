#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

typedef struct par_merge_sort_ctx {
  int *arr;
  int *tmp_arr;
  int m;
  int l;
  int r;
  int num_of_aval_threads;
  pthread_t *threads;
} par_merge_sort_ctx;

typedef struct merge_ctx {
  int *arr;
  int *tmp_arr;
  int l1;
  int r1;
  int l2;
  int r2;
  int tmp_arr_index;
} merge_ctx;

int cmp(const void *a, const void *b) {
  return (*(int*) a - *(int*) b);
}

int binsearch(int *arr, int l, int r, int target) {
  if (r - l == 1) {
    if (arr[l] >= target) {
      return l;
    } else {
      return l + 1;
    }
  }
  int middle = (r + l) / 2;
  if (arr[middle] == target) {
    return middle;
  } else {
    if (target > arr[middle]) {
      return binsearch(arr, middle, r, target);
    } else {
      return binsearch(arr, l, middle, target);
    }
  }
}

void *merge(void *context) {
  merge_ctx *ctx = context;
  int index1 = ctx->l1;
  int index2 = ctx->l2;
  while (index1 < ctx->r1 && index2 < ctx->r2) {
    if (ctx->arr[index1] < ctx->arr[index2]) {
      ctx->tmp_arr[ctx->tmp_arr_index++] = ctx->arr[index1++];
    } else {
      ctx->tmp_arr[ctx->tmp_arr_index++] = ctx->arr[index2++];
    }
  }
  while (index1 < ctx->r1) {
    ctx->tmp_arr[ctx->tmp_arr_index++] = ctx->arr[index1++];
  }
  while (index2 < ctx->r2) {
    ctx->tmp_arr[ctx->tmp_arr_index++] = ctx->arr[index2++];
  }
  return NULL;
}

void merge_sort_single_process(par_merge_sort_ctx *ctx) {
  if (ctx->r - ctx->l <= ctx->m) {
    qsort(ctx->arr + ctx->l, ctx->r - ctx->l, sizeof(int), cmp);
    return;
  }
  int middle = (ctx->l + ctx-> r) / 2;
  par_merge_sort_ctx left_sort_ctx = {
    .arr = ctx->arr,
    .tmp_arr = ctx->tmp_arr,
    .m = ctx->m,
    .l = ctx->l,
    .r = middle
  };
  par_merge_sort_ctx right_sort_ctx = {
    .arr = ctx->arr,
    .tmp_arr = ctx->tmp_arr,
    .m = ctx->m,
    .l = middle,
    .r = ctx->r
  };
  
  merge_sort_single_process(&left_sort_ctx);
  merge_sort_single_process(&right_sort_ctx);
  
  int left_chunk_middle = (ctx->l + middle) / 2;
  int right_chunk_middle = binsearch(ctx->arr, middle, ctx->r, ctx->arr[left_chunk_middle]);
  int new_middle_index = left_chunk_middle + (right_chunk_middle - middle);
  ctx->tmp_arr[new_middle_index] = ctx->arr[left_chunk_middle];
  
  merge_ctx left_merge_ctx = {
    .arr = ctx->arr,
    .tmp_arr = ctx->tmp_arr,
    .l1 = ctx->l,
    .r1 = left_chunk_middle,
    .l2 = middle,
    .r2 = right_chunk_middle,
    .tmp_arr_index = ctx->l
  };
  merge_ctx right_merge_ctx = {
    .arr = ctx->arr,
    .tmp_arr = ctx->tmp_arr,
    .l1 = left_chunk_middle + 1,
    .r1 = middle,
    .l2 = right_chunk_middle,
    .r2 = ctx->r,
    .tmp_arr_index = new_middle_index + 1
  };
  
  merge(&left_merge_ctx);
  merge(&right_merge_ctx);
  
  memcpy(ctx->arr + ctx->l, ctx->tmp_arr + ctx->l, sizeof(int) * (ctx->r - ctx->l));
}

void *parallel_merge_sort(void *context) {
  par_merge_sort_ctx *ctx = context;
  if (ctx->num_of_aval_threads <= 1) {
    merge_sort_single_process(ctx);
    return NULL;
  }
  int middle = (ctx->l + ctx-> r) / 2;
  
  par_merge_sort_ctx left_sort_ctx = {
    .arr = ctx->arr,
    .tmp_arr = ctx->tmp_arr,
    .m = ctx->m,
    .l = ctx->l,
    .r = middle,
    .num_of_aval_threads = (ctx->num_of_aval_threads - 2) / 2,
    .threads = ctx->threads
  };
  par_merge_sort_ctx right_sort_ctx = {
    .arr = ctx->arr,
    .tmp_arr = ctx->tmp_arr,
    .m = ctx->m,
    .l = middle,
    .r = ctx->r,
    .num_of_aval_threads = (ctx->num_of_aval_threads - 2) - ((ctx->num_of_aval_threads - 2) / 2),
    .threads = ctx->threads
  };
  pthread_t left_thread_sort = *(ctx->threads + ctx->num_of_aval_threads - 1);
  pthread_t right_thread_sort = *(ctx->threads + ctx->num_of_aval_threads - 2);
  
  pthread_create(&left_thread_sort, NULL, parallel_merge_sort, &left_sort_ctx);
  pthread_create(&right_thread_sort, NULL, parallel_merge_sort, &right_sort_ctx);
  pthread_join(left_thread_sort, NULL);
  pthread_join(right_thread_sort, NULL);
  
  int left_chunk_middle = (ctx->l + middle) / 2;
  int right_chunk_middle = binsearch(ctx->arr, middle, ctx->r, ctx->arr[left_chunk_middle]);
  int new_middle_index = left_chunk_middle + (right_chunk_middle - middle);
  ctx->tmp_arr[new_middle_index] = ctx->arr[left_chunk_middle];
  
  merge_ctx left_merge_ctx = {
    .arr = ctx->arr,
    .tmp_arr = ctx->tmp_arr,
    .l1 = ctx->l,
    .r1 = left_chunk_middle,
    .l2 = middle,
    .r2 = right_chunk_middle,
    .tmp_arr_index = ctx->l
  };
  merge_ctx right_merge_ctx = {
    .arr = ctx->arr,
    .tmp_arr = ctx->tmp_arr,
    .l1 = left_chunk_middle + 1,
    .r1 = middle,
    .l2 = right_chunk_middle,
    .r2 = ctx->r,
    .tmp_arr_index = new_middle_index + 1
  };
  if (ctx->num_of_aval_threads <= 1) {
    merge(&left_merge_ctx);
    merge(&right_merge_ctx);
    memcpy(ctx->arr + ctx->l, ctx->tmp_arr + ctx->l, sizeof(int) * (ctx->r - ctx->l));
    return NULL;
  }
  pthread_t left_thread_merge = *(ctx->threads + ctx->num_of_aval_threads - 1);
  pthread_t right_thread_merge = *(ctx->threads + ctx->num_of_aval_threads - 2);
  
  pthread_create(&left_thread_merge, NULL, merge, &left_merge_ctx);
  pthread_create(&right_thread_merge, NULL, merge, &right_merge_ctx);
  pthread_join(left_thread_merge, NULL);
  pthread_join(right_thread_merge, NULL);
  
  memcpy(ctx->arr + ctx->l, ctx->tmp_arr + ctx->l, sizeof(int) * (ctx->r - ctx->l));
  return NULL;
}


int main(int argc, char **argv) {
  if (argc != 4) {
    printf("Incorrect input!\n");
  }
  FILE *stats = fopen("stats.txt", "a");
  FILE *data = fopen("data.txt", "w");
  if (stats == NULL || data == NULL) {
    printf("Error opening file!\n");
    exit(1);
  }
  int n = atoi(argv[1]);
  int m = atoi(argv[2]);
  int P = atoi(argv[3]);
  int *arr = (int*)calloc(n, sizeof(int));
  int *qsort_arr = (int*)calloc(n, sizeof(int));
  int *tmp_arr = (int*)calloc(n, sizeof(int));
  pthread_t threads[P];
  par_merge_sort_ctx ctx = {
    .arr = arr,
    .tmp_arr = tmp_arr,
    .m = m,
    .l = 0,
    .r = n,
    .num_of_aval_threads = P,
    .threads = threads
  };
  srand(time(NULL));
  for (int i = 0; i < n; ++i) {
    ctx.arr[i] = rand() % 1000;
    qsort_arr[i] = ctx.arr[i];
    fprintf(data, "%d ", ctx.arr[i]);
  }
  fprintf(data, "\n");
  
  double prog_work_time = 0;
  struct timeval start, finish;
  
  if (P == 0) {
    gettimeofday(&start, NULL);
    merge_sort_single_process(&ctx);
    gettimeofday(&finish, NULL);
  }
  if (P > 0) {
    gettimeofday(&start, NULL);
    parallel_merge_sort(&ctx);
    gettimeofday(&finish, NULL);
  }
  prog_work_time = finish.tv_sec - start.tv_sec + (finish.tv_usec - start.tv_usec) / 1.e6;
  fprintf(stats, "%.5lfs %d %d %d\n", prog_work_time, n, m, P);
  for (int i = 0; i < n; ++i) {
    fprintf(data, "%d ", ctx.arr[i]);
  }
  fprintf(data, "\n");
  
  // for measuring qsort execution time
//  double qsort_prog_work_time = 0;
//  struct timeval q_start, q_finish;
//  gettimeofday(&q_start, NULL);
//  qsort(qsort_arr, n, sizeof(int), cmp);
//  gettimeofday(&q_finish, NULL);
//  qsort_prog_work_time = q_finish.tv_sec - q_start.tv_sec + (q_finish.tv_usec - q_start.tv_usec) / 1.e6;
//  fprintf(stats, "qsort: %.5lfs\n", qsort_prog_work_time);
  
  free(tmp_arr);
  free(qsort_arr);
  free(arr);
  fclose(stats);
  fclose(data);
  return 0;
}

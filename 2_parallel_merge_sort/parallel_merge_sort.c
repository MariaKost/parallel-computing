//
//  parallel_merge_sort.c
//  
//
//  Created by Emil Iksanov on 28.10.2017.
//

#include <omp.h>
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

void merge(merge_ctx *ctx) {
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


void parallel_merge_sort(par_merge_sort_ctx *ctx) {
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
  #pragma omp parallel sections
  {
    #pragma omp section
    {
      parallel_merge_sort(&left_sort_ctx);
    }
    #pragma omp section
    {
      parallel_merge_sort(&right_sort_ctx);
    }
  }
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
  #pragma omp parallel sections
  {
    #pragma omp section
    {
      merge(&left_merge_ctx);
    }
    #pragma omp section
    {
      merge(&right_merge_ctx);
    }
  }
  memcpy(ctx->arr + ctx->l, ctx->tmp_arr + ctx->l, sizeof(int) * (ctx->r - ctx->l));
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
  par_merge_sort_ctx ctx = {
    .arr = arr,
    .tmp_arr = tmp_arr,
    .m = m,
    .l = 0,
    .r = n
  };
  srand(time(NULL));
  for (int i = 0; i < n; ++i) {
    ctx.arr[i] = rand() % 1000;
    qsort_arr[i] = ctx.arr[i];
    fprintf(data, "%d ", ctx.arr[i]);
  }
  fprintf(data, "\n");
  
  double prog_work_time = 0;
  double ts = 0;  // time of start
  double tf = 0;  // time of finish
  
  double qsort_prog_work_time = 0;
  double q_ts = 0;  // time of start
  double q_tf = 0;  // time of finish
  
  if (P == 0) {
    ts = omp_get_wtime();
    merge_sort_single_process(&ctx);
    tf = omp_get_wtime();
  }
  if (P > 0) {
    omp_set_num_threads(P);
    ts = omp_get_wtime();
    parallel_merge_sort(&ctx);
    tf = omp_get_wtime();
  }
  prog_work_time = tf - ts;
  
//  q_ts = omp_get_wtime();
//  qsort(qsort_arr, n, sizeof(int), cmp);
//  q_tf = omp_get_wtime();
//  qsort_prog_work_time = q_tf - q_ts;
  
  fprintf(stats, "%.5lfs %d %d %d\n", prog_work_time, n, m, P);
//  fprintf(stats, "qsort: %.5lfs\n", qsort_prog_work_time);
  
  for (int i = 0; i < n; ++i) {
    fprintf(data, "%d ", ctx.arr[i]);
  }
  fprintf(data, "\n");
  
  free(arr);
  free(tmp_arr);
  fclose(stats);
  fclose(data);
  return 0;
}

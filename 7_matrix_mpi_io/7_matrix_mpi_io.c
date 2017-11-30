#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>
#include <sys/time.h>

typedef struct point {
    int x;
    int y;
    int r;
} point;

typedef struct mpi_data {
    int rank;
    int size;
    int l;
    int a;
    int b;
    int N;
    point *points;
} mpi_data;

void generate_points(mpi_data* par){
    int seed;
    int* seeds = (int*)calloc((size_t)par->size, sizeof(int));

    if (par->rank == 0) {
        srand(time(NULL));
        for (int i = 0; i < par->size; ++i) {
            seeds[i] = rand();
        }
    }

    MPI_Scatter(seeds, 1, MPI_UNSIGNED, &seed, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
    free(seeds);

    srand(time(NULL));
    for (int i = 0; i < par->N; ++i) {
        par->points[i].x = rand_r(&seed) % par->l;
        par->points[i].y = rand_r(&seed) % par->l;
        par->points[i].r = rand_r(&seed) % (par->a * par->b);
    }
};

void analysis (mpi_data* par) {
    int *table = (int*) calloc((size_t)(par->l * par->l * par->size), sizeof(int));
    for(int i = 0; i < par->N; ++i) {
        int x = par->points[i].x;
        int y = par->points[i].y;
        int r = par->points[i].r;
        table[y * par->l * par->size + x * par->size + r] += 1;
    }

    MPI_Aint intex;
    MPI_Type_extent(MPI_INT, &intex);

    MPI_File fh;
    MPI_File_open(MPI_COMM_WORLD, "data.bin", MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &fh);

    MPI_Datatype view;
    MPI_Type_vector(par->l, par->l * par->size, par->l * par->a * par->size, MPI_INT, &view);
    MPI_Type_commit(&view);

    int offset = ((par->rank % par->a * par->l) + (par->rank / par->a * par->a * par->l * par->l)) * par->size;
    offset = offset * intex;
    MPI_File_set_view(fh, offset, MPI_INT, view, "native", MPI_INFO_NULL);

    MPI_File_write(fh, table, par->l * par->l * par->size, MPI_INT, MPI_STATUS_IGNORE);

    MPI_Type_free(&view);
    MPI_File_close(&fh);
    free(table);
}

int main(int argc, char * argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 5) {
        printf("Wrong number of arguments\n");
    }

    mpi_data par = {
            .size = size,
            .rank = rank,
            .l = atoi(argv[1]),
            .a = atoi(argv[2]),
            .b = atoi(argv[3]),
            .N = atoi(argv[4]),
            .points = (point*)calloc((size_t)par.N, sizeof(point)),
    };


    struct timeval start, finish;
    if (rank == 0) {
        gettimeofday(&start, NULL);;
    }

    generate_points(&par);
    analysis(&par);

    if (rank == 0) {
        gettimeofday(&finish, NULL);
        double prog_work_time = 0;
        prog_work_time = finish.tv_sec - start.tv_sec + (finish.tv_usec - start.tv_usec) / 1.e6;

        FILE *f = fopen("stats.txt", "a");
        if (f == NULL) {
            printf("Error opening file!\n");
            exit(1);
        }
        fprintf(f, "%d %d %d %d %.5lfs\n", par.l, par.a, par.b, par.N, prog_work_time);
        fclose(f);
    }

    free(par.points);
    MPI_Finalize();
    return 0;
}

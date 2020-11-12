#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// Multiplies matricies together
void callProcess(int* result, int index, int n_columns_a, int (*matrix_a)[n_columns_a], int n_rows_a, int n_columns_b, int (*matrix_b)[n_columns_b], int n_rows_b) {
    for (int k = 0; k < n_columns_a; ++k) {
        // Performs result[row][column] += matrix_a[row][k] * matrix_b[k][column] for an array
        result[index] += matrix_a[index/n_columns_b][k] * matrix_b[k][index%n_columns_b];
    }
}

//Prints an m rows by n columns matrix that is stored as an array
void printMatrix(int* arr, int m, int n) {
    for (int i = 1; i <= (m * n); ++i) {
        printf("%d ", arr[i-1]);
        if ((i % n) == 0){
            printf("\n");
        }
    }
}

void multiplyMatrices(int n_columns_a, int (*matrix_a)[n_columns_a], int n_rows_a, int n_columns_b, int (*matrix_b)[n_columns_b], int n_rows_b) {
    //Make a matrix, n_rows_a by n_columns_b to 0.
    int shmid = shmget(IPC_PRIVATE,sizeof(int)*n_rows_a*n_columns_b,S_IRUSR|S_IWUSR);
    int* result = (int *)shmat(shmid,NULL,0);

    pid_t pid;
    for (int i = 0; i < n_rows_a * n_columns_b; ++i) {
        pid = fork();
        if (pid < 0) {
            printf("Error");
            exit(1);
        } else if (pid == 0) {
            callProcess(result, i, n_columns_a, matrix_a, n_rows_a, n_columns_b, matrix_b, n_rows_b);
            exit(0);
        } else {
            wait(NULL);
        }
   }
 
    printMatrix(result, n_rows_a, n_columns_b);
    //Detach for program to use it again
    shmdt(result);
    //Unlike attach and detach, this will delete the shared memory
    shmctl(shmid, IPC_RMID, 0);
}

int main(int argc, char **argv){
    //count number of rows and columns
    int n_rows = 0;
    char strvar[100];
    int n_columns = 0;
    int first_n_columns = -1; 
    int n_rows_a, n_columns_a, n_rows_b, n_columns_b;

    while(fgets (strvar, 100, stdin)) {
        if (!strcmp(strvar, "\n")) {
            //done counting matrix A
            n_rows_a = n_rows;
            n_columns_a = n_columns;
            n_rows = 0;
            first_n_columns = -1; 
        } else {
            //count number of columns
            n_columns = 0;
            int strvar_length = strlen(strvar);
            for (int i=0; i < strvar_length; i++) {
                //last digit in row or there is a space after it
                if (isdigit(strvar[i]) && (i == (strvar_length - 1) || isspace(strvar[i+1]))) {
                    n_columns++;
                } else if (!isdigit(strvar[i]) && !isspace(strvar[i])) {
                    printf("error: only accepts matrix of digits\n");
                    return 1;
                }
            }
            if (first_n_columns == -1) {
                first_n_columns = n_columns;
            } else if (first_n_columns != n_columns) {
                printf("error: invalid matrix due to inconsistent number of columns\n");
                return 1;
            }
            n_rows++;
        }
    }
    n_rows_b = n_rows;
    n_columns_b = n_columns;

    if (n_columns_a != n_rows_b){
        printf("error: unable to multiply a %d by %d matrix with a %d by %d matrix\n", n_rows_a, n_columns_a, n_rows_b, n_columns_b);
        return 1;
    }

    //dynamically create array to store matrices
    //matrix_a is n_rows_a by n_columns_a
    //matrix_b is n_rows_b by n_columns_b
    rewind(stdin);
    int matrix_a[n_rows_a][n_columns_a];
    int i, j;
    for (i=0; i < n_rows_a; i++) {
        for (j=0; j < n_columns_a; j++) {
            scanf("%d", &matrix_a[i][j]);
        }
    }

    int matrix_b[n_rows_b][n_columns_b];
    for (i=0; i < n_rows_b; i++) {
        for (j=0; j < n_columns_b; j++) {
            scanf("%d", &matrix_b[i][j]);
        }
    }

    multiplyMatrices(n_columns_a, matrix_a, n_rows_a, n_columns_b, matrix_b, n_rows_b);

    return 0;
}

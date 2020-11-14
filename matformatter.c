#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

 int main(int argc, char **argv) {
    char strvar[100];
    int n_rows = 0;
    int n_columns = 0;

    while(fgets (strvar, 100, stdin)) {
        if (!strcmp(strvar, "\n")) {
            break;
        } else {
            //count number of columns
            n_columns = 0;
            int strvar_length = strlen(strvar);
            for (int i=0; i < strvar_length; i++) {
                //last digit in row or there is a space after it
                if (isdigit(strvar[i]) && (i == (strvar_length - 1) || isspace(strvar[i+1]))) {
                    n_columns++;
                }
            }
            n_rows++;
        }
    }

    rewind(stdin);
    int matrix_a[n_rows][n_columns];
    int i, j;
    for (i=0; i < n_rows; i++) {
        for (j=0; j < n_columns; j++) {
            scanf("%d", &matrix_a[i][j]);
        }
    }

    int transpose[n_rows][n_columns];
    for (i = 0; i < n_rows; ++i)
        for (j = 0; j < n_columns; ++j) {
            transpose[j][i] = matrix_a[i][j];
        }

    for (i = 0; i < n_columns; ++i)
        for (j = 0; j < n_rows; ++j) {
            printf("%d ", transpose[i][j]);
            if (j == n_rows - 1)
                printf("\n");
        }
 }

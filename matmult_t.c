#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <setjmp.h>
#include <ctype.h>
#include <unistd.h>
#include <math.h>

#define MAX_THREADS 100
#define STACK_SIZE  16384
#define TEMP_FILE   "./matmult_t_temp.txt"

enum boolean{false, true};

//num of threads
int max_threads;           

// matrices.
int **a;                   
int **b;
int **c;

// dimensions of matrices
int a_rows = 0;            
int a_cols = 0;
int b_rows = 0;
int b_cols = 0;

//source code from: "Portable Multithreading: The Signal Stack Trick For User-Space Thread Creation"
static jmp_buf finish;     

typedef struct mctx_t {
    jmp_buf jb;
} mctx_t;
/* Save machine context. */
#define mctx_save(mctx) setjmp((mctx)->jb)

/* Restore machine context. */
#define mctx_restore(mctx) longjmp((mctx)->jb, 1)

/* Switch machine context. */
#define mctx_switch(mctx_old, mctx_new) if (setjmp((mctx_old)->jb) == 0) longjmp((mctx_new)->jb,1) 

//not from there but added in combination
static mctx_t uc[MAX_THREADS]; 
static char * stacks[MAX_THREADS];

//source code from: "Portable Multithreading: The Signal Stack Trick For User-Space Thread Creation"
static mctx_t mctx_caller;
static sig_atomic_t mctx_called;

static mctx_t *mctx_creat;
static void (*mctx_creat_func)(void *);
static void *mctx_creat_arg;
static sigset_t mctx_creat_sigs;

//back to not source code
int countline(char *line) {
    int total = 0;
    while (*line != '\n') {
        if (isdigit(*line) || (*line == '-')) {
            total++;
            while (isdigit(*(++line)));
        } 
        else if (isspace(*line)) {
            line++;
        }
        else {
            printf("Invalid input character '%c'. Terminating.\n", *line);
            exit(1);
        }
    }
    return total;
}


int **initialize_array(int rows, int cols) {
    int **array;
    array = (int**) malloc(rows * sizeof (int*));
    int i;
    for (i = 0; i < rows; i++)
        array[i] = (int*) malloc(cols * sizeof (int));
    return array;
}


void free_array(int **array, int rows) {
    int i;
    for (i = 0; i < rows; i++)
        free(array[i]);
    free(array);
}


void fill_array(int *array, char *line) {
    char *pch;
    int i = 0;
    pch = strtok(line, " ");
    while (pch != NULL) {
        array[i++] = atoi(pch);
        pch = strtok(NULL, " ");
    }
}


void multiply(int num) {
    int k = (num - 1) % (b_cols);
    double w = floor((num - 1)/b_cols);
    int i = (int)w;
    int j;
    for (j = 0; j < a_cols; j++) 
        c[i][k] += a[i][j] * b[j][k];
    if (num < max_threads)
        mctx_switch(&uc[num], &uc[num + 1]);
    longjmp(finish, 1);

}

//source code from: "Portable Multithreading: The Signal Stack Trick For User-Space Thread Creation"
void mctx_create(mctx_t *mctx, void (*sf_addr)(void *), void *sf_arg, void *sk_addr, size_t sk_size) {

    struct sigaction sa;
    struct sigaction osa;
    stack_t  ss;
    stack_t  oss;
    sigset_t osigs;
    sigset_t sigs;

    /* Step 1: */
    sigemptyset(&sigs);
    sigaddset(&sigs, SIGUSR1);
    sigprocmask(SIG_BLOCK, &sigs, &osigs);

    /* Step 2: */
    memset((void *) &sa, 0, sizeof (struct sigaction));
    sa.sa_handler = mctx_create_trampoline;
    sa.sa_flags = SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, &osa);

    /* Step 3: */
    ss.ss_sp = sk_addr;
    ss.ss_size = sk_size;
    ss.ss_flags = 0;
    sigaltstack(&ss, &oss);

    /* Step 4: */
    mctx_creat = mctx;
    mctx_creat_func = sf_addr;
    mctx_creat_arg = sf_arg;
    mctx_creat_sigs = osigs;
    mctx_called = false;
    kill(getpid(), SIGUSR1);
    sigfillset(&sigs);
    sigdelset(&sigs, SIGUSR1);

    while (!mctx_called) {
        sigsuspend(&sigs);
    }

    /* Step 6: */
    sigaltstack(NULL, &ss);
    ss.ss_flags = SS_DISABLE;
    sigaltstack(&ss, NULL);

    if (!(oss.ss_flags & SS_DISABLE))
        sigaltstack(&oss, NULL);

    sigaction(SIGUSR1, &osa, NULL);
    sigprocmask(SIG_SETMASK, &osigs, NULL);

    /* Step 7 and Step 8: */
    mctx_switch(&mctx_caller, mctx);

    /* Step 14: */
    return;
}

void mctx_create_trampoline(int sig) {
    /* Step 5: */
    if (mctx_save(mctx_creat) == 0) {
        mctx_called = true;
        return;
    }
    /* Step 9: */
    mctx_create_boot();
}

void mctx_create_boot(void) {

    void (*mctx_start_func)(void *);
    void *mctx_start_arg;

    /* Step 10: */
    sigprocmask(SIG_SETMASK, &mctx_creat_sigs, NULL);

    /* Step 11: */
    mctx_start_func = mctx_creat_func;
    mctx_start_arg = mctx_creat_arg;

    /* Step 12 and Step 13: */
    mctx_switch(mctx_creat, &mctx_caller);

    /* The thread "magically" starts... */
    mctx_start_func(mctx_start_arg);

    /* NOTREACHED */
    abort();
}
//back to my function
void my_thr_create(void (*func) (int), int thr_id) {
    int size = STACK_SIZE / 2;
    stacks[thr_id] = (char *) malloc(STACK_SIZE);
    mctx_create(&uc[thr_id], (void (*) (void *)) func,
            (void *) thr_id, stacks[thr_id] + size, size);
}
//my main again
int main(int argc, char *argv[]) {

    int nbytes = 256;
    char *line = (char *) malloc(nbytes + 1);

    int a = true;
    int b = true;

    FILE *fout;

    if ((fout = fopen(TEMP_FILE, "w+")) == NULL) {
        printf("Error: Cannot open file for processing matrices. Terminating.\n");
        exit(1);
    }

    while (a || b) {

        getline((char **)&line, (size_t *)&nbytes, stdin);
        fprintf(fout, "%s", line);

        int num = countline(line);
        if (num == 0) {
            if (a) {
                a = false;
                if (a_rows == 0) {
                    printf("error: no input for matrix A\n");
                    exit(1);
                }
            } else {
                b = false;
                if (b_rows == 0) {
                    printf("error: No input for matrix B\n");
                    exit(1);
                }
            }
        } else if (a) {
            a_rows++;
            if (a_cols == 0) {
                a_cols = num;
            } else if (num != a_cols) {
                printf("error: wrong number of columns in A\n");
                exit(1);
            }
        } else {
            b_rows++;
            if (b_cols == 0) {
                b_cols = num;
            } else if (num != b_cols) {
                printf("error: wrong number of columns in B\n");
                exit(1);
            }
        }

    }
    
    if (a_cols != b_rows) {
        printf("error: number of cols in A not equal to number of rows in B\n");
        exit(1);
    } 
    if (fseek(fout, 0, SEEK_SET) < 0) {
        printf("file error\n");
        exit(1);
    }
   
    a = initialize_array(a_rows, a_cols);
    b = initialize_array(b_rows, b_cols);
    c = initialize_array(a_rows, b_cols);
    
    int i;
    for (i = 0; i < a_rows; i++) {
        getline((char **)&line, (size_t *)&nbytes, fout);
        fill_array(a[i], line);
    }
	
    getline((char **)&line, (size_t *)&nbytes, fout); // space between input matrices

    for (i = 0; i < b_rows; i++) {
        getline((char **)&line, (size_t *)&nbytes, fout);
        fill_array(b[i], line);
    }

    free(line);
    fclose(fout);
    remove(TEMP_FILE);
   
    max_threads = a_rows * b_cols;
    for (i = 1; i <= max_threads; i++)
        my_thr_create(multiply, i);

    //printf("a_rows = %d.\na_cols = %d.\nb_rows = %d.\nb_cols = %d.\n", a_rows, a_cols, b_rows, b_cols); // debug
    
    if (setjmp(finish) == 0)
        mctx_switch(&mctx_caller, &uc[1]);

    // product matrix
    int j;
    for (i = 0; i < a_rows; i++) {
        for (j = 0; j < b_cols; j++) {
            printf("%d", c[i][j]);
	    if (j != b_cols - 1) {
		printf(" ");
	    }
        }
        printf("\n");   
    }

    free_array(a, a_rows);
    free_array(b, b_rows);
    free_array(c, a_rows);
    for (i = 1; i <= max_threads; i++)
        free(stacks[i]);
    
    return 0;
}

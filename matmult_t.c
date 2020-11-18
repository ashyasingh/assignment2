#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <string.h>

#define MATRIX_MAX 100
#define STACK_SIZE  65536

// save machine context
#define mctx_save(mctx) setjmp((mctx)->jb)

// restore machine context
#define mctx_restore(mctx) longjmp((mctx)->jb, 1)

// switch machine context
#define mctx_switch(mctx_old, mctx_new) if (setjmp((mctx_old)->jb) == 0) longjmp((mctx_new)->jb,1) 

enum boolean{false, true};

static int my_debug = 0;	//unit testing and debugging

//num of threads
static int max_threads, num_threads;           

// matrices
int a[MATRIX_MAX][MATRIX_MAX];                   
int b[MATRIX_MAX][MATRIX_MAX];
int c[MATRIX_MAX][MATRIX_MAX];

// dimensions of matrices
int a_rows;            
int a_cols;
int b_rows;
int b_cols;

//result matrix
int result[MATRIX_MAX][MATRIX_MAX];

//source code from: "Portable Multithreading: The Signal Stack Trick For User-Space Thread Creation" but modified accordingly!! 

//machine context!
typedef struct mctx_t {
    jmp_buf jb;
} mctx_t;

static jmp_buf finish;   

static mctx_t uc[MATRIX_MAX*MATRIX_MAX]; 
static char * stacks[MATRIX_MAX*MATRIX_MAX];

static mctx_t mctx_caller;
static sig_atomic_t mctx_called;

static mctx_t *mctx_creat;
static void (*mctx_creat_func)(int *);
static int *mctx_creat_arg;
static sigset_t mctx_creat_sigs;

//source code from: "Portable Multithreading: The Signal Stack Trick For User-Space Thread Creation"
/*
* this function calls another function to bootstrap into a new stack 
* it's going to save the machine context but it's also going to get the 
* machine context from mctx_create and gives control back to it
*
*/
void mctx_create_boot(void) {

    void (*mctx_start_func)(int *);
    int *mctx_start_arg;

    // step 10: set new signal mask = old signal mask
    sigprocmask(SIG_SETMASK, &mctx_creat_sigs, NULL);

    // step 11: global variables = local variables
    mctx_start_func = mctx_creat_func;
    mctx_start_arg = mctx_creat_arg;

    if (my_debug) printf("%s: call start_func %p (&%p) for arg %p\n", __FUNCTION__, mctx_start_func, &mctx_start_func, mctx_start_arg);

    // step 12 & 13: transfer control back to original machine context
    mctx_switch(mctx_creat, &mctx_caller);

    if (my_debug) printf("%s: call start_func %p (&%p) for arg %p\n", __FUNCTION__, mctx_start_func, &mctx_start_func, mctx_start_arg);

    // thread starts
    mctx_start_func(mctx_start_arg);

    abort();
}

//source code from: "Portable Multithreading: The Signal Stack Trick For User-Space Thread Creation"
/*
* this function saves the machine context and ends the signal handler scope
* now it gives control to clean stack frame
* calls the bootstrap function
*/
void mctx_create_trampoline(int sig) {

    // step 5: save machine context from mctx_create, returns to end signal handler scope
    if (mctx_save(mctx_creat) == 0) {
        if (my_debug) printf("%s: mctx_called true\n", __FUNCTION__);
        mctx_called = true;
        return;
    }

    if (my_debug) printf("%s: call mctx_create_boot\n", __FUNCTION__);
    // step 9: bootstrap to clean stack frame!
    mctx_create_boot();

}

//source code from: "Portable Multithreading: The Signal Stack Trick For User-Space Thread Creation"
/*
*	A function “void mctx create(mctx t
*	*mctx, void (*sf addr)(void *), void *sf arg,
*	void *sk addr, size t sk size)” which creates and
*	initializes a machine contextstructure in mctx with
*	a start function sf addr, a start function argument
*	
*	so it's creating the machine context for the thread, defining
*	all the signals, signal handler scope, signal mask and more, and then it goes to the trampoline function
*	giving it control
*/
void mctx_create(mctx_t *mctx, void (*sf_addr)(int *), int *sf_arg, void *sk_addr, size_t sk_size) {

    struct sigaction sa;
    struct sigaction osa;
    stack_t  ss;
    stack_t  oss;
    sigset_t osigs;
    sigset_t sigs;

    // step 1: preserve signal mask
    sigemptyset(&sigs);
    sigaddset(&sigs, SIGUSR1);
    sigprocmask(SIG_BLOCK, &sigs, &osigs);

    // step 2: preserve signal action, transer that to trampoline
    memset((void *) &sa, 0, sizeof (struct sigaction));
    sa.sa_handler = mctx_create_trampoline;
    sa.sa_flags = SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, &osa);

    // step 3: preserve signal stack
    ss.ss_sp = sk_addr;
    ss.ss_size = sk_size;
    ss.ss_flags = 0;
    sigaltstack(&ss, &oss);

    if (my_debug) printf("%s: mctx %p (sf_addr %p sf_arg %p) created sigaltstack %p (%lu)\n", __FUNCTION__, mctx, sf_addr, sf_arg, sk_addr, sk_size);

    // step 4: create global variables from trampoline arguments, signal to signal stack
    mctx_creat = mctx;
    mctx_creat_func = sf_addr;
    mctx_creat_arg = sf_arg;
    mctx_creat_sigs = osigs;
    mctx_called = false;
    kill(getpid(), SIGUSR1);
    sigfillset(&sigs);
    sigdelset(&sigs, SIGUSR1);

    if (my_debug) printf("%s: mctx %p mctx_called %d\n", __FUNCTION__, mctx, mctx_called);

    while (!mctx_called) {
        sigsuspend(&sigs);
    }

    if (my_debug) printf("%s: mctx %p mctx_called %d\n", __FUNCTION__, mctx, mctx_called);

    // step 6: restore the preserved signal mask, signal action, and signal stack from above
    sigaltstack(NULL, &ss);
    ss.ss_flags = SS_DISABLE;
    sigaltstack(&ss, NULL);

    if (!(oss.ss_flags & SS_DISABLE))
        sigaltstack(&oss, NULL);

    sigaction(SIGUSR1, &osa, NULL);
    sigprocmask(SIG_SETMASK, &osigs, NULL);

    if (my_debug) printf("%s: calling mctx_switch for %p\n", __FUNCTION__, mctx);

    // step 7 & 8: save machine context of mctx_create, transfer control to other stack
    mctx_switch(&mctx_caller, mctx);

    if (my_debug) printf("%s: return from mctx_switch for %p\n", __FUNCTION__, mctx);
    return;
}

//my multiply function which multiplies the matrices and saves it to the result matrix
void multiply(int thr_id) {
	int row = thr_id/b_cols;
	int col = thr_id%b_cols;
	int sum = 0;
	if (my_debug) printf("multiply: thr_id = %d, row = %d, col = %d\n", thr_id, row, col);
	for (int i = 0; i < a_cols; ++i){
		sum += a[row][i] * b[i][col];
	}
	result[row][col] = sum;
	if (my_debug) printf("multiply: thr_id = %d, result = %d\n", thr_id, result[row][col]);
	num_threads++;
	if (num_threads < max_threads){
	        if (my_debug) printf("multiply: mctx_switch %d -> %d\n", thr_id, thr_id+1);
		mctx_switch(&uc[thr_id], &uc[thr_id+1]);
	}
	
	if (my_debug) printf("multiply: thr_id %d longjmp\n", thr_id);
	longjmp(finish, 1);
}

//helper function
int number(char letter){
	return (letter - '0' < 10 && letter - '0' > -1);
}

//my function
/*
* creates a series of threads to help itself calculate the matrix multiplication
* the first argument is a pointer to a function for the thread to execute
* the second argument is a unique integer thread ID that you create for each new thread
* we use mctx_create here
*/
void my_thr_create(void (*func) (int), int thr_id) {
    int size = STACK_SIZE / 2;
    stacks[thr_id] = (char *) malloc(STACK_SIZE);
    if (my_debug) printf("my_thr_create: thr_id %d\n", thr_id);
    mctx_create(&uc[thr_id], func, thr_id, (void*) stacks[thr_id] + size, size);
}

//main function
/*
* get the input array, check if it's the right format
* call functions to create threads
* output product matrix
*/
int main(int argc, char *argv[]) {
	int thr_id;
	
	a_rows = 0;
	a_cols = 0;
	
	b_rows = 0;
	b_cols = 0;
	
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	int new = 0; //incrementing into a new array
	int fill = 0;//if fill = 0, matrix a. if fill = 1, matrix b.
	
	//parsing in matrix a and matrix b from stdin
	int i; //rows so C[i, k]
	while((read = getline(&line, &len, stdin))!=-1){
		int k = 0; //columns C[i, k] 
		
		//only 2 input matrices allowed
		if (line[0] == '\n'){
		    if (fill == 1)
		    	break;
		    fill = 1;
		} else {
		    new = 0;
		    if(line[0] == ' '){
			    k--;
		    }
		    for(i = 0; line[i] != '\0'; i++){
		    	if (new == 1 && number(line[i])){
			    if(i != 0 || line[i+1] == ' '){
                                    k++;
                                    new = 0;
                            }
			 } else if (line[i] == ' '&& line [i+1] != ' '){
				    new = 1;
			 }
			 if (number(line[i])){
//                             if (k >= MATRIX_MAX) {
//				printf("error: matrix must not have more than %d columns\n", MATRIX_MAX);
//				exit(0);
//			     }
         			if (fill == 0){
					a[a_rows][k] = a[a_rows][k]*10 + (line[i] - '0');
				} else {
					b[b_rows][k] = b[b_rows][k]*10 + (line[i] - '0');
				}	
			 }
		   }
		if (fill == 0) {
				if (a_cols == 0) {
					a_cols = k + 1;
				} else {
					if (a_cols != k + 1) {
						printf("error: matrix a doesn't have the same number of elements in each row\n");
						exit(0);
					}
				}
				a_rows++;

//				if (a_rows > MATRIX_MAX) {
  //                              	printf("error: matrix a must not have more than %d rows\n", MATRIX_MAX);
    //                            	exit(0);
       //                      	}	
		} else {
				if (b_cols == 0) {
					b_cols = k + 1;
				} else {
					if (b_cols != k + 1) {
						printf("error: matrix b doesn't have the same number of elements in each row\n");
						exit(0);
					}
				}
				b_rows++;

         //                    	if (b_rows > MATRIX_MAX) {
	//				printf("error: matrix b must not have more than %d rows\n", MATRIX_MAX);
	//				exit(0);
	//		     	}
		}
	}
   }
   if (a_cols != b_rows){
       printf("error: unable to multiply a %d by %d matrix with a %d by %d matrix\n", a_rows, a_cols, b_rows, b_cols);
       return 1;
   }
   else if (my_debug) {
	printf("multiply a %d by %d matrix with a %d by %d matrix\n", a_rows, a_cols, b_rows, b_cols);
	int i, j;	
	for (i = 0; i < a_rows; i++){
		for(j = 0; j < a_cols; j++){
			printf("%d ",a[i][j]);
		} 
		printf("\n");
	} 
	printf("\n");
	for (i = 0; i < b_rows; i++){
		for(j = 0; j < b_cols; j++){
			printf("%d ",b[i][j]);
		} 
		printf("\n");
	} 
	printf("\n");
 }

    //product matrix but thread style
    int rc, cc; //rows, columns								     
    for (rc = 0; rc < a_rows; rc++) {
        for (cc = 0; cc < b_cols; cc++) {
            thr_id = ((b_cols * rc) + cc);
	    my_thr_create(multiply, thr_id);
        }
    }

   max_threads = a_rows * b_cols;
   if (my_debug) printf("main: max_threads %d setjmp\n", max_threads);
   if (setjmp(finish) == 0){
        if (my_debug) printf("main: mctx_switch -> 0\n");
        mctx_switch(&mctx_caller, &uc[0]);
   }

   if (my_debug) printf("main: print result\n");
   //print result matrix to stdout
   for (rc = 0; rc < a_rows; rc++){
	for (cc = 0; cc < b_cols; cc++){
		printf("%d ", result[rc][cc]);
	}
        printf("\n");
    }
   printf("\n");				     
   return 0;
}

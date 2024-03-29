#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>

// Goes through an integer array of memory keys and gets rid of the shared memory segments
void releaseSharedMemory(int *keys, int numKeys) {
	int i; 
	for (i = 0; i < numKeys; i++) {
		shmctl(keys[i], IPC_RMID, NULL);
	}
	return;
}

// Deallocates memory
void freeMem(char **stringArray, int numRows) {
	int i = 0;
	for (i = 0; i < numRows; i++) {
		free(stringArray[i]);
	}
	free(stringArray);
	return;
}

// Returns 0 on success, -1 on failure, or 1 if it only read in ONE matrix (e.g. only one aws input), leaving the fields
// null. 
int readTwoMatrices(int *leftMatrix, int *rightMatrix, int *rows1, int *cols1, int *rows2, int *cols2) {
	//printf("here\n");
	char **temp = calloc(MAX_MATRIX_DIMENSION, sizeof(int));
 	char **temp2 = calloc(MAX_MATRIX_DIMENSION, sizeof(int));
 	int oneMatrix = 0;
 	// Read in the first line as a special case so I can know how many columns matrix 1 will be. Need to also count the max size of a number to know
 	// how big this will be. ALSO check to make sure something is input at all; if not it causes it to freeze up
 	

 	int colSize = (MAX_DIGITS_INT + 1)*MAX_MATRIX_DIMENSION;
 	int numRows1 = 0;
 	while (numRows1 < MAX_MATRIX_DIMENSION) {
 		temp[numRows1] = calloc(colSize, 1);
 		fgets(temp[numRows1], colSize, stdin);
		int tempVal = temp[numRows1][0] == '\n';
		int tempVal2 = temp[numRows1][0] == EOF || temp[numRows1][0] == '\0';
		if (tempVal2) {
			oneMatrix = 1;
			break;
		} if (tempVal) {
			break;
		}
		numRows1++;

 	}

 	int numRows2 = 0;
 	while (numRows2 < MAX_MATRIX_DIMENSION && !oneMatrix) {

 		temp2[numRows2] = calloc(colSize, 1);
 		fgets(temp2[numRows2], colSize, stdin);
		int tempVal = (temp2[numRows2][0] == '\n' || temp2[numRows2][0] == EOF || temp2[numRows2][0] == '\0');
		if (tempVal) {
			break;
		}
		numRows2++;

 	}


 	int itemsCol1 = 1;
 	int i;
 	// ItemsInCol1 starts at 1 b/c will run off edge of buffer and stop loop on last item
 	for (i = 0; temp[0][i] != '\0'; i++) {
 		if (temp[0][i] == ' ') {
 			itemsCol1++;
 		}
 	}

 	int itemsCol2 = 1;
 	for (i = 0; !oneMatrix && temp2[0][i] != '\0'; i++) {
 		if (temp2[0][i] == ' ') {
 			itemsCol2++;
 		}
 	}
 	
 	


 	if ((itemsCol1 != numRows2) && !oneMatrix) {
		printf("%d\t%d\t%d\t%d\n", numRows1, itemsCol1, numRows2, itemsCol2);
 		printf("Invalid input matrices. Matrixes must be of A x B and B x C size.\n");
		freeMem(temp, numRows1);
		freeMem(temp2, numRows2);
 		return -1;
 	}

 	int leftArrayKey = shmget(0, itemsCol1*numRows1*sizeof(int), IPC_CREAT | S_IRUSR | S_IWUSR);
 	if (leftArrayKey < 0) {
 		printf("%d\t%d\n", itemsCol1, numRows1);

 		perror("shmget failed: ");
 		printf("Error allocating shared memory. Aborting\n");
 		return -1;
 	}
 	void *leftArrayVoid = shmat(leftArrayKey, NULL, 0);

 	if (leftArrayVoid < 0) {
 		printf("Error allocating shared memory. Aborting\n");
		shmctl(leftArrayKey, IPC_RMID, NULL);
 		return -1;
 	}
 	
 	
 	int *leftArray = (int *)leftArrayVoid;
  	for (i = 0; i < numRows1; i++) {
 		int j = 0;
 		int k = 0;
 		char *num = calloc(MAX_DIGITS_INT, 1);
 		
 		for (j = 0; temp[i][j] != '\n' && temp[i][j] != EOF && temp[i][j] != '\0'; j++) {
 			if (temp[i][j] == ' ') {
 				leftArray[i*itemsCol1 + k] = atoi(num);
				//printf("%d\t", leftArray[i*itemsCol1 + k]);
 				k++;
 				free(num);
 				num = calloc(MAX_DIGITS_INT, 1);
 			}
 			char *tempStore = calloc(2, 1);
 			tempStore[0] = temp[i][j];		
 			strncat(num, tempStore, 2);
 			free(tempStore);
 		}

 		leftArray[i*itemsCol1 + k] = atoi(num);			
		//printf("%d\n", leftArray[i*itemsCol1 + k]);
 		free(num);
 	}


 	if (oneMatrix) {
 		freeMem(temp, numRows1);
 		freeMem(temp2, numRows2);
 		*leftMatrix = leftArrayKey;
		*rows1 = numRows1;
		*cols1 = itemsCol1;
 		return 1;
 	}

 	// Sets up the "right" (secodn array)s
 	int rightArrayKey = shmget(IPC_PRIVATE, itemsCol2*numRows2*sizeof(int), IPC_CREAT | S_IRUSR | S_IWUSR);
	if (rightArrayKey < 0) {
 		printf("Error allocating shared memory. Aborting\n");
		shmctl(leftArrayKey, IPC_RMID, NULL);
 		return -1;
 	}

	void *rightArrayVoid = shmat(rightArrayKey, NULL, 0);
 	if (rightArrayVoid < 0) {
 		printf("Error allocating shared memory. Aborting\n");
		shmctl(leftArrayKey, IPC_RMID, NULL);
		shmctl(rightArrayKey, IPC_RMID, NULL);
 		return -1;
 	}
 	int *rightArray = (int *)rightArrayVoid;

 	for (i = 0; i < numRows2; i++) {
 		int j = 0;
 		int k = 0;
 		char *num = calloc(MAX_DIGITS_INT, 1);
 		for (j = 0; temp2[i][j] != '\n' || temp2[i][j] == EOF; j++) {
 			if (temp2[i][j] == ' ') {
 				rightArray[i*itemsCol2 + k] = atoi(num);
				//printf("%d\t", rightArray[i*itemsCol2 + k]);
 				k++;
 				free(num);
 				num = calloc(MAX_DIGITS_INT, 1);
 			}
 			char *tempStore = calloc(2, 1);
 			tempStore[0] = temp2[i][j];		
 			strncat(num, tempStore, 2);
 			free(tempStore);
 		}
 		rightArray[i*itemsCol2 + k] = atoi(num);
		//printf("%d\n", rightArray[i*itemsCol2 + k]);
 		free(num);
 	}
	//printf("last element first row: %d\n", rightArray[24]);

 	freeMem(temp, numRows1);
 	freeMem(temp2, numRows2);
	// Storing everything away. 
	*leftMatrix = leftArrayKey;
	*rightMatrix = rightArrayKey;
	*rows1 = numRows1;
	*cols1 = itemsCol1;
	*rows2 = numRows2;
	*cols2 = itemsCol2;
	return 0;
}
 int main(int argc, char **argv) {
 	int i;
	int leftArrayKey;
	int rightArrayKey;
	int numRows1;
	int itemsCol1;
	int numRows2 = 8675309;
	int itemsCol2;
	int val = readTwoMatrices(&leftArrayKey, &rightArrayKey, &numRows1, &itemsCol1, &numRows2, &itemsCol2);
	if (val != 1) {
		return -1;
	}

	int *array = (int *)shmat(leftArrayKey, NULL, 0);
	if (array == (void *) -1) {
 		printf("error accessing shared memory\n");
 		return -1;
	}

	int transpose[itemsCol1][numRows1];
	for (i = 0; i < itemsCol1; i++) {
		int j;
		for (j = 0; j < numRows1; j++) {
			transpose[i][j] = array[j*itemsCol1 + i];
			printf("%d", transpose[i][j]);
			if (j != (numRows1 - 1)) {
				printf(" ");
			}
		}
		/*
		if (i != (itemsCol1 - 1)) {
			printf("\n");
		}
		*/
		printf("\n");
	}

	int keys[1];
	keys[0] = leftArrayKey;
	releaseSharedMemory(keys,1);

	return 0;
 }

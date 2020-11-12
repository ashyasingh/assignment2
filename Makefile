all : shell matmult_p matmult_t

shell: myshell.c
	gcc -o myshell myshell.c

matmult_p : matmult_p.c
	gcc -o matmult_p matmult_p.c
	
matmult_t: matmult_t.c
	gcc -o matmult_t matmult_t.c

clean:
	rm -rf myshell matmult_p matmult_t

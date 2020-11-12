#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

// store jump mark
sigjmp_buf mark;

// command struct to store
// individual cmds and their
// arguments
struct command
{
    char **cmd;  // list of arguments
    int numArgs; // number of total arguments in line
};

// on sigint kill stdin buffer and show prompt
void sigintHandler(int sig_num)
{
    // Reset handler to catch SIGINT next time.
    signal(SIGINT, sigintHandler);
    // jump back to main to avoid fgets
    siglongjmp(mark, 1);
}

// wait for and read from stdin
char *get_args()
{
    char *args = calloc(sizeof(char), 2048);
    char *result = fgets(args, 2047, stdin);
    // terminate on end of file (ctrl-d)
    if (!result)
    {
        printf("exit\n");
        exit(EXIT_FAILURE);
    }
    char *pter = strrchr(args, '\n');
    if (pter)
        *pter = 0;
    return args;
}

// parse whiteSpace in command
char **oldParseW(char *cmd)
{
    // duplicate input command to keep it unchanged
    char *tempCmd = strndup(cmd, strlen(cmd));
    // savePter for strtok_r
    char *savePter;
    int i = 0, size = 0;

    // get the first whitespace token from cmd
    char *token = strtok_r(tempCmd, " \t", &savePter);
    // loop through and count number of actual args
    while (token)
    {
        size += strlen(token);
        token = strtok_r(NULL, " \t", &savePter);
    }

    // allocate return array of arguments
    char **ret = calloc(size + 1, sizeof(char *));
    ret[size] = (char)0;

    // reset the input cmd and savepter
    strcpy(tempCmd, cmd);
    savePter = NULL;

    // get token for whitespace from cmd
    token = strtok_r(tempCmd, " \t", &savePter);
    // loop through and put args in ret
    while (token)
    {
        // check for invalid pipes
        if (!strcmp(token, "|") && (i == 0 || i == size - 1))
        {
            perror("parseW: invalid pipe input");
            return NULL;
        }
        ret[i++] = token;
        token = strtok_r(NULL, " \t", &savePter);
    }

    return ret;
}

// parse whiteSpace in command, return array of command structs
struct command *parseW(char *cmd)
{
    char *tempCmd = strndup(cmd, strlen(cmd)); // duplicate input command to keep it unchanged
    char *savePter;                            // savePter for strtok_r
    int i = 0, size = 0, numCmds = 0;          // i fo

    char *token = strtok_r(tempCmd, " \t", &savePter); // get the first whitespace token from cmd
    while (token)                                      // loop through and count number of actual args
    {
        // if not operator, add strlen
        if (strcmp(token, "|") && strcmp(token, ">") && strcmp(token, "<") &&
            strcmp(token, "1>") && strcmp(token, "2>") && strcmp(token, "&>"))
        {
        }
        else // is special operator
        {
            numCmds += 2;
        }
        i++;
        token = strtok_r(NULL, " \t", &savePter);
    }
    numCmds++; // increment bc of initial cmd

    // initializing commands list
    struct command *commands = calloc(numCmds + 1, sizeof(struct command));
    strcpy(tempCmd, cmd);
    savePter = NULL;
    i = 0;

    token = strtok_r(tempCmd, " \t", &savePter);
    // loop through and allocate space for each cmd and its arguments
    while (token)
    {
        // error check invalid pipes
        if (!strcmp(token, "|") && i == 0 && size == 0)
        {
            printf("parseW: invalid pipe parsed\n");
            exit(EXIT_FAILURE);
        }

        // if not operator, add strlen
        if (strcmp(token, "|") && strcmp(token, ">") && strcmp(token, "<") &&
            strcmp(token, "1>") && strcmp(token, "2>") && strcmp(token, "&>"))
        {
            size += strlen(token);
        }
        else // is special operator
        {
            if (size == 0)
            {
                printf("parseW: invalid syntax, unexpected operator or token '%s'\n", token);
                exit(EXIT_FAILURE);
            }
            else
            {

                // add all prev commands size
                commands[i].numArgs = numCmds;
                commands[i++].cmd = calloc(size + 1, sizeof(char *));

                // add special operator
                commands[i].numArgs = numCmds;
                commands[i++].cmd = calloc(strlen(token) + 1, sizeof(char *));
                size = 0;
            }
        }
        token = strtok_r(NULL, " \t", &savePter);
    }
    if (!size) // if last token parsed was a pipe, it's invalid
    {
        printf("parseW: invalid pipe parsed\n");
        exit(EXIT_FAILURE);
    }
    commands[i].numArgs = numCmds;
    commands[i].cmd = calloc(size + 1, sizeof(char *)); // add final command allocation

    strcpy(tempCmd, cmd);
    savePter = NULL;
    i = 0;     // cmd index
    int z = 0; // cmd argument index
    token = strtok_r(tempCmd, " \t", &savePter);

    // loop through and set each cmd and their args
    while (token)
    {
        // if not operator, add token
        if (strcmp(token, "|") && strcmp(token, ">") && strcmp(token, "<") &&
            strcmp(token, "1>") && strcmp(token, "2>") && strcmp(token, "&>"))
        {
            commands[i].cmd[z++] = token;
        }
        else // is special operator, reset arg index, inc cmd index
        {
            z = 0;
            commands[++i].cmd[0] = token;
            i++;
        }
        token = strtok_r(NULL, " \t", &savePter);
    }

    return commands;
}

// runs non pipe/redirection commands
void runSimple(char **args)
{
    pid_t pid;
    int i = 0;
    int fd; // store file descriptors

    pid = fork(); // store child pid
    if (pid < 0)  // error
    {
        printf("An error occured while forking child for runSimple.\n");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0) // child
    {
        i = 0;
        // loop and deal with special operators
        while (args[i])
        {
            // printf("cur: %s, next: %s, afteR: %s\n", args[i], args[i + 1], args[i + 2]);
            // deal with |
            if (args[i + 1] && !strcmp(args[i + 1], "|"))
            {
                int fd1[2]; // Used to store two ends of first pipe
                pid_t p;
                if (pipe(fd1) == -1)
                {
                    fprintf(stderr, "Pipe Failed");
                    exit(EXIT_FAILURE);
                }
                p = fork();
                if (p < 0)
                {
                    fprintf(stderr, "fork Failed");
                    exit(EXIT_FAILURE);
                }
                // Parent process
                else if (p > 0)
                {
                    close(fd1[0]); // Close reading end of first pipe

                    args[i + 1] = NULL;
                    // Write input string and close writing end of first
                    // pipe.
                    dup2(fd1[1], STDOUT_FILENO);
                    execvp(args[0], args);
                    close(fd1[1]);

                    // Wait for child to send a string
                    wait(NULL);
                } // child process
                else
                {
                    close(fd1[1]); // Close writing end of first pipe
                    dup2(fd1[0], STDIN_FILENO);
                    printf("child cmd: %s\n", args[i + 2]);
                    char *temp[] = {"wc", "-l", NULL};
                    execvp(args[i + 2], temp);
                    // Close both reading ends
                    close(fd1[0]);

                    exit(0);
                }
            }

            // deal with &>
            if (args[i] && !strcmp(args[i], "&>"))
            {
                fd = creat(args[i + 1], 0644);
                if (fd < 0)
                {
                    perror("open");
                    exit(EXIT_FAILURE);
                }

                if (dup2(fd, STDOUT_FILENO) < 0)
                {
                    perror("runSimple: failed dup2 for stdout on &>");
                    exit(EXIT_FAILURE);
                }
                if (dup2(STDOUT_FILENO, STDERR_FILENO) < 0)
                {
                    perror("runSimple: failed dup2 for stderr on &>");
                    exit(EXIT_FAILURE);
                }
                close(fd);
                args[i] = NULL;
            }

            // deal with 2>
            if (args[i] && !strcmp(args[i], "2>"))
            {

                fd = creat(args[i + 1], 0644);
                if (fd < 0)
                {
                    perror("runSimple: error opening file for 2>");
                    exit(EXIT_FAILURE);
                }
                if (dup2(fd, STDERR_FILENO) < 0)
                {
                    perror("runSimple: failed dup2 on stderr redirecting on 2>");
                    exit(EXIT_FAILURE);
                }
                close(fd);
                args[i] = NULL;
            }

            // deal with > and 1>
            if (args[i] && (!strcmp(args[i], ">") || !strcmp(args[i], "1>")))
            {
                fd = creat(args[i + 1], 0644);
                if (fd < 0)
                {
                    perror("runSimple: error opening argument for >");
                    exit(EXIT_FAILURE);
                }

                // make child's stdout into file
                if (dup2(fd, STDOUT_FILENO) < 0)
                {
                    perror("runSimple: failure dup2 on redirecting STDOUT to fd on > or 1>");
                    exit(EXIT_FAILURE);
                }
                close(fd);
                args[i] = NULL;
            }

            // deal with <, making sure to check if args[i] exists first
            if (args[i] && !strcmp(args[i], "<"))
            {
                fd = open(args[i + 1], O_RDONLY);
                if (fd < 0)
                {
                    perror("runSimple: error opening argument for <");
                    exit(EXIT_FAILURE);
                }
                if (dup2(fd, STDIN_FILENO) < 0)
                {
                    perror("runSimple: error dup2 on redirecting stdin to fd on <");
                    exit(EXIT_FAILURE);
                }
                close(fd);
                args[i] = NULL;
            }

            i++;
        }

        // exec input with appropriate fd
        execvp(args[0], args);
        perror("execvp");
        exit(EXIT_FAILURE);
    }
    int status;

    // parent should wait for child to finish if & not specified
    waitpid(pid, &status, 0);
}

/*
Main method has an infinite while loop that does:
    1) check if input is from terminal, if so go to 2
    2) print myshell prompt
    3) wait for user stdin by using get_args() function
    4) while loop to loop through individual semicolon separated commands
    5) for each individual command, get each argument token and parse
       whitespace by calling parseW, which returns array of strings
       of every single command and their arguments
    6) hands control off to runSimple(), which runs the list of commands 
       appropriately, and handles command operators (>, <, 1>, etc)
Main method also has a signal handler for SIGINT (ctrl+c), in which event
another signal handler for SIGINT is setup, and a jump is made to the before
the while loop so that the shell can print prompt and listen for commands again
*/
int main(int argc, char *argv[])
{
    int i = 0, size = 0;
    char *semi;
    char *semiSavePter;
    signal(SIGINT, sigintHandler);
    siginterrupt(SIGINT, 1);
    sigsetjmp(mark, 1);

    while (1)
    {
        if (isatty(STDIN_FILENO))
        {
            // print prompt
            printf("\n\033[1;32mmyshell\033[0m>");
            char *line = get_args();

            // parse semis
            semi = strtok_r(line, ";", &semiSavePter);
            // for every semicolon, parse each cmd
            while (semi)
            {
                // get array of args
                // struct command *args = parseW(semi);
                char **args = oldParseW(semi);
                // if error in parsing, break and reprint prompt
                if (!args)
                    break;

                runSimple(args);
                semi = strtok_r(NULL, ";", &semiSavePter);
            }
        }
    }
    return 0;
};

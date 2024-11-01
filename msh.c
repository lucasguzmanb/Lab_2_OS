// P2-SSOO-23/24

//  MSH main file
// Write your msh source code here

// #include "parser.h"
#include <fcntl.h>
#include <signal.h>
#include <stddef.h> /* NULL */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wait.h>

#define MAX_COMMANDS 8
#define READ_END 0  // index of reading pipe end
#define WRITE_END 1 // index of writing pipe end

// files in case of redirection
char filev[3][64];

// to store the execvp second parameter
char *argv_execvp[8];

void siginthandler(int param) {
    printf("****  Exiting MSH **** \n");
    // signal(SIGINT, siginthandler);
    exit(0);
}

void sigchldhandler(int signal) {
    // When SIGCHLD is recieved, clean the status of the child that finished
    int status;
    waitpid(-1, &status, WNOHANG);
}
/* myhistory */

/* mycalc */

struct command {
    // Store the number of commands in argvv
    int num_commands;
    // Store the number of arguments of each command
    int *args;
    // Store the commands
    char ***argvv;
    // Store the I/O redirection
    char filev[3][64];
    // Store if the command is executed in background or foreground
    int in_background;
};

int history_size = 20;
struct command *history;
int head = -1;
int tail = -1;
int n_elem = 0;

void free_command(struct command *cmd) {
    if ((*cmd).argvv != NULL) {
        char **argv;
        for (; (*cmd).argvv && *(*cmd).argvv; (*cmd).argvv++) {
            for (argv = *(*cmd).argvv; argv && *argv; argv++) {
                if (*argv) {
                    free(*argv);
                    *argv = NULL;
                }
            }
        }
    }
    free((*cmd).args);
}

void store_command(char ***argvv, char filev[3][64], int in_background, struct command *cmd) {
    int num_commands = 0;
    while (argvv[num_commands] != NULL) {
        num_commands++;
    }

    for (int f = 0; f < 3; f++) {
        if (strcmp(filev[f], "0") != 0) {
            strcpy((*cmd).filev[f], filev[f]);
        } else {
            strcpy((*cmd).filev[f], "0");
        }
    }

    (*cmd).in_background = in_background;
    (*cmd).num_commands = num_commands - 1;
    (*cmd).argvv = (char ***)calloc((num_commands), sizeof(char **));
    (*cmd).args = (int *)calloc(num_commands, sizeof(int));

    for (int i = 0; i < num_commands; i++) {
        int args = 0;
        while (argvv[i][args] != NULL) {
            args++;
        }
        (*cmd).args[i] = args;
        (*cmd).argvv[i] = (char **)calloc((args + 1), sizeof(char *));
        int j;
        for (j = 0; j < args; j++) {
            (*cmd).argvv[i][j] = (char *)calloc(strlen(argvv[i][j]), sizeof(char));
            strcpy((*cmd).argvv[i][j], argvv[i][j]);
        }
    }
}

/**
 * Get the command with its parameters for execvp
 * Execute this instruction before run an execvp to obtain the complete command
 * @param argvv
 * @param num_command
 * @return
 */
void getCompleteCommand(char ***argvv, int num_command) {
    // reset first
    for (int j = 0; j < 8; j++)
        argv_execvp[j] = NULL;

    int i = 0;
    for (i = 0; argvv[num_command][i] != NULL; i++)
        argv_execvp[i] = argvv[num_command][i];
}

char *commandToString(char ***argvv, int num_commands, int in_background, char filev[3][64]) {
    char *result = (char *)malloc(100 * sizeof(char));
    if (result == NULL) {
        perror("Error in memory assignment");
        exit(EXIT_FAILURE);
    }
    result[0] = '\0'; // Initialise result string as empty

    for (int i = 0; i < num_commands; i++) {
        for (int j = 0; argvv[i][j] != NULL; j++) {
            // Concat each argument to result
            sprintf(result + strlen(result), "%s ", argvv[i][j]);
        }
        if (i != num_commands - 1) { // if not the last command, put a pipe operator between commands
            sprintf(result + strlen(result), "| ");
        }
    }
    // remove last empty sapce
    if (strlen(result) > 0)
        result[strlen(result) - 1] = '\0';

    if (in_background)
        sprintf(result + strlen(result), " &");

    if (strcmp(filev[0], "0") != 0) // redirection of input
        sprintf(result + strlen(result), " < %s", filev[0]);

    if (strcmp(filev[1], "0") != 0) // redirection of output
        sprintf(result + strlen(result), " > %s", filev[1]);

    if (strcmp(filev[2], "0") != 0) // redirection of error
        sprintf(result + strlen(result), " !> %s", filev[2]);

    return result;
}

/**
 * Main sheell  Loop
 */
int main(int argc, char *argv[]) {
    /**** Do not delete this code.****/
    int end = 0;
    int executed_cmd_lines = -1;
    char *cmd_line = NULL;
    char *cmd_lines[10];

    if (!isatty(STDIN_FILENO)) {
        cmd_line = (char *)malloc(100);
        while (scanf(" %[^\n]", cmd_line) != EOF) {
            if (strlen(cmd_line) <= 0)
                return 0;
            cmd_lines[end] = (char *)malloc(strlen(cmd_line) + 1);
            strcpy(cmd_lines[end], cmd_line);
            end++;
            fflush(stdin);
            fflush(stdout);
        }
    }

    /*********************************/

    char ***argvv = NULL;
    int num_commands;

    history = (struct command *)malloc(history_size * sizeof(struct command));
    int run_history = 0;

    // set environment variable to 0
    if (setenv("Acc", "0", 1) != 0) {
        perror("setenv");
        return 1;
    }

    while (1) {
        int status = 0;
        int command_counter = 0;
        int in_background = 0;
        signal(SIGINT, siginthandler);

        if (run_history) {
            run_history = 0;
        } else {
            // Prompt
            write(STDERR_FILENO, "MSH>>", strlen("MSH>>"));

            // Get command
            //********** DO NOT MODIFY THIS PART. IT DISTINGUISH BETWEEN NORMAL/CORRECTION MODE***************
            executed_cmd_lines++;
            if (end != 0 && executed_cmd_lines < end) {
                command_counter = read_command_correction(&argvv, filev, &in_background, cmd_lines[executed_cmd_lines]);
            } else if (end != 0 && executed_cmd_lines == end)
                return 0;
            else
                command_counter = read_command(&argvv, filev, &in_background); // NORMAL MODE
        }
        //************************************************************************************************

        /************************ STUDENTS CODE ********************************/
        if (command_counter > 0) {
            if (command_counter > MAX_COMMANDS) {
                printf("Error: Maximum number of commands is %d \n", MAX_COMMANDS);
            } else {
                // store command in history
                if (strcmp(argvv[0][0], "myhistory") != 0) { // the command myhistory is not stored
                    if (head == tail && head == -1) {        // if history is empty
                        head = 0;
                        tail = 0;
                        store_command(argvv, filev, in_background, &history[tail]);
                    } else { // if not empty
                        tail = (tail + 1) % history_size;
                        if (head == tail) { // if we reach the "end" of the queue
                            head = (head + 1) % history_size;
                        }
                        store_command(argvv, filev, in_background, &history[tail]); // store in the next spot or overwrite the oldest one
                    }
                }

                signal(SIGCHLD, sigchldhandler); // signal handler when recieving SIGCHLD
                pid_t pid;

                if (command_counter == 1) { // simple command

                    /* mycalc */

                    if (strcmp(argvv[0][0], "mycalc") == 0) {

                        int num_args = 0;
                        while (argvv[0][num_args] != NULL) { // count the number of arguments passed to mycalc
                            num_args++;
                        }

                        if (num_args != 4 || in_background || strcmp(filev[0], "0") != 0 || strcmp(filev[1], "0") != 0 || strcmp(filev[2], "0") != 0) { // not correct format of the command

                            fprintf(stdout, "[ERROR] The structure of the command is mycalc <operand_1> <add/mul/div> <operand_2>\n");

                        } else {

                            int operand_1 = atoi(argvv[0][1]); // we get the values of the operands that are stored in argvv
                            char *operator= argvv[0][2];
                            int operand_2 = atoi(argvv[0][3]);
                            int result;
                            if (strcmp(operator, "add") == 0) { // if the operator is add we add the operators and update Acc

                                result = operand_1 + operand_2;

                                char *acc_value_str = getenv("Acc");                                        // obtain value of environment Acc
                                int acc_value = (acc_value_str != NULL) ? atoi(acc_value_str) + result : 0; // if not NULL (correct get of env value), add result to its value, else assign 0

                                char acc_value_buf[20];
                                snprintf(acc_value_buf, sizeof(acc_value_buf), "%d", acc_value); // transform acc_value to str

                                if (setenv("Acc", acc_value_buf, 1) != 0) { // update value of Acc variable to the environment Acc variable
                                    perror("setenv");
                                    return 1;
                                }

                                fprintf(stderr, "[OK] %d + %d = %d; Acc %d\n", operand_1, operand_2, result, acc_value);

                            } else if (strcmp(operator, "mul") == 0) { // if the operator is mul we multiply the operators

                                result = operand_1 * operand_2;
                                fprintf(stderr, "[OK] %d * %d = %d\n", operand_1, operand_2, result);

                            } else if (strcmp(operator, "div") == 0) { // if the operator is div we get the integer quotient between the operators and the remainder

                                if (operand_2 != 0) {

                                    int quotient = operand_1 / operand_2;
                                    int remainder = operand_1 % operand_2;
                                    fprintf(stderr, "[OK] %d / %d = %d; Remainder %d\n", operand_1, operand_2, quotient, remainder);

                                } else {
                                    fprintf(stdout, "[ERROR] Division by zero is not allowed\n"); // we cannot divide by 0
                                }
                            } else { // if it did not enter in any of these cases, the structure is wrong
                                fprintf(stdout, "[ERROR] The structure of the command is mycalc <operand_1> <add/mul/div> <operand_2>\n");
                            }
                        }
                    }

                    /* myhistory */

                    else if (strcmp(argvv[0][0], "myhistory") == 0) {
                        int num_args = 0;
                        while (argvv[0][num_args] != NULL) { // count the number of arguments passed to myhistory
                            num_args++;
                        }
                        if (num_args == 1) { // only myhistory, print all saved history
                            int counter = 0;
                            int index = head;
                            do {
                                char *command_str = commandToString(history[index].argvv, history[index].num_commands, history[index].in_background, history[index].filev);
                                fprintf(stderr, "%d %s\n", counter, command_str);
                                index = (index + 1) % history_size;
                                counter++;
                            } while (index != (tail + 1) % history_size);

                        } else if (in_background || strcmp(filev[0], "0") != 0 || strcmp(filev[1], "0") != 0 || strcmp(filev[2], "0") != 0) { // file redirection or background execution
                            fprintf(stdout, "ERROR: Command not found \n");
                        } else if (num_args == 2 && atoi(argvv[0][1]) < history_size && atoi(argvv[0][1]) >= 0) { // correct format of the command
                            // execute the command
                            if (tail >= head && atoi(argvv[0][1]) > tail) { // if we try to run a command in an empty position
                                fprintf(stdout, "ERROR: Command not found \n");
                            } else {
                                fprintf(stderr, "Running command %s\n", argvv[0][1]);
                                pid = fork();
                                int index_to_exec = (head + atoi(argvv[0][1])) % history_size;

                                if (pid == -1) {
                                    perror("Error in fork");
                                    exit(EXIT_FAILURE);
                                } else if (pid == 0) {
                                    getCompleteCommand(history[index_to_exec].argvv, 0);
                                    execvp(argv_execvp[0], argv_execvp);
                                    perror("Error in execvp");
                                    exit(EXIT_FAILURE);
                                }
                            }
                        } else { // invalid arguments or incorrect number of arguments
                            fprintf(stdout, "ERROR: Command not found \n");
                        }
                    }

                    /* any other command */

                    else {

                        pid = fork();

                        if (pid == -1) {
                            perror("Error in fork"); // if fork doesn't work correctly
                            exit(EXIT_FAILURE);
                        }

                        else if (pid == 0) { // child process

                            int fid;
                            if (strcmp(filev[0], "0") != 0) { // redirection of input
                                fid = open(filev[0], O_RDONLY);
                                if (fid < 0) {
                                    perror("Error in open");
                                    exit(-1);
                                }
                                dup2(fid, STDIN_FILENO);
                                close(fid);
                            }
                            if (strcmp(filev[1], "0") != 0) { // redirection of output
                                fid = open(filev[1], O_CREAT | O_WRONLY | O_TRUNC, 0666);
                                if (fid < 0) {
                                    perror("Error in open");
                                    exit(-1);
                                }
                                dup2(fid, STDOUT_FILENO);
                                close(fid);
                            }
                            if (strcmp(filev[2], "0") != 0) { // redirection of error
                                fid = open(filev[2], O_CREAT | O_WRONLY | O_TRUNC, 0666);
                                if (fid < 0) {
                                    perror("Error in open");
                                    exit(-1);
                                }
                                dup2(fid, STDERR_FILENO);
                                close(fid);
                            }
                            getCompleteCommand(argvv, 0);        // get command
                            execvp(argv_execvp[0], argv_execvp); // execute the command
                            perror("Error in execvp");           // if execvp doesnt work correctly
                            exit(EXIT_FAILURE);
                        }

                        else { // Parent process
                            if (!in_background) {
                                waitpid(pid, &status, 0); // if command is in foreground, wait for the child to finish, blocking shell
                            } else {
                                printf("[%d]\n", pid); // print bg process id, and not wait for child to finish (will "wait" for SIGCHLD, but not blocking the shell)
                            }
                        }
                    }

                } else if (command_counter > 1) { // more than 1 command, we need pipes

                    signal(SIGCHLD, sigchldhandler); // signal handler when recieving SIGCHLD
                    int fd[command_counter - 1][2];  // to store each pipe's fd

                    for (int i = 0; i < command_counter; i++) {

                        if (i != command_counter - 1) { // if not the last command
                            if (pipe(fd[i]) == -1) {
                                perror("Error in pipe");
                                exit(EXIT_FAILURE);
                            }
                        }

                        pid = fork(); // create a child for each process

                        if (pid == -1) {
                            perror("Error in fork"); // if fork doesnt work correctly
                            exit(EXIT_FAILURE);
                        }

                        else if (pid == 0) { // child process (has inherited both fds of the pipe)

                            int fid;
                            if (strcmp(filev[2], "0") != 0) { // redirection of error
                                fid = open(filev[2], O_CREAT | O_WRONLY | O_TRUNC, 0666);
                                if (fid < 0) {
                                    perror("Error in open");
                                    exit(-1);
                                }
                                dup2(fid, STDERR_FILENO);
                                close(fid);
                            }

                            if (i == 0) { // first command of sequence

                                if (strcmp(filev[0], "0") != 0) { // redirection of input
                                    fid = open(filev[0], O_RDONLY);
                                    if (fid < 0) {
                                        perror("Error in open");
                                        exit(-1);
                                    }
                                    dup2(fid, STDIN_FILENO);
                                    close(fid);
                                }
                                close(fd[i][READ_END]);                // non needed end of pipe
                                dup2(fd[i][WRITE_END], STDOUT_FILENO); // to write into the pipe
                                close(fd[i][WRITE_END]);

                            } else if (i == command_counter - 1) { // last command of sequence

                                close(fd[i - 1][WRITE_END]);
                                dup2(fd[i - 1][READ_END], STDIN_FILENO); // to read from the pipe
                                close(fd[i - 1][READ_END]);

                                if (strcmp(filev[1], "0") != 0) { // redirection of output
                                    fid = open(filev[1], O_CREAT | O_WRONLY | O_TRUNC, 0666);
                                    if (fid < 0) {
                                        perror("Error in open");
                                        exit(-1);
                                    }
                                    dup2(fid, STDOUT_FILENO);
                                    close(fid);
                                }

                            } else {                                     // "middle" command
                                dup2(fd[i - 1][READ_END], STDIN_FILENO); // read from previous pipe
                                close(fd[i - 1][READ_END]);
                                dup2(fd[i][WRITE_END], STDOUT_FILENO); // write on next pipe
                                close(fd[i][WRITE_END]);
                            }

                            getCompleteCommand(argvv, i);        // get 1st command
                            execvp(argv_execvp[0], argv_execvp); // execute command
                            perror("Error in execvp");           // if execvp doesnt work correctly
                            exit(EXIT_FAILURE);

                        }

                        else { // parent
                            // close pipes from the parent
                            if (i != 0) {
                                close(fd[i - 1][READ_END]);
                            }

                            if (i != command_counter - 1) {
                                close(fd[i][WRITE_END]);
                            }

                            if (!in_background) {
                                wait(&status); // if command is in foreground, wait for the child to finish, blocking shell
                            } else if (i == command_counter - 1) {
                                printf("[%d]\n", pid); // print bg process id only of the last command, and not wait for child to finish (will "wait" for SIGCHLD, but not blocking the shell)
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}

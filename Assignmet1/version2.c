#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 100

void parseInput(char* input, char** commands);
void parseCommand(char* command, char** args, char** inputFile, char** outputFile, int* append, int* fd_out);
void executePipedCommands(char** commands);
void executeCommand(char** args, char* inputFile, char* outputFile, int append, int fd_out);

int main() {
    char input[MAX_INPUT_SIZE];
    char *commands[MAX_ARGS];
    char cwd[1024];

    while (1) {
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("PUCITshell:- %s$ ", cwd);  // Display prompt with current working directory
        } else {
            perror("getcwd() error");
            return 1;
        }

        if (fgets(input, MAX_INPUT_SIZE, stdin) == NULL) {
            printf("\nExit shell\n");
            break;  // Exit if CTRL+D is pressed
        }

        if (input[strlen(input) - 1] == '\n') {
            input[strlen(input) - 1] = '\0';  // Remove newline character
        }

        // Parse the input line into separate commands separated by '|'
        parseInput(input, commands);

        // Execute the piped commands
        executePipedCommands(commands);
    }
    return 0;
}

// Function to parse the input line into commands separated by '|'
void parseInput(char* input, char** commands) {
    char* command;
    int i = 0;
    command = strtok(input, "|");
    while (command != NULL && i < MAX_ARGS - 1) {
        commands[i++] = command;
        command = strtok(NULL, "|");
    }
    commands[i] = NULL;
}

// Function to parse a single command into arguments and detect I/O redirection
void parseCommand(char* command, char** args, char** inputFile, char** outputFile, int* append, int* fd_out) {
    char* token;
    int i = 0;
    *inputFile = NULL;
    *outputFile = NULL;
    *append = 0;
    *fd_out = STDOUT_FILENO; // Default output is stdout

    token = strtok(command, " ");
    while (token != NULL && i < MAX_ARGS - 1) {
        if (strchr(token, '>') != NULL || strchr(token, '<') != NULL) {
            char *operator_pos = NULL;
            int is_append = 0;
            int fd = STDOUT_FILENO; // Default output fd

            // Check for output append redirection (>>)
            if ((operator_pos = strstr(token, ">>")) != NULL) {
                is_append = 1;
            } else if ((operator_pos = strchr(token, '>')) != NULL) {
                is_append = 0;
            } else if ((operator_pos = strchr(token, '<')) != NULL) {
                fd = STDIN_FILENO;
            }

            if (operator_pos != NULL) {
                // Determine file descriptor if specified
                if (operator_pos != token) {
                    if (isdigit(token[0])) {
                        fd = token[0] - '0';
                        if (fd == 0) {
                            fd = STDIN_FILENO;
                        } else if (fd == 1) {
                            fd = STDOUT_FILENO;
                        } else if (fd == 2) {
                            fd = STDERR_FILENO;
                        } else {
                            fprintf(stderr, "Unsupported file descriptor: %d\n", fd);
                        }
                    }
                }

                // Get the filename
                char *filename = operator_pos + (is_append ? 2 : 1);
                if (strlen(filename) == 0) {
                    // Filename is in the next token
                    token = strtok(NULL, " ");
                    if (token != NULL) {
                        filename = token;
                    } else {
                        fprintf(stderr, "Expected filename after redirection operator\n");
                        break;
                    }
                }

                if (fd == STDIN_FILENO) {
                    *inputFile = filename;
                } else {
                    *outputFile = filename;
                    *append = is_append;
                    *fd_out = fd;
                }
            }
        } else if (strcmp(token, "<") == 0 || strcmp(token, ">") == 0 || strcmp(token, ">>") == 0) {
            // Redirection operators with separate tokens
            int fd = (strcmp(token, "<") == 0) ? STDIN_FILENO : STDOUT_FILENO;
            int is_append = (strcmp(token, ">>") == 0);

            // Get the filename
            token = strtok(NULL, " ");
            if (token != NULL) {
                if (fd == STDIN_FILENO) {
                    *inputFile = token;
                } else {
                    *outputFile = token;
                    *append = is_append;
                    *fd_out = fd;
                }
            } else {
                fprintf(stderr, "Expected filename after redirection operator\n");
                break;
            }
        } else {
            // Regular argument
            args[i++] = token;
        }
        token = strtok(NULL, " ");
    }
    args[i] = NULL;
}

// Function to execute piped commands
void executePipedCommands(char** commands) {
    int i = 0;
    int fd[2];
    int input_fd = 0; // Initially, input is from STDIN
    pid_t pid;

    while (commands[i] != NULL) {
        char* args[MAX_ARGS];
        char* inputFile = NULL;
        char* outputFile = NULL;
        int append = 0;
        int fd_out = STDOUT_FILENO;

        // Parse individual command
        parseCommand(commands[i], args, &inputFile, &outputFile, &append, &fd_out);

        // Create pipe if not the last command
        if (commands[i + 1] != NULL) {
            if (pipe(fd) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }

        pid = fork();
        if (pid == 0) {
            // Child process

            // Input redirection
            if (inputFile != NULL) {
                int in_fd = open(inputFile, O_RDONLY);
                if (in_fd < 0) {
                    perror("open inputFile");
                    exit(EXIT_FAILURE);
                }
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            } else if (input_fd != 0) {
                dup2(input_fd, STDIN_FILENO);
                close(input_fd);
            }

            // Output redirection
            if (outputFile != NULL) {
                int out_fd;
                if (append) {
                    out_fd = open(outputFile, O_WRONLY | O_CREAT | O_APPEND, 0644);
                } else {
                    out_fd = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                }
                if (out_fd < 0) {
                    perror("open outputFile");
                    exit(EXIT_FAILURE);
                }
                dup2(out_fd, fd_out);
                close(out_fd);
            } else if (commands[i + 1] != NULL) {
                // If there's a next command, redirect output to pipe
                dup2(fd[1], STDOUT_FILENO);
                close(fd[1]);
                close(fd[0]);
            }

            // Close unused file descriptors
            if (commands[i + 1] != NULL) {
                close(fd[0]);
                close(fd[1]);
            }
            if (execvp(args[0], args) == -1) {
                perror("execvp");
            }
            exit(EXIT_FAILURE);
        } else if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else {
            // Parent process
            waitpid(pid, NULL, 0);
            if (input_fd != 0) {
                close(input_fd);
            }
            if (commands[i + 1] != NULL) {
                close(fd[1]);
                input_fd = fd[0]; // Next command's input
            }
        }
        i++;
    }
}




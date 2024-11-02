#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_LINE 1024
#define MAX_ARGS 100

void sigchld_handler(int signo) {
    // Reap child processes to prevent zombies
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main() {
    char input[MAX_LINE];
    char *args[MAX_ARGS];
    struct sigaction sa;

    // Set up signal handling for SIGCHLD
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    int job_number = 0; // To keep track of background jobs

    while (1) {
        // Prompt the user
        printf("PUCITshell:- ");

        // Read user input
        if (fgets(input, MAX_LINE, stdin) == NULL) {
            break; // Exit on Ctrl+D
        }

        // Remove trailing newline
        input[strcspn(input, "\n")] = 0;

        // Check for background execution
        int bg = 0;
        if (input[strlen(input) - 1] == '&') {
            bg = 1;
            input[strlen(input) - 1] = 0; // Remove '&'
        }

        // Tokenize input into arguments
        char *token = strtok(input, " ");
        int i = 0;
        while (token != NULL) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL; // Null-terminate the argument list

        if (i > 0) {
            pid_t pid = fork(); // Create a child process
            if (pid < 0) {
                perror("Fork failed");
                exit(1);
            }

            if (pid == 0) { // Child process
                if (execvp(args[0], args) < 0) {
                    perror("Execution failed");
                    exit(1);
                }
            } else { // Parent process
                if (bg) {
                    job_number++; // Increment job number for each background job
                    printf("[%d] %d\n", job_number, pid); // Print job number and PID
                } else {
                    waitpid(pid, NULL, 0); // Wait for foreground process
                }
            }
        }
    }
    return 0;
}

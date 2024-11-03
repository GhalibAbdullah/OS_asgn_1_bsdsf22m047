#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_LINE 1024     // Maximum length of a command line input
#define MAX_ARGS 100      // Maximum number of arguments in a command
#define HISTORY_SIZE 10   // Number of commands to keep in the history

// Array to store the last 10 commands
char history[HISTORY_SIZE][MAX_LINE];
int history_count = 0;  // Keeps track of the number of commands in history

// Adds a command to the history array
void add_to_history(const char *command) {
    if (history_count < HISTORY_SIZE) {
        // If history is not full, add the command to the end
        strcpy(history[history_count++], command);
    } else {
        // If history is full, move all commands up one position to make space
        for (int i = 1; i < HISTORY_SIZE; i++) {
            strcpy(history[i - 1], history[i]);
        }
        strcpy(history[HISTORY_SIZE - 1], command);  // Place new command at the end
    }
}

// Displays the list of commands in the history
void print_history() {
    int start = history_count < HISTORY_SIZE ? 0 : history_count - HISTORY_SIZE;
    for (int i = 0; i < history_count && i < HISTORY_SIZE; i++) {
        printf("%d %s\n", i + 1, history[i]);  // Print command number and text
    }
}

// Handles the SIGCHLD signal to clean up finished background processes
void sigchld_handler(int signo) {
    // Use a loop to "reap" zombie processes without waiting
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main() {
    char input[MAX_LINE];     // Store the user's command input
    char *args[MAX_ARGS];     // Array to store command arguments
    struct sigaction sa;      // Struct to manage signal handling

    // Set up the SIGCHLD signal handler to manage background processes
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    int job_number = 0;  // Tracks the background job number

    while (1) {
        // Display a prompt to the user
        printf("PUCITshell:- ");

        // Get the user's input
        if (fgets(input, MAX_LINE, stdin) == NULL) {
            break; // Exit the shell if the user presses Ctrl+D
        }

        // Remove the newline character from the end of the input
        input[strcspn(input, "\n")] = 0;

        // Add the command to history if it's not a repeat command
        if (input[0] != '!' && strlen(input) > 0) {
            add_to_history(input);
        }

        // Check if the command is a request to repeat a previous command
        if (input[0] == '!') {
            int index;
            if (input[1] == '-') {
                // If the input is "!-1", repeat the last command
                index = history_count - 1;
            } else {
                // If the input is "!number", get the command at that index
                index = atoi(&input[1]) - 1;
            }

            // Check if the index is valid, and if so, retrieve the command
            if (index >= 0 && index < history_count && index < HISTORY_SIZE) {
                strcpy(input, history[index]); // Replace input with the command from history
                printf("Repeating command: %s\n", input);
            } else {
                printf("No such command in history.\n");
                continue; // Skip to the next iteration if command is not found
            }
        }

        // Check if the command should run in the background
        int bg = 0;
        if (input[strlen(input) - 1] == '&') {
            bg = 1;                     // Set background flag to true
            input[strlen(input) - 1] = 0; // Remove '&' from the command
        }

        // Split the input into command and arguments
        char *token = strtok(input, " ");
        int i = 0;
        while (token != NULL) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;  // Set the last argument to NULL as required by execvp

        // Check if there is a command to execute
        if (i > 0) {
            pid_t pid = fork();  // Create a new process
            if (pid < 0) {
                perror("Fork failed");  // Handle fork error
                exit(1);
            }

            if (pid == 0) { // Child process
                if (execvp(args[0], args) < 0) { // Run the command
                    perror("Execution failed"); // Handle execvp error
                    exit(1);
                }
            } else { // Parent process
                if (bg) {
                    // For background processes, print job number and PID
                    job_number++;
                    printf("[%d] %d\n", job_number, pid);
                } else {
                    // Wait for the child process if it's not in the background
                    waitpid(pid, NULL, 0);
                }
            }
        }
    }
    return 0;
}

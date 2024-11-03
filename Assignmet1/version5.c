#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>

#define MAX_LINE 1024     // Maximum size of the input line
#define MAX_ARGS 100      // Maximum number of arguments in a command
#define HISTORY_SIZE 10   // History size to store the last 10 commands
#define MAX_BG_JOBS 100   // Maximum number of background jobs

// Structure to store background job information
typedef struct {
    pid_t pid;                    // Process ID of the background job
    char command[MAX_LINE];       // Command executed as background job
} Job;

// Array and counter to store background jobs
Job bg_jobs[MAX_BG_JOBS];
int bg_job_count = 0;

// Array to store the history of last 10 commands
char history[HISTORY_SIZE][MAX_LINE];
int history_count = 0;

// Function prototypes
void add_to_history(const char *command);
void print_history();
void sigchld_handler(int signo);
void print_jobs();
void kill_job(int job_index);
int is_builtin_command(char **args);
void execute_builtin_command(char **args);

// Adds a command to the history array
void add_to_history(const char *command) {
    if (history_count < HISTORY_SIZE) {
        // Add command to history if there is space
        strcpy(history[history_count++], command);
    } else {
        // If history is full, shift commands up and add the new command at the end
        for (int i = 1; i < HISTORY_SIZE; i++) {
            strcpy(history[i - 1], history[i]);
        }
        strcpy(history[HISTORY_SIZE - 1], command);
    }
}

// Displays the history of commands
void print_history() {
    for (int i = 0; i < history_count && i < HISTORY_SIZE; i++) {
        printf("%d %s\n", i + 1, history[i]);  // Print each command with its index
    }
}

// Handles SIGCHLD to clean up finished background processes
void sigchld_handler(int signo) {
    int status;
    pid_t pid;
    // Iterate over the background jobs array to find finished jobs
    for (int i = 0; i < bg_job_count; i++) {
        pid = waitpid(bg_jobs[i].pid, &status, WNOHANG); // Non-blocking check
        if (pid > 0) {  // Job has finished
            printf("[Finished] %s\n", bg_jobs[i].command);
            // Shift jobs array to remove completed job
            for (int j = i; j < bg_job_count - 1; j++) {
                bg_jobs[j] = bg_jobs[j + 1];
            }
            bg_job_count--; // Decrease job count
            i--; // Adjust index after removal
        }
    }
}

// Lists currently running background jobs
void print_jobs() {
    for (int i = 0; i < bg_job_count; i++) {
        printf("[%d] %d %s\n", i + 1, bg_jobs[i].pid, bg_jobs[i].command); // Print each job's info
    }
}

// Terminates a background job by job number
void kill_job(int job_index) {
    if (job_index < 0 || job_index >= bg_job_count) {
        printf("Invalid job number.\n");  // Check if job number is valid
        return;
    }
    if (kill(bg_jobs[job_index].pid, SIGKILL) == 0) {
        printf("Job %d terminated.\n", job_index + 1); // Confirm job termination
    } else {
        perror("Failed to kill job"); // Error if job couldn't be killed
    }
}

// Check if a command is built-in and handle it if so
int is_builtin_command(char **args) {
    // Check if command is a built-in (e.g., "cd", "exit", "jobs")
    if (strcmp(args[0], "cd") == 0 || strcmp(args[0], "exit") == 0 ||
        strcmp(args[0], "jobs") == 0 || strcmp(args[0], "kill") == 0 ||
        strcmp(args[0], "help") == 0) {
        return 1;
    }
    return 0;
}

// Executes built-in commands directly without forking
void execute_builtin_command(char **args) {
    if (strcmp(args[0], "cd") == 0) {
        // Change directory if "cd" is provided
        if (args[1] == NULL) {
            fprintf(stderr, "cd: missing argument\n");
        } else if (chdir(args[1]) != 0) {
            perror("cd failed");  // Error if chdir fails
        }
    } else if (strcmp(args[0], "exit") == 0) {
        exit(0); // Exit the shell
    } else if (strcmp(args[0], "jobs") == 0) {
        print_jobs(); // List background jobs
    } else if (strcmp(args[0], "kill") == 0) {
        if (args[1] == NULL) {
            fprintf(stderr, "kill: missing job number\n");
        } else {
            int job_index = atoi(args[1]) - 1; // Convert job number from string
            kill_job(job_index); // Kill the specified job
        }
    } else if (strcmp(args[0], "help") == 0) {
        // Display help for built-in commands
        printf("Built-in commands:\n");
        printf("cd <directory>: Change the current working directory.\n");
        printf("exit: Exit the shell.\n");
        printf("jobs: List background jobs.\n");
        printf("kill <job_number>: Kill a background job.\n");
        printf("help: Show this help message.\n");
    }
}

int main() {
    char input[MAX_LINE];
    char *args[MAX_ARGS];
    struct sigaction sa;

    // Set up signal handling for SIGCHLD to clean up background jobs
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    while (1) {
        printf("PUCITshell:- "); // Shell prompt

        // Read input from the user
        if (fgets(input, MAX_LINE, stdin) == NULL) {
            break; // Exit on Ctrl+D
        }
        input[strcspn(input, "\n")] = 0; // Remove newline

        // Add non-history commands to history array
        if (input[0] != '!' && strlen(input) > 0) {
            add_to_history(input);
        }

        // Check if command is background (ends with '&')
        int bg = 0;
        if (input[strlen(input) - 1] == '&') {
            bg = 1; // Background job flag
            input[strlen(input) - 1] = 0; // Remove '&' symbol
        }

        // Parse input into arguments
        char *token = strtok(input, " ");
        int i = 0;
        while (token != NULL) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;

        if (i == 0) continue; // Skip empty commands

        // Check and execute built-in commands
        if (is_builtin_command(args)) {
            execute_builtin_command(args);
            continue;
        }

        // Handle external commands using fork and execvp
        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            exit(1);
        }
        if (pid == 0) { // Child process
            if (execvp(args[0], args) < 0) {
                perror("Execution failed"); // Error if exec fails
                exit(1);
            }
        } else { // Parent process
            if (bg) { // For background jobs
                if (bg_job_count < MAX_BG_JOBS) {
                    bg_jobs[bg_job_count].pid = pid;
                    strcpy(bg_jobs[bg_job_count].command, input);
                    printf("[%d] %d\n", ++bg_job_count, pid); // Show job info
                } else {
                    printf("Max background jobs reached.\n");
                }
            } else {
                waitpid(pid, NULL, 0); // Wait for foreground job to finish
            }
        }
    }
    return 0;
}

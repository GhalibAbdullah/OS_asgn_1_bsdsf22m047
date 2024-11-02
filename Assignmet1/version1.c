#include <stdio.h> 
#include <string.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/wait.h>

#define MAX_LEN 512 
#define MAXARGS 10 
#define ARGLEN 30 
#define PROMPT "PUCITshell:- "

int execute(char* arglist[]);
char** tokensize(char* cmdline);
char* read_cmd(const char* prompt, FILE* fp);  // Changed FILE to FILE*

int main(){
    char *cmdline;
    char** arglist;
    const char* prompt = PROMPT;  // Use const char*

    while((cmdline = read_cmd(prompt, stdin)) != NULL){
        if((arglist = tokensize(cmdline)) != NULL){
            execute(arglist);
            // Freeing memory
            for(int j=0; j < MAXARGS+1; j++) 
                free(arglist[j]);
            free(arglist);
            free(cmdline);
        }
    }   // END OF WHILE
    
    printf("\n");
    return 0;
}

int execute(char* arglist[]){
    int status;
    int cpid = fork();
    
    switch(cpid){
        case -1:
            perror("fork failed");
            exit(1);
        case 0:
            execvp(arglist[0], arglist);
            perror("...Command not found...");
            exit(1);
        default:
            waitpid(cpid, &status, 0);
            printf("child exited with status %d \n", status >> 8);
            return 0;
    }
}

char** tokensize(char* cmdline){
    char** arglist = (char**)malloc(sizeof(char*)* (MAXARGS+1));
    
    for(int j=0; j < MAXARGS+1; j++){
        arglist[j] = (char*)malloc(sizeof(char)* ARGLEN);
        memset(arglist[j], 0, ARGLEN);
    }
    
    if(cmdline[0] == '\0') // if nothing is entered and ENTER is pressed!
        return NULL;
    
    int argnum = 0;         // SLOTS USED
    char* cp = cmdline;     // POS IN STRING
    char* start;
    int len;
    
    while(*cp != '\0'){
        while(*cp == ' ' || *cp == '\t') // SKIP STARTING SPACES
            cp++;
        
        start = cp;         // START OF THE WORD
        len = 1;
        
        // FIND THE END OF THE WORD
        while(*++cp != '\0' && !(*cp == ' ' || *cp == '\t'))
            len++;
        
        strncpy(arglist[argnum], start, len);
        arglist[argnum][len] = '\0';
        argnum++;
    }
    
    arglist[argnum] = NULL;
    return arglist;
}

char* read_cmd(const char* prompt, FILE* fp){  // Changed to FILE* and const char*
    printf("%s", prompt);
    int c;                  // CHARACTER INPUT
    int pos = 0;            // CHARACTER POSITION ON CMD_LINE
    char* cmdline = (char*) malloc(sizeof(char)*MAX_LEN);
    
    while((c = getc(fp)) != EOF){
        if(c == '\n')
            break;
        cmdline[pos++] = c;
    }
    
    // IN CASE USER PRESSES CTRL+D TO EXIT THE SHELL
    if(c == EOF && pos == 0)
        return NULL;
    
    cmdline[pos] = '\0';
    return cmdline;
}


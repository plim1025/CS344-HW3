#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

// Assume user enters arguments separated by spaces
// If "&" is last word, execute command in background, else treat it as normal text
// Assume "<" and ">" are followed by filename
// No need to support quoting (arguments with spaces inside them)
// Support max 2048 chars, 512 arguments
// Don't need to error handle syntax of cli

// Handle 3 commands - exit, cd, status - rest handled with exec()
// cd - with no arguments changes to directory specified by HOME env var, also supports 1 argument (must support both relative/absolute paths)
// status - prints exit status or terminating signal of last foreground process, if no previous, print "exit status 0"
// built-in shell commands don't count as foreground processes, status should ignore these
// don't have to handle input/output redirection for built-in commands

// When any other command other than 3 built in are run
//      fork child
//      exec command
//      if command fails, set exit status to 1
//      terminate child process whether fail or not

// input/output redirection using dup2()
// do redirection before exec()
// input file via stdin - open with read only permissions - if error set exit status to 1, don't exit shell
// output file via stdout - open with write only permissions - if error set exit status to 1, don't exit shell

// foreground (commands without & at end) - must wait for completion before prompting for next
// background (commands with & at end) - shell doesn't have to wait for command to complete before prompting
//      print ID of backgroud process when start
//      when background process terminates, print process id and exit status
//      if user doesn't redirect stdin/stdout, redirect to /dev/null

// CTRL+C
// CTRL+Z

// Swaps $$ for pid
void expandVariable(char input[]) {
    if(strlen(input) > 0) {
        pid_t pid = getpid();
        char pidStr[6];
        sprintf(pidStr, "%d", pid);
        char prevChar = input[0];
        // Iterate through all chars in input
        for(int i = 1; i < strlen(input); i++) {
            // If find $$
            if(prevChar == '$' && input[i] == '$') {
                if(strlen(pidStr) > 2) {
                    // Left shift all chars to right above $$
                    input[strlen(input)+strlen(pidStr)-2] = '\0';
                    for(int j = strlen(input)-1; j > i; j--) {
                        input[j+strlen(pidStr)-2] = input[j];
                    }
                } else {
                    // Right shift all chars to right above $$
                    input[strlen(input)+strlen(pidStr)-2] = '\0';
                    for(int j = i+1; j < strlen(input); j++) {
                        input[j+strlen(pidStr)-2] = input[j];
                    }
                }
                // Replace $$ with pid
                for(int j = 0; j < strlen(pidStr); j++) {
                    input[i+j-1] = pidStr[j];
                }
                i += strlen(pidStr)-1;
            }
            prevChar = input[i];
        }
    }
}

// Stores args in string arr and returns number of args
int parseArgs(char input[], char *args[]) {
    char *savePtr;
    char *token = strtok_r(input, " ", &savePtr);
    int i = 0;
    while(token) {
        args[i] = token;
        token = strtok_r(NULL, " ", &savePtr);
        i++;
    }
    return i;
}

// Handles built-in commands
void builtInCommand(char *args[], int numArgs, char *status) {
    if(!strcmp(args[0], "cd")) {
        if(numArgs == 1) {
            chdir(getenv("HOME"));
        } else {
            chdir(args[1]);
        }
    } else if (!strcmp(args[0], "status")) {
        printf("%s\n", status);
        fflush(stdout);
    } else if (!strcmp(args[0], "exit")) {
        exit(0);
    }
}

// Runs command in foreground
void foregroundCommand(char *args[], char *status, int numArgs) {
    // Fork off child
    pid_t spawnPid = fork();

    // Error forking child
    if(spawnPid == -1) {
        perror("source fork()");
        exit(1);
    // In child
    } else if (spawnPid == 0) {
        int i = 0;
        char *newArgs[numArgs];
        // Find main command and arguments
        while(i < numArgs) {
            // If find stdin/stdout or & operator, args finished
            if(strcmp(args[i], "<") && strcmp(args[i], ">") && (strcmp(args[i], "&") && numArgs == i-1)) {
                break;
            }
            newArgs[i] = args[i];
            i++;
        }
        newArgs[i] = NULL;

        // Redirect stdin/stdout
        while(i < numArgs) {
            if(!strcmp(args[i], "<")) {
                int sourceFD = open(args[i+1], O_RDONLY);
                if(sourceFD == -1) {
                    perror("source open()");
                    exit(1);
                }
                int result = dup2(sourceFD, 0);
                if (result == -1) {
                    perror("source dup2()");
                    exit(1);
                }
                i++;
            } else if (!strcmp(args[i], ">")) {
                int targetFD = open(args[i+1], O_WRONLY);
                if(targetFD == -1) {
                    perror("source open()");
                    exit(1);
                }
                int result = dup2(targetFD, 1);
                if (result == -1) {
                    perror("source dup2()");
                    exit(1);
                }
                i++;
            }
            // Skip over next arg since it is the input/output file
            i++;
        }
        execvp(newArgs[0], newArgs);
        perror("source execvp()");
        exit(1);
    } else {

    }
}

// Runs command in background
void backgroundCommand(char *args[], char *status) {

}

int main() {
    // Support up to 2048 chars and 512 args
    char input[2049];
    char *args[513];
    // Keep asking for user input
    while(1) {
        printf(": ");
        fflush(stdout);
        fgets(input, 2048, stdin);
        input[strlen(input)-1] = '\0';
        char *status = "exit value 0";
        // If line isn't comment, execute it
        if(input[0] != '#') {
            expandVariable(input);
            int numArgs = parseArgs(input, args);
            // If at least one argument, run command
            if(numArgs != 0) {
                if(!strcmp("exit", args[0]) || !strcmp("cd", args[0]) || !strcmp("status", args[0])) {
                    builtInCommand(args, numArgs, status);
                } else if (!strcmp("&", args[numArgs-1])) {
                    // backgroundCommand(args, status);
                } else {
                    foregroundCommand(args, status, numArgs);
                }
            }
        }
    }
    return 0;
}
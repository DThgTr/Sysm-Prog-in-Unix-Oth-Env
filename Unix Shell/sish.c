#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/wait.h>
#include <ctype.h>

#define MAX_CMD_LEN 1000
#define MAX_HIST_CMDS 100

//======================================FUNCTIONS========================================
//Trim beginning and trailing spaces
void trim(char* str) {
    int len = strlen(str);
    int start = 0;
    int end = len - 1;
    //Get beginning spaces
    while (str[start] == ' ') {
        start++;
    }
    //Get trailing spaces
    while (str[end] == ' ') {
        end--;
    }
    //Remove spaces by making a new string
    memmove(str, str + start, end - start + 1); 
    str[end - start + 1] = '\0'; //Null terminate the string
}
//pipeLine
void pipeLine(char ***cmd)
{
	int fd[2];
	pid_t pid;
	int fdin = 0;
    
	while (*cmd != NULL) {
	    int status;
		pipe(fd);	
		pid = fork();
		if (pid == -1) {
			perror("Execution fork() failed");
			break;
		}
		else if (pid == 0) {    
		    //Child process
		    //Redirect read end
			dup2(fdin, STDIN_FILENO);
			//Check for second to last command
			if (*(cmd + 1) != NULL) {   
				dup2(fd[1], STDOUT_FILENO); //Redirect write end
			}
			close(fd[0]);   //Close child pipe read end
			execvp((*cmd)[0], *cmd);    //Execute command
			perror("Cannot execute");
			exit(EXIT_FAILURE);
		}
		else {
		    //Parent process
			//Wait for child
			waitpid(pid, &status, 0);
			//Check child exit state
            if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_FAILURE) {
                break;
            } 
			close(fd[1]);   //Close parent pipe write end
			fdin = fd[0]; //Back up read end
			cmd++;
		}
	}
}
//Shift history to make space for new command
void histShift(char hist[MAX_HIST_CMDS][MAX_CMD_LEN], int histCount) {
    for (int i = 0; i < histCount; i++) {
        if (hist[i] == NULL || hist[i+1] == NULL) //Shift the value left 
            break;
        strcpy(hist[i], hist[i+1]);
    }
}
//Check if a string is a valid integer
int isInt(char *str) {
    if (*str == '\0') {
        return 0;
    }
    while (*str != '\0') {
        if (!isdigit(*str)) {
            return 0;
        }
        str++;
    }
    return 1;
}
//Print custome error message
void printError(const char *errMsg) {
    // If the command is not recognized, print an error message
    printf("%s\n", errMsg);
}
//Parse a string for token
void parseCmd(char *cmdStr, char **cmd, const char* delim) {
    char *token = strtok(cmdStr, delim);
    int i = 0;
    while (token != NULL && i < 1000 - 1) {
        strcpy(cmd[i], token); 
        trim(cmd[i]);
        token = strtok(NULL, delim);
        i++;
    }
    cmd[i] = NULL;  // NULL terminate the char array
}
//Fork and execute command
void forkExec(char* cmd[MAX_CMD_LEN]) {
    // Fork a new process to execute the command
    pid_t pid = fork();

    if (pid == 0) {
        // Child process
        execvp(cmd[0], cmd);
        perror("Cannot execute");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Parent process
        wait(NULL); //Wait for child
    } else {
        // Fork failed
        perror("Execution fork() failed");
    }
}
//Free allocated array
void freeStrArr(char **arr, int size) {
    for (int i = 0; i < size; i++) {
        free(arr[i]); // free the memory for the string pointed to by each element
    }
}

//=======================================MAIN==========================================
int main(int argc, char *argv[]) {
    //input buffer and holder
    char *buffer = NULL;
    size_t bufSize = MAX_CMD_LEN;
    //history variables
    char history[MAX_HIST_CMDS][MAX_CMD_LEN];
    int histCount = 0;

    // Loop indefinitely until the user enters the "exit" command
    while (1) {
        char cmdStr[MAX_CMD_LEN];
        //pipe variables
        //Allocate memory for pipeComdsRaw
        char* pipeComdsRaw[MAX_CMD_LEN];
        for (int i = 0; i < MAX_CMD_LEN; i++) {
            pipeComdsRaw[i] = (char *) malloc(MAX_CMD_LEN * sizeof(char));
        }
        int pipeNum = 0;
        
        // Prompt the user for input
        printf("sish>> ");

        // Read the user's input
        getline(&buffer, &bufSize, stdin);
        strcpy(cmdStr, buffer);

        // Remove the newline character from the end of the input
        cmdStr[strcspn(cmdStr, "\n")] = '\0';

//================================================BODY================================================
        //get command separated by pipe
        parseCmd(cmdStr, pipeComdsRaw, "|");
        
        // Save the command to the history
        while (pipeComdsRaw[pipeNum] != NULL) {
            if (histCount > 100) {
                histShift(history, histCount);
                histCount--; //decrement histCount back to the correct value
            }
            strcpy(history[histCount], pipeComdsRaw[pipeNum]);
            //Increment histCount and pipeNum
            histCount++;
            pipeNum++;
        }
        //=============================PIPE COMMANDS===================================
        //If there are pipe commands
        if (pipeNum > 1) {
            //pipe command preparation
            char **pipeComd[MAX_CMD_LEN]; //Array of pointer of command
            for (int i = 0; i < pipeNum; i++) {
                pipeComd[i] = (char**) malloc(sizeof(char*) * MAX_CMD_LEN); // Allocate memory for an array of MAX_CMD_LEN
                for (int j = 0; j < MAX_CMD_LEN; j++) {
                    pipeComd[i][j] = (char*) malloc(sizeof(char) * MAX_CMD_LEN); // Allocate memory for a string of up to MAX_CMD_LEN
                }
            }
            //Parse command
            for (int i = 0; i < pipeNum; i++) {
                char cmdStr[MAX_CMD_LEN];
                strcpy(cmdStr, pipeComdsRaw[i]);
                parseCmd(cmdStr, pipeComd[i], " ");
            }
            pipeComd[pipeNum] = NULL;
            pipeLine(pipeComd); //Execute pipe commands
            //Free the allocated pipeComd
            for (int i = 0; i < pipeNum; i++) {
                for (int j = 0; j < MAX_CMD_LEN; j++) {
                    free(pipeComd[i][j]); // Free the memory allocated for each string
                }
                free(pipeComd[i]); // Free the memory allocated for the array of pointers
            }
            //Free pipeComdsRaw
            freeStrArr(pipeComdsRaw, MAX_CMD_LEN);
            continue;
        }
        //----------------NO PIPE----------------
        // Tokenize the input command if there's no pipe
        strcpy(cmdStr, pipeComdsRaw[0]);
        
        //Free allocated memory for pipeComdsRaw
        freeStrArr(pipeComdsRaw, MAX_CMD_LEN);
        //Allocate memory for cmd
        char *cmd[MAX_CMD_LEN];
        for (int i = 0; i < MAX_CMD_LEN; i++) {
            cmd[i] = (char *) malloc(MAX_CMD_LEN * sizeof(char));
        }
        parseCmd(cmdStr, cmd, " ");
        //=============================BUILT-IN COMMANDS===============================
        // Check for the 'exit' command
        if (strcmp(cmd[0], "exit") == 0) {
            freeStrArr(cmd, MAX_CMD_LEN);
            break;
        }
        //-----------------------------------------------------------------------------
        // Check for the 'history' command
        if (strcmp(cmd[0], "history") == 0) {
            if (cmd[1] == NULL) {
                for (int i = 0; i < histCount && i < MAX_HIST_CMDS; i++) {
                    printf("%d: %s\n", i, history[i]);
                }
                freeStrArr(cmd, MAX_CMD_LEN);
                continue;
            }
            if (strcmp(cmd[1], "-c") == 0 && cmd[2] == NULL) {
                histCount = 0;  // Reset histCount
                freeStrArr(cmd, MAX_CMD_LEN);
                continue;
            } 
            else if (isInt(cmd[1]) == 1 && cmd[2] == NULL) {
                int offset = atoi(cmd[1]);
                //check if offset out of bound
                if (offset < histCount) {
                    //Set cmd to corresponding command in history
                    char comd[MAX_CMD_LEN];
                    strcpy(comd, history[offset]);
                    //parse the command
                    parseCmd(comd, cmd, " ");  
                }
                else {
                    printError("Offset out of bound\nUsage: history + 'no argument/-c/offset'");
                    freeStrArr(cmd, MAX_CMD_LEN);
                    continue;
                }
            }
            else {
                printError("Usage: history + 'no argument/-c/offset'");
                freeStrArr(cmd, MAX_CMD_LEN);
                continue;
            }
        }
        //-----------------------------------------------------------------------------
        // Check for the 'cd' command
        if (strcmp(cmd[0], "cd") == 0) {
            if (cmd[1] != NULL) {
                if (chdir(cmd[1]) == 0)
                    continue;
            }
            printError("Usage: cd + 'valid directory'");
            freeStrArr(cmd, MAX_CMD_LEN);
            continue;
        }
        //================================EXECUTABLE====================================
        // Fork and execute command
        forkExec(cmd);
        
        //Free allocated memory for cmd
        freeStrArr(cmd, MAX_CMD_LEN);
    }
//============================================================================================
    exit(EXIT_SUCCESS);

    return 0;
}
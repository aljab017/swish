// Header file for swish helper functions
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

#include "string_vector.h"
#include "swish_funcs.h"

#define MAX_ARGS 10

/*
 * Helper function to close all file descriptors in an array.
 * fds: An array of file descriptor values (e.g., from an array of pipes)
 * n: Number of file descriptors to close
 */
int close_all(int *fds, int n) {
    int ret_val = 0;
    for (int i = 0; i < n; i++) {
        if (close(fds[i]) == -1) {
            perror("close");
            ret_val = 1;
        }
    }
    return ret_val;
}

/*
 * Helper function to run a single command within a pipeline.
 * tokens: String vector containing the tokens representing the command to be
 * executed, possible redirection, and the command's arguments.
 * pipes: An array of pipe file descriptors.
 * n_pipes: Length of the 'pipes' array
 * in_idx: Index of the file descriptor in the array from which the program
 *         should read its input, or -1 if input should not be read from a pipe.
 * out_idx: Index of the file descriptor int he array to which the program
 *          should write its output, or -1 if output should not be written to
 *          a pipe.
 * Returns 0 on success or 1 on error.
 */
int run_piped_command(strvec_t *tokens, int *pipes, int n_pipes, int in_idx, int out_idx) {
  
    // run first commmand
    if(in_idx == -1){
        pid_t child_pid = fork();
        if (child_pid == -1) {
            perror("fork");
            close_all(pipes, n_pipes);
            return 1;
        } else if (child_pid == 0){
            if (dup2(pipes[out_idx], STDOUT_FILENO) == -1) {
                perror("dup2");
                close_all(pipes, n_pipes);
                return 1;
            }
            if(close(pipes[0])==-1){
                perror("close1");
                return 1;
            }
            if(run_command(tokens)==1){
                perror("run command1 faild\n");
                return 1;
            } 
            _exit(1);
        }
        else{
            if(close(pipes[1])==-1){
                perror("close");
                return 1;
            }
            if(wait(NULL)==-1){
                perror("wait");
                return 1;
            }
        }
    }

    //run last command
    else if(out_idx == -1){
        pid_t child_pid = fork();
        if (child_pid == -1) {
            perror("fork");
            close_all(pipes, n_pipes);
            return 1;
        } else if (child_pid == 0){
            if (dup2(pipes[in_idx], STDIN_FILENO) == -1) {
                perror("dup2");
                close_all(pipes, n_pipes);
                return 1;
            }
            if (close(pipes[in_idx])==1){
                perror("close");
                return 1;
            }
            if(run_command(tokens)==1){
                perror("run command1 faild\n");
                return 1;
            } 
            _exit(1);
        } else{
            if (close(pipes[in_idx])==1){
                perror("close");
                return 1;
            }
            if(wait(NULL)==-1){
                perror("wait");
                return 1;
            }
        }
    }
    
    // run interior commands 
    else{
        pid_t child_pid = fork();
        if (child_pid == -1) {
            perror("fork");
            close_all(pipes,n_pipes);
            return 1;
        } else if (child_pid == 0){           
            if (dup2(pipes[in_idx], STDIN_FILENO) == -1) {
                perror("dup2");
                close_all(pipes,n_pipes);
                return 1;
            }
            if (dup2(pipes[out_idx], STDOUT_FILENO) == -1) {
                perror("dup2");
                close_all(pipes,n_pipes);
                return 1;
            }
            
            if (close(pipes[out_idx])==1){
                perror("close");
                return 1;
            }
            if (close(pipes[in_idx])==1){
                perror("close");
                return 1;
            }
            if(run_command(tokens)==1){
                perror("run command faild\n");
                return 1;
            } 
            _exit(1);
        } else {
            if (close(pipes[out_idx])==1){
                perror("close");
                return 1;
            }
            if(close(pipes[in_idx])==1){
                perror("close");
                return 1;
            }
            if(wait(NULL)==-1){
                perror("wait");
                return 1;
            }
        }
    }
    return 0;
}

int run_pipelined_commands(strvec_t *tokens) {

    //count the number of pipes
    int pipesCount = strvec_num_occurrences(tokens, "|"); 

    //create pipes
    int pipes[2 * pipesCount];
    for (int i = 0; i < pipesCount; i++) {
        if (pipe(pipes + 2*i) == -1) {
            perror("pipe");
            close_all(pipes, 2*i);
            return 1;
        }
    }

    //compute pipes' indices and store them in pipesIdx
    int pipesIdx[pipesCount]; 
    int j =0; //index in pipesIdx
    for(int i =0; i<tokens->length; i++){
        if(strcmp(strvec_get(tokens, i), "|")==0){
           pipesIdx[j] = i; 
           j++;
        }
    }
  
    //run first command
    int firstPipeIdx = strvec_find(tokens, "|"); 
    strvec_t first[MAX_ARGS];
    strvec_init(first);
    for(int i =0; i<tokens->length; i++){
        strvec_add(first, strvec_get(tokens,i));
    }
    strvec_take(first, firstPipeIdx);
    if (run_piped_command(first, pipes, pipesCount, -1, 1)==1){
        perror("run_piped_command");
        return 1;
    }
    
    // run interior commands
    if(pipesCount>1){
        for(int i=0; i < pipesCount-1; i++){
            strvec_t new[MAX_ARGS];
            strvec_slice(tokens, new, pipesIdx[i]+1, pipesIdx[i+1]);
            if (run_piped_command(new, pipes, pipesCount, i*2, i*2+3)==1){
                perror("run_piped_command");
                return 1;
            }
        }
    }
    
    //run last command
    strvec_t new[MAX_ARGS];
    strvec_slice(tokens, new, pipesIdx[pipesCount-1]+1, tokens->length);
    if (run_piped_command(new, pipes, pipesCount, (pipesCount-1)*2, -1)==1){
        perror("run_piped_command");
        return 1;
    }
    return 0;
}



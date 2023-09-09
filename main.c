#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>

#define MAX_INPUTS 2000
#define MAX_TOKENS 1000
#define MAX_ARG_CHARS 30

void redirect_command(int file_descriptors[][3], char *args[]);

int file_descriptors[2][3] = {{-1, -1, -1}, {-1, -1, -1}};

int main()
{
    char input[MAX_INPUTS];
    char *token = (char *)malloc((MAX_ARG_CHARS) * sizeof(char));
    char *command_args = (char *)malloc((MAX_INPUTS) * sizeof(char));
    char *args[2][MAX_TOKENS];

    int i, j, k;
    int row;
    int token_num;
    int store_token_flag; // flag to stop storing tokens
    // int redirect_idx[2][3] = {-1}; // 1st row: <, >, 2>; 2nd row: <, >, 2>;
    int exit_flag = 0; // 0: dont exit, 1: exit

    int redirect_input_flag = 0;  // idx of "<"
    int redirect_output_flag = 0; // idx of ">"
    int redirect_error_flag = 0;  // idx of "2>"

    int prev_descriptor; // previous descriptor

    int pipe_fd[2];

    do
    {
        // Get user input
        printf("#");
        fgets(input, MAX_INPUTS, stdin);

        // Delete input of '\n'
        input[strlen(input) - 1] = 0;

        // Parse inputs for tokens with delimeters of ' '
        char *input_address = input;

        // Dictate which row to save in arg matrix
        row = 0;

        // Seperate commands
        while (command_args = strtok_r(input_address, "|", &input_address))
        {
            // Reset Token count
            token_num = 0;

            // Set flag when token reaches a symbol
            store_token_flag = 1;

            // Parse Tokens in input
            while (token = strtok_r(command_args, " ", &command_args))
            {
                // If there is more than one pipe
                if (row == 2)
                {
                    break;
                }
                // Set off flag whenever one of the redirect tokens are parsed
                store_token_flag = (strcmp(token, "<") == 0 || strcmp(token, ">") == 0 || strcmp(token, "2>") == 0) ? 0 : store_token_flag;

                // Parse command
                if (store_token_flag)
                {
                    // Allocate memory for each token parsed
                    args[row][token_num] = (char *)malloc((strlen(token) * sizeof(char)));
                    args[row][token_num] = token;

                    // Increment number of tokens
                    token_num++;
                }
                else
                {
                    // Depeneding on which redirect flag, open corresponding file descriptor
                    if (redirect_input_flag)
                    {
                        file_descriptors[row][0] = open(token, O_RDONLY);
                        if (file_descriptors[row][0] == -1)
                        {
                            perror("File descriptor error\n"); // Change the error to skip to next token
                            exit(1);
                        }
                        redirect_input_flag = 0;
                    }
                    else if (redirect_output_flag)
                    {
                        file_descriptors[row][1] = open(token, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
                        if (file_descriptors[row][1] == -1)
                        {
                            perror("File descriptor error\n");
                            exit(1);
                        }
                        redirect_output_flag = 0;
                    }
                    else if (redirect_error_flag)
                    {
                        file_descriptors[row][2] = open(token, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
                        if (file_descriptors[row][2] == -1)
                        {
                            perror("File descriptor error\n");
                            exit(1);
                        }
                        redirect_error_flag = 0;
                    }
                    else
                    {
                        // Set Flags when symbols are in token
                        redirect_input_flag = (strcmp(token, "<") == 0) ? 1 : redirect_input_flag;
                        redirect_output_flag = (strcmp(token, ">") == 0) ? 1 : redirect_output_flag;
                        redirect_error_flag = (strcmp(token, "2>") == 0) ? 1 : redirect_error_flag;
                    }
                }
            }

            // Set the last token to NULL for execvp to run
            args[row][token_num] = NULL;

            // Increment row for next pipe
            row++;
        }

        // Check if user wants to exit
        if (strcmp(args[0][0], "exit") == 0)
        {
            exit_flag = 1;
        }
        else if (row > 1)
        {
            // Pipe
            if (pipe(pipe_fd) == -1)
            {
                perror("pipe");
                exit(1);
            }
            pid_t cpid1 = fork();
            if (cpid1 == 0)
            {
                // Close the read end of pipe
                close(pipe_fd[0]);

                // Dup2 to place output into pipe
                if (file_descriptors[0][1] == -1)
                {
                    dup2(pipe_fd[1], STDOUT_FILENO);
                }

                // Close the write end of pipe
                close(pipe_fd[1]);
                redirect_command(&file_descriptors[0], args[0]);
                exit(1);
            }
            else if (cpid1 > 0)
            {
                // Close the read end of pipe
                close(pipe_fd[1]);

                int status;
                waitpid(cpid1, &status, 0);

                pid_t cpid2 = fork();

                if (cpid2 == 0)
                {
                    dup2(pipe_fd[0], STDIN_FILENO);
                    close(pipe_fd[0]);
                    redirect_command(&file_descriptors[1], args[1]);
                    exit(1);
                }
                else if (cpid2 > 0)
                {
                    close(pipe_fd[0]);
                    int status;
                    waitpid(cpid2, &status, 0);
                }
                else
                {
                    // Fork error
                    perror("fork error\n");
                }
            }
            else
            {
                // Fork error
                perror("fork error\n");
            }
        }
        else
        {
            redirect_command(file_descriptors, args[0]);
        }
    } while (!exit_flag);

    // Free allocated memory
    free(token);
}

void redirect_command(int file_descriptors[][3], char *args[])
{
    pid_t cpid;

    // Create a child process
    cpid = fork();

    if (cpid == 0)
    {
        // Dup2 for the file redirects
        if (file_descriptors[0][0] != -1)
        {
            if (dup2(file_descriptors[0][0], STDIN_FILENO) == -1)
            {
                perror("Dup2 STDIN error\n");
                exit(1);
            }
        }
        if (file_descriptors[0][1] != -1)
        {
            if (dup2(file_descriptors[0][1], STDOUT_FILENO) == -1)
            {
                perror("Dup2 STDOUT error\n");
                exit(1);
            }
        }
        if (file_descriptors[0][2] != -1)
        {
            if (dup2(file_descriptors[0][2], STDERR_FILENO) == -1)
            {
                perror("Dup2 STDERR error\n");
                exit(1);
            }
        }

        // Execute command
        execvp(args[0], args);

        // Error handling if child process doesn't terminate (Should remove for final submission)
        perror("execvp error\n");
        exit(1);
    }
    else if (cpid > 0)
    {
        // Wait for child process to finish running
        int status;
        waitpid(cpid, &status, 0);

        // Close File descriptor
        for (int i = 0; i < 3; i++)
        {
            if (file_descriptors[0][i] != -1)
            {
                close(file_descriptors[0][i]);
            }
            // Clear File Descriptor
            file_descriptors[0][i] = -1;
        }
    }
    else
    {
        // Fork error
        perror("fork error\n");
    }
}
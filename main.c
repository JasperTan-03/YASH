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

void parse_input(char *input_address, int file_descriptors[][3], char *args[][MAX_TOKENS], int *output);

void execute_command(int file_descriptors[][3], char *args[]);

void pipe_command(int file_descriptors[][3], char *args[][(MAX_TOKENS)]);

void close_fd(int file_descriptors[][3]);

int main()
{
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    char input[MAX_INPUTS];
    char *token = (char *)malloc((MAX_ARG_CHARS) * sizeof(char));
    char *args[2][MAX_TOKENS];

    int row;
    int token_num;
    int store_token_flag;  // flag to stop storing tokens
    int exit_flag;         // 0: dont exit, 1: exit
    int next_command_flag; // 0: countinue, 1: next token
    int output_flags[2];

    int redirect_input_flag = 0;  // idx of "<"
    int redirect_output_flag = 0; // idx of ">"
    int redirect_error_flag = 0;  // idx of "2>"
    int file_descriptors[2][3] = {{-1, -1, -1}, {-1, -1, -1}};

    int prev_descriptor; // previous descriptor

    struct Job
    {

    } do
    {
        // Get user input
        printf("#");
        fgets(input, MAX_INPUTS, stdin);

        // Check for ctrl D
        if (input[strlen(input) - 1] == '\0')
        {
            exit(1);
        }

        // Delete input of '\n'
        input[strlen(input) - 1] = 0;

        // Parse inputs for tokens with delimeters of ' '
        char *input_address = input;

        parse_input(input_address, file_descriptors, args, output_flags);

        row = output_flags[0];

        next_command_flag = output_flags[1];

        // Check if user wants to exit
        if (!next_command_flag)
        {
            if (strcmp(args[0][0], "exit") == 0)
            {
                exit_flag = 1;
            }
            else if (row > 1) // pipe
            {
                pipe_command(file_descriptors, args);
                close_fd(file_descriptors);
            }
            else // Regular command
            {
                execute_command(file_descriptors, args[0]);
                close_fd(file_descriptors);
            }
        }
    }
    while (!exit_flag)
        ;

    // Free allocated memory
    free(token);
}

void parse_input(char *input_address, int file_descriptors[][3], char *args[][MAX_TOKENS], int output_flags[2])
{
    int redirect_input_flag = 0;  // idx of "<"
    int redirect_output_flag = 0; // idx of ">"
    int redirect_error_flag = 0;  // idx of "2>"

    // Dictate which row to save in arg matrix
    int row = 0;

    // Reset Token count
    int token_num = 0;

    // Set flag when token reaches a symbol
    int store_token_flag = 1;

    // Set flag when file descriptor is incorrect
    int next_command_flag = 1;

    char *token = (char *)malloc((MAX_ARG_CHARS) * sizeof(char));
    // Parse Tokens in input
    while (token = strtok_r(input_address, " ", &input_address))
    {
        next_command_flag = 0;

        // If there is more than one pipe
        if (row == 2)
        {
            break;
        }
        else if (strcmp(token, "|") == 0)
        {
            // Set flag when token reaches a symbol
            store_token_flag = 1;

            // Set the last token to NULL for execvp to run
            args[row][token_num] = NULL;

            // Increment row for next pipe
            row++;

            // Reset Token count
            token_num = 0;
        }
        else
        {
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

                // Set the last token to NULL for execvp to run
                args[row][token_num] = NULL;
            }
            else
            {
                // Depeneding on which redirect flag, open corresponding file descriptor
                if (redirect_input_flag)
                {
                    file_descriptors[row][0] = open(token, O_RDONLY);
                    if (file_descriptors[row][0] == -1)
                    {
                        perror("no directory");
                        next_command_flag = 1;
                    }

                    redirect_input_flag = 0;
                }
                else if (redirect_output_flag)
                {
                    file_descriptors[row][1] = open(token, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
                    if (file_descriptors[row][1] == -1)
                    {
                        perror("no directory");
                        next_command_flag = 1;
                    }
                    redirect_output_flag = 0;
                }
                else if (redirect_error_flag)
                {
                    file_descriptors[row][2] = open(token, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
                    if (file_descriptors[row][2] == -1)
                    {
                        perror("no directory");
                        next_command_flag = 1;
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
    }
    row++;
    free(token);

    output_flags[0] = row;
    output_flags[1] = next_command_flag;
}

void execute_command(int file_descriptors[][3], char *args[])
{
    pid_t cpid;

    // Create a child process
    cpid = fork();

    if (cpid == 0)
    {
        // Set the signal to default
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);

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
    }
    else
    {
        // Fork error
        perror("fork error\n");
    }
}

void pipe_command(int file_descriptors[][3], char *args[][(MAX_TOKENS)])
{
    // Create pipe
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1)
    {
        perror("pipe");
        exit(1);
    }

    // First fork
    pid_t cpid1 = fork();
    if (cpid1 == 0)
    {
        // Child process

        // Set the signal to default
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);

        // Close the read end of pipe
        close(pipe_fd[0]);

        // Place output into pipe only if nothing is overriding it
        if (file_descriptors[0][1] == -1)
        {
            dup2(pipe_fd[1], STDOUT_FILENO);
        }

        // Close the write end of pipe
        close(pipe_fd[1]);

        execute_command(&file_descriptors[0], args[0]);
        exit(1);
    }
    else if (cpid1 > 0)
    {
        // Close the read end of pipe
        close(pipe_fd[1]);

        // Wait for first child process to finish
        int status;
        waitpid(cpid1, &status, 0);

        // Another fork to run other side of pipe
        pid_t cpid2 = fork();
        if (cpid2 == 0)
        {
            // Get input from pipe based on if pipe was set by command
            if (file_descriptors[1][0] == -1)
            {
                dup2(pipe_fd[0], STDIN_FILENO);
            }

            // Close pipe and execute command
            close(pipe_fd[0]);
            execute_command(&file_descriptors[1], args[1]);
            exit(1);
        }
        else if (cpid2 > 0)
        {
            // Close pipes and wait for child process to finish
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

void close_fd(int file_descriptors[][3])
{
    // Close File descriptor
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            if (file_descriptors[i][j] != -1)
            {
                close(file_descriptors[i][j]);
            }
            file_descriptors[i][j] = -1;
        }
    }
}
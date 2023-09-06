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

#define MAX_INPUTS 2000
#define MAX_TOKENS 1000
#define MAX_ARG_CHARS 30

int main()
{
    pid_t child_pid;
    char input[MAX_INPUTS];
    char *token = (char *)malloc((MAX_ARG_CHARS) * sizeof(char));
    char *args[MAX_TOKENS];

    int i;
    int row;
    int token_num;
    int store_token_flag; // flag to stop storing tokens
    // int redirect_idx[2][3] = {-1}; // 1st row: <, >, 2>; 2nd row: <, >, 2>;
    int exit_flag = 0; // 0: dont exit, 1: exit

    int redirect_input_flag = 0;  // idx of "<"
    int redirect_output_flag = 0; // idx of ">"
    int redirect_error_flag = 0;  // idx of "2>"
    int input_file_descriptor = -1;
    int output_file_descriptor = -1;
    int error_file_descriptor = -1;
    int prev_descriptor; // previous descriptor

    do
    {
        // Get user input
        printf("#");
        fgets(input, MAX_INPUTS, stdin);

        // Delete input of '\n'
        input[strlen(input) - 1] = 0;

        // Parse inputs for tokens with delimeters of ' '
        token_num = 0;
        char *input_address = input;

        // Dictate which row to save in redirect matrix
        row = 0;

        // Set flag when token reaches a symbol
        store_token_flag = 1;

        // Parse Tokens in input
        while (token = strtok_r(input_address, " ", &input_address))
        {
            // Set off flag whenever one of the redirect tokens are parsed
            store_token_flag = (strcmp(token, "<") == 0 || strcmp(token, ">") == 0 || strcmp(token, "2>") == 0) ? 0 : store_token_flag;

            // Parse command
            if (store_token_flag)
            {
                // Allocate memory for each token parsed
                args[token_num] = (char *)malloc((strlen(token) * sizeof(char)));
                args[token_num] = token;

                // Increment number of tokens
                token_num++;
            }
            else
            {
                // Depeneding on which redirect flag, open corresponding file descriptor
                if (redirect_input_flag)
                {
                    input_file_descriptor = open(token, O_RDONLY);
                    if (input_file_descriptor == -1)
                    {
                        perror("File descriptor error\n");
                        exit(1);
                    }
                    redirect_input_flag = 0;
                }
                else if (redirect_output_flag)
                {
                    output_file_descriptor = open(token, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
                    if (output_file_descriptor == -1)
                    {
                        perror("File descriptor error\n");
                        exit(1);
                    }
                    redirect_output_flag = 0;
                }
                else if (redirect_error_flag)
                {
                    error_file_descriptor = open(token, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
                    if (error_file_descriptor == -1)
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
        args[token_num] = NULL;

        // Check if user wants to exit
        if (strcmp(args[0], "exit") == 0)
        {
            exit_flag = 1;
        }
        else
        {
            // Create a child process
            child_pid = fork();

            if (child_pid == 0)
            {
                // Dup2 for the file redirects
                if (input_file_descriptor != -1)
                {
                    if (dup2(input_file_descriptor, STDIN_FILENO) == -1)
                    {
                        perror("Dup2 STDIN error\n");
                        exit(1);
                    }
                }
                if (output_file_descriptor != -1)
                {
                    if (dup2(output_file_descriptor, STDOUT_FILENO) == -1)
                    {
                        perror("Dup2 STDOUT error\n");
                        exit(1);
                    }
                }
                if (error_file_descriptor != -1)
                {
                    if (dup2(error_file_descriptor, STDERR_FILENO) == -1)
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
            else if (child_pid > 0)
            {
                // Wait for child process to finish running
                int status;
                waitpid(child_pid, &status, 0);

                // Close File descriptor
                if (input_file_descriptor != -1)
                    close(input_file_descriptor);
                if (output_file_descriptor != -1)
                    close(output_file_descriptor);
                if (error_file_descriptor != -1)
                    close(error_file_descriptor);

                // Clear File descriptor
                input_file_descriptor = -1;
                output_file_descriptor = -1;
                error_file_descriptor = -1;
            }
            else
            {
                // Fork error
                perror("fork error\n");
            }
        }
    } while (!exit_flag);

    // Free allocated memory
    free(token);
}

int main2()
{
    int file_descriptor;

    char *data[] = {"ls", "<", "output.txt"};

    file_descriptor = open("test_files/output.txt", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

    if (file_descriptor == -1)
    {
        perror("open");
        exit(1);
    }

    if (dup2(file_descriptor, 1) == -1)
    {
        perror("dup2");
        exit(1);
    }
    pid_t child_pid;
    child_pid = fork();
    char *args[] = {"ls", "-l", NULL};
    if (child_pid == 0)
    {
        execvp(args[0], args);
        exit(1);
    }
    else if (child_pid > 0)
    {
        int status;
        waitpid(child_pid, &status, 0);
    }
    else
    {
        perror("fork error");
    }

    close(file_descriptor);
    return 0;
}

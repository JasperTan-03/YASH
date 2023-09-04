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

int main2()
{
    pid_t child_pid;
    char input[MAX_INPUTS];
    char *token = (char *)malloc((MAX_ARG_CHARS) * sizeof(char));
    char *args[MAX_TOKENS];

    int i;
    int row;
    int token_num;
    int redirect_idx[2][3] = {-1}; // 1st row: <, >, 2>; 2nd row: <, >, 2>;
    int exit_flag = 0;             // 0: dont exit, 1: exit

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

        while (token = strtok_r(input_address, " ", &input_address))
        {
            // Allocate memory for each token parsed
            args[token_num] = (char *)malloc((strlen(token) * sizeof(char)));
            args[token_num] = token;

            // Cases to store location of redirects
            redirect_idx[row][0] = (strcmp(args[token_num], "<") == 0) ? token_num : redirect_idx[row][0];
            redirect_idx[row][1] = (strcmp(args[token_num], ">") == 0) ? token_num : redirect_idx[row][0];
            redirect_idx[row][2] = (strcmp(args[token_num], "2>") == 0) ? token_num : redirect_idx[row][0];
            row = (strcmp(args[token_num], "|") == 0) ? 1 : row;

            // Increment number of tokens
            token_num++;
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

int main()
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

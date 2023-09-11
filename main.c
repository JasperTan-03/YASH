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

typedef struct Job
{
    int row;               // number of rows (for pipe)
    int next_command_flag; // 0: dont skip command, 1: skip command
    int background_flag;   // 0: foreground, 1: background
    int job_completed;     // 0: incomplete, 1: complete
    pid_t job_pid;
    int job_order; // Number to print for job number
    char input[MAX_INPUTS];
    int file_descriptors[2][3];
    char *args[2][MAX_TOKENS];
} Job;

Job bg_job_list[20];
int bg_job_num = 1;
int bg_job_idx = 0;

void parse_input(char *input_address, Job *current_job);

void execute_command(Job *current_job, int row_param);

void single_command(Job *current_job);

void pipe_command(Job *current_job);

void close_fd(int file_descriptors[][3]);

void update_job_list(int del_idx);

void check_completed_jobs();

int main()
{
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    // char input[MAX_INPUTS];
    char *token = (char *)malloc((MAX_ARG_CHARS) * sizeof(char));
    char input[MAX_INPUTS];

    int exit_flag; // 0: dont exit, 1: exit
    int output_flags[3];

    do
    {
        memset(input, 0, strlen(input));

        // Get user input
        printf("# ");
        fgets(input, MAX_INPUTS, stdin);

        // Check for ctrl D
        if (input[strlen(input) - 1] == '\0')
        {
            exit(1);
        }

        // Delete input of '\n'
        input[strlen(input) - 1] = 0;
        char *input_address = input;

        // Initialize new Job
        Job fg_job = {};
        memset(fg_job.file_descriptors, -1, sizeof(fg_job.file_descriptors));
        fg_job.job_completed = 0;

        // Parse inputs for tokens with delimeters of ' '
        parse_input(input_address, &fg_job);

        Job *current_job;
        current_job = fg_job.background_flag ? &bg_job_list[bg_job_idx - 1] : &fg_job;

        // Check if user wants to exit
        if (!current_job->next_command_flag)
        {
            if (strcmp(current_job->args[0][0], "exit") == 0)
            {
                exit_flag = 1;
            }
            else if (current_job->row > 1) // pipe
            {
                pipe_command(current_job);
                if (current_job->background_flag != 1)
                {
                    close_fd(current_job->file_descriptors);
                }
                // update_job_list(bg_job_list, current_job_idx, job_num);
                // job_num--;
            }
            else // Regular command
            {
                single_command(current_job);
                if (current_job->background_flag != 1)
                {
                    close_fd(current_job->file_descriptors);
                }
                // update_job_list(bg_job_list, current_job_idx, job_num);
                // job_num--;
            }
        }
        check_completed_jobs();

    } while (!exit_flag);

    // Free allocated memory
    free(token);
}

void parse_input(char *input_address, Job *current_job)
{
    int redirect_input_flag = 0;  // idx of "<"
    int redirect_output_flag = 0; // idx of ">"
    int redirect_error_flag = 0;  // idx of "2>"

    // Dictate which row to save in arg matrix
    current_job->row = 0;

    // Reset Token count
    int token_num = 0;

    // Set flag when token reaches a symbol
    int store_token_flag = 1;

    // Set flag when file descriptor is incorrect
    current_job->next_command_flag = 1;

    current_job->background_flag = 0;

    char *token = (char *)malloc((MAX_ARG_CHARS) * sizeof(char));
    // Parse Tokens in input
    while (token = strtok_r(input_address, " ", &input_address))
    {
        current_job->next_command_flag = 0;

        // If there is more than one pipe
        if (current_job->row == 2)
        {
            break;
        }
        else if (strcmp(token, "|") == 0)
        {
            // Set flag when token reaches a symbol
            store_token_flag = 1;

            // Set the last token to NULL for execvp to run
            current_job->args[current_job->row][token_num] = NULL;

            // Increment row for next pipe
            current_job->row++;

            // Reset Token count
            token_num = 0;
        }
        else if (strcmp(token, "&") == 0)
        {
            current_job->background_flag = 1;
            current_job->job_order = bg_job_num;
            bg_job_list[bg_job_idx] = *current_job;
            bg_job_list[bg_job_idx].row++;
            bg_job_idx++;
            bg_job_num++;
        }
        else
        {
            // Set off flag whenever one of the redirect tokens are parsed
            store_token_flag = (strcmp(token, "<") == 0 || strcmp(token, ">") == 0 || strcmp(token, "2>") == 0) ? 0 : store_token_flag;

            // Parse command
            if (store_token_flag)
            {
                // Allocate memory for each token parsed
                current_job->args[current_job->row][token_num] = (char *)malloc((strlen(token) * sizeof(char)));
                current_job->args[current_job->row][token_num] = token;

                // Increment number of tokens
                token_num++;

                // Set the last token to NULL for execvp to run
                current_job->args[current_job->row][token_num] = NULL;
            }
            else
            {
                // Depeneding on which redirect flag, open corresponding file descriptor
                if (redirect_input_flag)
                {
                    current_job->file_descriptors[current_job->row][0] = open(token, O_RDONLY);
                    if (current_job->file_descriptors[current_job->row][0] == -1)
                    {
                        perror("no directory");
                        current_job->next_command_flag = 1;
                    }

                    redirect_input_flag = 0;
                }
                else if (redirect_output_flag)
                {
                    current_job->file_descriptors[current_job->row][1] = open(token, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
                    if (current_job->file_descriptors[current_job->row][1] == -1)
                    {
                        perror("no directory");
                        current_job->next_command_flag = 1;
                    }
                    redirect_output_flag = 0;
                }
                else if (redirect_error_flag)
                {
                    current_job->file_descriptors[current_job->row][2] = open(token, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
                    if (current_job->file_descriptors[current_job->row][2] == -1)
                    {
                        perror("no directory");
                        current_job->next_command_flag = 1;
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
        strcat(current_job->input, token);
        strcat(current_job->input, " ");
    }
    current_job->row++;
    free(token);
}

void execute_command(Job *current_job, int row_param)
{

    // Dup2 for the file redirects
    if (current_job->file_descriptors[row_param][0] != -1)
    {
        if (dup2(current_job->file_descriptors[row_param][0], STDIN_FILENO) == -1)
        {
            perror("Dup2 STDIN error\n");
            exit(1);
        }
    }
    if (current_job->file_descriptors[row_param][1] != -1)
    {
        if (dup2(current_job->file_descriptors[row_param][1], STDOUT_FILENO) == -1)
        {
            perror("Dup2 STDOUT error\n");
            exit(1);
        }
    }
    if (current_job->file_descriptors[row_param][2] != -1)
    {
        if (dup2(current_job->file_descriptors[row_param][2], STDERR_FILENO) == -1)
        {
            perror("Dup2 STDERR error\n");
            exit(1);
        }
    }

    // Execute command
    execvp(current_job->args[row_param][0], current_job->args[row_param]);

    // Error handling if child process doesn't terminate (Should remove for final submission)
    // perror("execvp error\n");
    exit(1);
}

void single_command(Job *current_job)
{
    // Create a child process
    pid_t cpid = fork();
    if (cpid == 0)
    {
        // Set the signal to default
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        execute_command(current_job, 0);
    }
    else if (cpid > 0)
    {
        current_job->job_pid = cpid;
        // Wait for child process to finish running
        int status;

        // Determine what to do depending on background or foreground
        if (!current_job->background_flag)
        {
            waitpid(current_job->job_pid, &status, WUNTRACED);
        }
    }
    else
    {
        // Fork error
        perror("fork error\n");
    }
}

void pipe_command(Job *current_job)
{
    // Create pipe
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1)
    {
        perror("pipe");
        exit(1);
    }

    // First fork
    // current_job->job_pid = fork();
    pid_t cpid1 = fork();
    if (cpid1 == 0)
    {
        // Child process

        // Set pgid
        if (setpgid(0, 0) < 0)
        {
            perror("setpgid");
            exit(1);
        }

        // Set the signal to default
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);

        // Close the read end of pipe
        close(pipe_fd[0]);

        // Place output into pipe only if nothing is overriding it
        if (current_job->file_descriptors[0][1] == -1 || current_job->file_descriptors[0][2] == -1)
        {
            dup2(pipe_fd[1], STDOUT_FILENO);
        }

        // Close the write end of pipe
        close(pipe_fd[1]);

        execute_command(current_job, 0);
        exit(1);
    }
    else if (cpid1 > 0)
    {
        // Close the read end of pipe
        close(pipe_fd[1]);

        current_job->job_pid = cpid1;

        // Another fork to run other side of pipe
        pid_t cpid2 = fork();
        if (cpid2 == 0)
        {
            // Set pgid
            if (setpgid(0, current_job->job_pid) < 0)
            {
                perror("setpgid");
                exit(1);
            }

            // Get input from pipe based on if pipe was set by command
            if (current_job->file_descriptors[1][0] == -1)
            {
                dup2(pipe_fd[0], STDIN_FILENO);
            }

            // Close pipe and execute command
            close(pipe_fd[0]);
            execute_command(current_job, 1);
            exit(1);
        }
        else if (cpid2 > 0)
        {
            // Close pipes and wait for child process to finish
            close(pipe_fd[0]);
            int status;
            // Determine what to do depending on background or foreground
            if (!current_job->background_flag)
            {
                waitpid(current_job->job_pid, &status, WUNTRACED);
            }
            else
            {
                waitpid(current_job->job_pid, &status, WNOHANG);
                if (WIFEXITED(status))
                {
                    current_job->job_completed = 1;
                }
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

void update_job_list(int del_idx)
{
    Job new_job1, new_job2;
    bg_job_list[del_idx] = new_job1;
    for (int i = del_idx; i < bg_job_idx - 1; i++)
    {
        bg_job_list[i] = bg_job_list[i + 1];
    }
    bg_job_list[bg_job_idx] = new_job2;
}

void check_completed_jobs()
{
    int changes = 0;
    for (int i = 0; i < bg_job_idx; i++)
    {
        int status;
        int result = waitpid(bg_job_list[i].job_pid, &status, WNOHANG);
        if (result == -1)
        {
            perror("completed jobs error");
        }
        else
        {
            if (WIFEXITED(status))
            {
                if (bg_job_list[i].job_order >= bg_job_idx)
                {
                    printf("[%d]+ Done \t\t %s\n", bg_job_list[i].job_order, bg_job_list[i].input);
                }
                else
                {
                    printf("[%d]- Done \t\t %s\n", bg_job_list[i].job_order, bg_job_list[i].input);
                }
                close_fd(bg_job_list[i].file_descriptors);
                update_job_list(i);
                changes++;
            }
        }
    }
    bg_job_idx -= changes;
    bg_job_num -= changes;
}
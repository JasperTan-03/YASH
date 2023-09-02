#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_INPUTS 2000
#define MAX_TOKENS 2000
#define MAX_ARG_CHARS 30

int main()
{
    pid_t child_pid;
    char *input = (char *)malloc((MAX_INPUTS) * sizeof(char));
    char *token = (char *)malloc((MAX_TOKENS) * sizeof(char));
    char *args[100];

    int i;
    int token_num;
    do
    {
        printf("#");
        // Parsing:
        fgets(input, MAX_INPUTS, stdin);
        input[strlen(input) - 1] = 0;
        token_num = 0;
        while (token = strtok_r(input, " ", &input))
        {
            // printf("token: %s\n", token);
            args[token_num] = (char *)malloc((strlen(token) * sizeof(char)));
            args[token_num] = token;
            token_num++;
        }
        args[token_num] = NULL;

        // Execute command:
        child_pid = fork();
        if (child_pid == 0)
        {
            if (execvp(args[0], args) == -1)
            {
                printf("Execvp Error\n");
            }
        }
        else if (child_pid > 0)
        {
            int status;
            waitpid(child_pid, &status, 0);
            // if (WIFEXITED(status)){
            //     for(i = 0; i < token_num; i++){
            //         free(args[i]);
            //     }
            // } else {
            //     printf("Child did not exit\n");
            // }
            // printf("Parent\n");
        }
        else
        {
            printf("error\n");
        }
    } while (strcmp(args[0], "exit") != 0);

    // free(input);
    // free(token);
    // for (i = 0; i < MAX_ARG; i++){
    //     free(args[i]);
    // }
    // return 0;
}

int test()
{
    pid_t child_pid;

    char q[2000];
    char a[2000];
    fgets(q, sizeof(q), stdin);
    fgets(a, sizeof(a), stdin);
    int result;

    child_pid = fork();
    if (child_pid == 0)
    {
        printf("Child process: My PID is %d\n", getpid());
        if (strcmp(q, a) == 0)
        {
            printf("Success\n");
            char *args[] = {"cat", "README.md", NULL};
            result = execvp("cat", args);
        }
        else
        {
            printf("Fail\n");
        }
    }
    else if (child_pid > 0)
    {
        printf("Parent process: My child's PID is %d\n", getpid());
    }
    else
    {
        printf("Fork failed!\n");
    }
    printf("These are the results: %d\n", result);
    return 0;
}

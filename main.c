#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(){
    pid_t child_pid;
    char* input = (char*)malloc(2000*sizeof(int));
    char* token = (char*)malloc(2000*sizeof(int));
    do{
        printf("#");
        fgets(input, sizeof(input), stdin);
        token = strtok(input, " ");
        while(token != NULL){
            printf("token: %s\n", token);
            token = strtok(NULL, " ");
        }
    } while (strcmp(input, "exit\n") != 0);
    free(input);
    free(token);
    return 0;
}

int test(){
    pid_t child_pid;

    char q[2000];
    char a[2000];
    fgets(q, sizeof(q), stdin);
    fgets(a, sizeof(a), stdin);
    int result;
 
    child_pid = fork();
    if (child_pid == 0){
        printf("Child process: My PID is %d\n", getpid());
        if(strcmp(q,a) == 0){
            printf("Success\n");
            char *args[] = {"cat", "README.md", NULL};
            result = execvp("cat", args);
        } else {
            printf("Fail\n");
        }
    } else if (child_pid > 0){
        printf("Parent process: My child's PID is %d\n", getpid());
    } else {
        printf("Fork failed!\n");
    }
    printf("These are the results: %d\n", result);
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main() {
    pid_t pid;
    int i;

    for (i = 0; i < 10; i++) {
        pid = fork();

        if (pid < 0) {
            // Fork failed
            perror("Fork failed");
            return(1);
        } else if (pid == 0) {
            // Child process
            printf("I'm the child number %d (pid %d)\n", i, getpid());
	    sleep(3);
            return(0); // End child
        }
        // Parent continues loop to create next child
    }

    // Parent waits for all 10 children
    for (i = 0; i < 10; i++) {
        wait(NULL);
    }

    printf("Parent terminates (pid %d)\n", getpid());
    return 0;
}

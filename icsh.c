#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <setjmp.h>

#define BUFFERSIZE 1024
#define TOKEN_BUFFERSIZE 64
#define TOKEN_DELIM " \t\r\n\a"

pid_t pgid;
pid_t pid;

static jmp_buf env;
static volatile sig_atomic_t jump_active = 0;

void sigint_handler(int signo) {
    if (!jump_active) {
        return;
    }
    siglongjmp(env, 42);
}
char *input(void) {

    int bufferSize = BUFFERSIZE;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufferSize);
    int c;

    while (1) {
        // Read a character
        c = getchar();

        if (c == EOF) {
            exit(EXIT_SUCCESS);
        } else if (c == '\n') {
            buffer[position] = '\0';
            return buffer;
        } else {
            buffer[position] = c;
        }
        position++;
    }
}

char **arguments(char *line) {
    int bufferSize = TOKEN_BUFFERSIZE, position = 0;
    char **tokens = malloc(bufferSize * sizeof(char *));
    char *token;

    if (line[0] == '\0') {
        tokens[position] = "\0";
    } else {
        token = strtok(line, TOKEN_DELIM);
        while (token != NULL) {
            tokens[position] = token;
            position++;
            token = strtok(NULL, TOKEN_DELIM);
        }
        tokens[position] = NULL;
        return tokens;
    }
}

int builtInCommands(char **args) {
    if (strcmp("exit", args[0]) == 0) {
        exit(EXIT_SUCCESS);
        return 1;
    }
    else {
//        printf("%s",args[0]);
    }
    return 0;
}

void launcher(char **args) {
    pid = fork();
    if (pid == 0) {
        signal(SIGINT,SIG_DFL);
//        execvp(args[0],args);
        for (int i = 0; i < 1000000000; ++i) {

        }
    }
    else {
        wait(NULL);
        printf("%d\n",pid);
    }
}

void execute(char **args) {
    if ((builtInCommands(args)) == 0) {
        launcher(args);
    }
}


#pragma clang diagnostic push

#pragma clang diagnostic ignored "-Wmissing-noreturn"
void shellPrompt() {
    char *line;
    char **args;
    int status;

    pgid = getpid();

//        struct sigaction as;
    //    signal(SIGINT, SIG_IGN);
//        as.sa_flags = SA_RESTART;
//        sigaction(SIGINT,&as,NULL);

    struct sigaction s;
    s.sa_handler = sigint_handler;
    sigemptyset(&s.sa_mask);
    s.sa_flags = SA_RESTART;
    sigaction(SIGINT, &s, NULL);

    do {

        if (sigsetjmp(env, 1) == 42) {
            printf("\n");
        }
        jump_active = 1;
        printf("icsh> ");
        line = input();
        args = arguments(line);
        execute(args);

//        status = lsh_execute(args);

        free(line);
        free(args);

    } while (1);
}
#pragma clang diagnostic pop

void signalHandler(){
    shellPrompt();
}

int main() {
    printf("Hello");
    signal(SIGINT, signalHandler);
    shellPrompt();
    return EXIT_SUCCESS;
}

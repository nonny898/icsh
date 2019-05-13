#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <setjmp.h>
#include <fcntl.h>

#define BUFFER_SIZE 2048
#define TOKEN_BUFFER 64
#define TOKEN_DELIMITERS " \t\r\n\a"

static sigjmp_buf env;
static volatile sig_atomic_t jump_active = 0;
int exit_status;
int in = 0, out = 0;

void sigint_handler() {
    if (!jump_active) {
        return;
    }
    siglongjmp(env, 115);
}

void signalHandler_child() {
    pid_t pid;
    int terminationStatus;
    pid = waitpid(WAIT_ANY, &terminationStatus, WUNTRACED | WNOHANG);
    if (pid > 0) {
        if (WIFEXITED(terminationStatus)) {
            exit_status = terminationStatus + 128;
        } else if (WIFSIGNALED(terminationStatus)) {
            exit_status = terminationStatus + 128;
        } else if (WIFSTOPPED(terminationStatus)) {
            exit_status = terminationStatus + 128;
        } else {
            exit_status = terminationStatus + 128;
        }
    }
}

char *input(void) {

    int bufferSize = BUFFER_SIZE;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufferSize);
    int c;

    while (1) {
        c = getchar();
        if (c == '<') {
            in = 1;
        }
        if (c == '>') {
            out = 1;
        }
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

    int bufferSize = TOKEN_BUFFER, position = 0;
    char **tokens = malloc(bufferSize * sizeof(char *));
    char *token;

    if (line[0] == '\0') {
        tokens[position] = "\0";
    } else {
        token = strtok(line, TOKEN_DELIMITERS);
        while (token != NULL) {
            tokens[position] = token;
            position++;
            token = strtok(NULL, TOKEN_DELIMITERS);
        }
        tokens[position] = NULL;
    }
    return tokens;
}

/*
 *  Print out correct exit status with “echo $?”
 *  “Exit” command works.
 *
 */
int builtInCommands(char **args) {

    if ((strcmp("Exit", args[0]) == 0) || (strcmp("exit", args[0]) == 0)) {
        exit(EXIT_SUCCESS);
    }
    if ((strcmp("echo", args[0]) == 0) && (strcmp("$?", args[1]) == 0)) {
        printf("%d\n", exit_status);
        exit_status = 0;
        return 1;
    } else {
        return 0;
    }
}

/*
 * Correctly execute a foreground program.
 * The program starts, runs to completion and return to the prompt.
 * Support executing a program with arguments
 */
void launcher(char **args) {

    pid_t pid;
    pid = fork();
    int status;


    if (pid < 0) {
        printf("Can't create child process\n");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {

        signal(SIGQUIT, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGINT, SIG_DFL);
        signal(SIGCHLD, &signalHandler_child);

        /*
         *  Set the STDIN/STDOUT channels of the new process.
         */
        int commandDescriptor;
        // open file for read only (it's STDIN)
        if (in) {
            commandDescriptor = open(args[2], O_RDONLY, 0600);
            dup2(commandDescriptor, STDIN_FILENO);
            close(commandDescriptor);
        }
        // open (create) the file truncating it at 0, for write only
        if (out) {
            commandDescriptor = open(args[2], O_CREAT | O_TRUNC | O_WRONLY,0600);
            dup2(commandDescriptor, STDOUT_FILENO);
            close(commandDescriptor);
        }
        /*
         * The command is an error
         * if the executable file named by the first string does not exist,
         * or is not an executable.
         */
        if (execvp(args[0], args) < 0) {
            perror("");
            exit(EXIT_FAILURE);
        } else {
            execvp(args[0], args);
            exit(EXIT_SUCCESS);
        }


    } else {
        in = 0;
        out = 0;
        while (wait(&status) != -1);
    }
}

void execute(char **args) {
    if ((builtInCommands(args)) == 0) {
        launcher(args);
    }
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"

/*
 * Compiled properly without any warning or error.
 * A prompt, “icsh>” , is display correctly after being run.
 * Empty command does nothing.
 * Ctrl+C does not exit the shell.
 */
void shellPrompt() {
    char *line;
    char **args;

    signal(SIGQUIT, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, &sigint_handler);
    signal(SIGINT,  &sigint_handler);
    signal(SIGCHLD, &signalHandler_child);

    do {

        if (sigsetjmp(env, 1) == 115) {
            printf("\n");
        }

        jump_active = 1;
        printf("icsh > ");
        line = input();
        args = arguments(line);
        execute(args);
        printf("in = %d, out = %d\n",in,out);
        free(line);
        free(args);

    } while (1);
}

#pragma clang diagnostic pop

int main() {
    shellPrompt();
    return EXIT_SUCCESS;
}

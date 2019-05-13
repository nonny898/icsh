#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <setjmp.h>
#include <fcntl.h>
#include <termios.h>

#define TRUE 1
#define FALSE !TRUE
#define BUFFER_SIZE 4096
#define TOKEN_BUFFER 2048
#define TOKEN_DELIMITERS " \t\r\n\a\'%'"
#define FOREGROUND 'F'
#define BACKGROUND 'B'
#define SUSPENDED 'S'
#define WAITING_INPUT 'W'
#define BY_PROCESS_ID 1
#define BY_JOB_ID 2
#define BY_JOB_STATUS 3

typedef struct job {
    int id;
    char *name;
    pid_t pid;
    pid_t pgid;
    int status;
    struct job *next;
} job;

pid_t shell_pid;
pid_t shell_pgid;
int shell_terminal;
int shell_is_interactive;
struct termios shell_tmodes;
static sigjmp_buf env;
static volatile sig_atomic_t jump_active = 0;
int exit_status;
int in = 0, out = 0, background_execution = 0;
static job* jobsList = NULL;
static job* head = NULL;
static int numActiveJobs = 0;

void sigint_handler() {
    if (!jump_active) {
        return;
    }
    siglongjmp(env, 115);
}

char *input(void) {
    int bufferSize = BUFFER_SIZE;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufferSize);
    int c = 0;
    while (1) {
        c = getchar();
        if (c == '<') {
            in = 1;
        }
        if (c == '>') {
            out = 1;
        }
        if (c == '&') {
            background_execution = 1;
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

job* insertJob(pid_t pid, pid_t pgid, char* name, int status) {
    usleep(10000);
    job *newJob = malloc(2048 * sizeof(job));

    newJob->name = (char *) malloc(2048 * sizeof(name));
    newJob->name = strcpy(newJob->name, name);
    newJob->pid = pid;
    newJob->pgid = pgid;
    newJob->status = status;
    newJob->next = NULL;

    if (jobsList == NULL) {
        numActiveJobs++;
        newJob->id = numActiveJobs;
        head = newJob;
        return newJob;
    } else {
        job *auxNode = jobsList;
        while (auxNode->next != NULL) {
            auxNode = auxNode->next;
        }
        newJob->id = auxNode->id + 1;
        auxNode->next = newJob;
        numActiveJobs++;
        return auxNode;
    }
}

void printJobs() {
    printf("Active jobs:\n");
    printf("--------------------------------------------------------------\n");
    printf("| %7s  | %30s | %5s | %6s |\n", "job no.", "name", "pid", "status");
    printf("--------------------------------------------------------------\n");
    job *job = jobsList;
    if (job == NULL) {
        printf("| %s %49s |\n", "No Jobs.", "");
    } else {
        while (job != NULL) {
            printf("|  %7d | %30s | %5d | %6c |\n", job->id, job->name, job->pid, job->status);
            job = job->next;
        }
    }
    printf("--------------------------------------------------------------\n");
}

job* getJob(int searchValue, int searchParameter) {
    usleep(10000);
    job *job = jobsList;
    while (job != NULL) {
        if (searchParameter == BY_PROCESS_ID && job->pid == searchValue) {
            return job;
        } else if (searchParameter == BY_JOB_ID && job->id == searchValue) {
            return job;
        } else if (searchParameter == BY_JOB_STATUS && job->status == searchValue) {
            return job;
        } else
            job = job->next;
    }
    return NULL;
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCDFAInspection"
#pragma ide diagnostic ignored "UnusedValue"

job* delJob(job* job) {
    usleep(10000);
    struct job *currentJob = head;
    struct job *previousJob = NULL;
    currentJob = jobsList;
    if (head == NULL) {
        return NULL;
    }
    while (currentJob->pid != job->pid) {
        if (currentJob->next == NULL) {
            return NULL;
        } else {
            previousJob = currentJob;
            currentJob = currentJob->next;
        }
    }
    if (currentJob == head) {
        currentJob = head->next;

    } else {
        previousJob->next = currentJob->next;
    }
    return currentJob;
}
#pragma clang diagnostic pop

void waitJob(job* job) {
    int terminationStatus;
    while (waitpid(job->pid, &terminationStatus, WNOHANG) == 0) {
        if (job->status == SUSPENDED)
            return;
    }
    jobsList = delJob(job);
}

void putJobForeground(job* job, int continueJob) {
    job->status = FOREGROUND;
    tcsetpgrp(shell_terminal, job->pgid);
    if (continueJob) {
        if (kill(-job->pgid, SIGCONT) < 0)
            perror("kill (SIGCONT)");
    }
    waitJob(job);
    tcsetpgrp(shell_terminal, shell_pgid);
}

void putJobBackground(job* job, int continueJob) {
    if (job == NULL)
        return;
    if (continueJob && job->status != WAITING_INPUT)
        job->status = WAITING_INPUT;
    if (continueJob)
        if (kill(-job->pgid, SIGCONT) < 0)
            perror("kill (SIGCONT)");
    tcsetpgrp(shell_terminal, shell_pgid);
}

int changeJobStatus(int pid, int status) {
    usleep(10000);
    job *job = jobsList;
    if (job == NULL) {
        return 0;
    } else {
        int counter = 0;
        while (job != NULL) {
            if (job->pid == pid) {
                job->status = status;
                return TRUE;
            }
            counter++;
            job = job->next;
        }
        return FALSE;
    }
}

void signalHandler_child() {
    pid_t pid;
    int terminationStatus;
    pid = waitpid(WAIT_ANY, &terminationStatus, WUNTRACED | WNOHANG);
    if (pid > 0) {
        job *job = getJob(pid, BY_PROCESS_ID);
        if (job == NULL)
            return;
        if (WIFEXITED(terminationStatus)) {
            if (job->status == BACKGROUND) {
                printf("\n[%d]+  Done\t   %s\n", job->id, job->name);
                jobsList = delJob(job);
            }
            exit_status = terminationStatus + 128;
        } else if (WIFSIGNALED(terminationStatus)) {
            printf("\n[%d]+  KILLED\t   %s\n", job->id, job->name);
            jobsList = delJob(job);
            exit_status = terminationStatus + 128;
        } else if (WIFSTOPPED(terminationStatus)) {
            if (job->status == BACKGROUND) {
                tcsetpgrp(shell_terminal, shell_pgid);
                changeJobStatus(pid, WAITING_INPUT);
                printf("\n[%d]+   suspended [wants input]\t   %s\n",
                       numActiveJobs, job->name);
            } else {
                tcsetpgrp(shell_terminal, job->pgid);
                changeJobStatus(pid, SUSPENDED);
                printf("\n[%d]+   stopped\t   %s\n", numActiveJobs, job->name);
            }
            exit_status = terminationStatus + 128;
            return;
        } else {
            if (job->status == BACKGROUND) {
                jobsList = delJob(job);
            }
            exit_status = terminationStatus + 128;
        }
        tcsetpgrp(shell_terminal, shell_pgid);
    }
}

/*
 *  Set the STDIN/STDOUT channels of the new process.
 *  In the case of <, the input of the program will be redirected from the specified file name.
 *  In the case of >, the output of the program will be redirected to the specified file name.
 *  If the output file does not exist, it should be created.
 *  If the input file does not exist, this is an error.
 */
void executeCommand(char **args) {
    int commandDescriptor;
    if (in) {
        if ((commandDescriptor = open(args[2], O_RDONLY, 0600)) < 0) {
            perror("Couldn't open input file");
            exit(EXIT_FAILURE);
        }
        dup2(commandDescriptor, STDIN_FILENO);
        close(commandDescriptor);
    }
    if (out) {
        commandDescriptor = open(args[2], O_CREAT | O_TRUNC | O_WRONLY, 0600);
        dup2(commandDescriptor, STDOUT_FILENO);
        close(commandDescriptor);
    }
    /*
     * The command is an error
     * if the executable file named by the first string does not exist,
     * or is not an executable.
     */
    if (execvp(args[0], args) < 0) {
        perror("Execution Failed: ");
        exit(EXIT_FAILURE);
    }
}

/*
 * Correctly userCommands a foreground program.
 * The program starts, runs to completion and return to the prompt.
 * Support executing a program with arguments
 */
void launcher(char **args, int executionMode) {
    pid_t pid;
    pid = fork();
    if (pid < 0) {
        printf("Can't create child process\n");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGINT, SIG_DFL);
        signal(SIGCHLD, &signalHandler_child);
        setpgrp();
        if (executionMode == FOREGROUND) {
            tcsetpgrp(shell_terminal, getpid());
        }
        if (executionMode == BACKGROUND) {
            printf("[%d] %d\n", ++numActiveJobs, getpid());
        }
        executeCommand(args);
    } else {
        in = 0;
        out = 0;
        background_execution = 0;
        setpgid(pid, pid);
        jobsList = insertJob(pid, pid, args[0], executionMode);
        job *job = getJob(pid, BY_PROCESS_ID);
        if (executionMode == FOREGROUND) {
            putJobForeground(job, FALSE);
        }
        if (executionMode == BACKGROUND) {
            putJobBackground(job, FALSE);
        }
    }
}

/*
 * exit: Kill all child processes and exit icsh with a meaningful return code.
 * echo $?: Prints the exit status of the most recent foreground child process to have exited. Return 0 if no such child has existed.
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
    }
    if (background_execution > 0) {
        launcher(args, BACKGROUND);
        return 1;
    }
    if (strcmp("bg", args[0]) == 0) {
        if (args[1] == NULL) {
            printf("Please ID the process\n");
            return 1;
        } else {
            int jobID = strtol(args[1], NULL, 10);
            job *job = getJob(jobID, BY_JOB_ID);
            if (job == NULL) {
                printf("Invalid process ID\n");
                return 1;
            } else {
                putJobBackground(job, TRUE);
                return 1;
            }
        }
    }
    if (strcmp("fg", args[0]) == 0) {
        if (args[1] == NULL) {
            printf("Please ID the process\n");
            return 1;
        }
        int jobID = strtol(args[1], NULL, 10);
        job *job = getJob(jobID, BY_JOB_ID);
        if (job == NULL) {
            printf("Invalid process ID\n");
            return 1;
        } else if (job->status == SUSPENDED || job->status == WAITING_INPUT) {
            putJobForeground(job, TRUE);
            return 1;
        } else {
            putJobForeground(job, FALSE);
            return 1;
        }
    }
    if (strcmp("jobs", args[0]) == 0) {
        printJobs();
        return 1;
    }
    return 0;
}

void userCommands(char **args) {
    if ((builtInCommands(args)) == 0) {
        launcher(args, FOREGROUND);
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
    do {
        if (sigsetjmp(env, 1) == 115) {
            printf("\n");
        }
        jump_active = 1;
        printf("icsh > ");
        line = input();
        args = arguments(line);
        userCommands(args);
        free(line);
        free(args);
    } while (1);
}

#pragma clang diagnostic pop

/*
 * Keep track of attributes of the shell.
 * Make sure the shell is running interactively as the foreground job before proceeding.
 * See if we are running interactively.
 * Loop until we are in the foreground.
 * Ignore interactive and job-control signals.
 * Put ourselves in our own process group.
 * Grab control of the terminal.
 * Save default terminal attributes for shell.
 */
void init_shell () {
    shell_pid = getpid();
    shell_terminal = STDIN_FILENO;
    shell_is_interactive = isatty(shell_terminal);
    if (shell_is_interactive) {
        while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp())) {
            kill(shell_pgid, SIGTTIN);
        }
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTSTP, &sigint_handler);
        signal(SIGINT, &sigint_handler);
        signal(SIGCHLD, &signalHandler_child);
        setpgid(shell_pid, shell_pgid);
        shell_pgid = getpgrp();
        if (shell_pid != shell_pgid) {
            perror("Error, the shell is not process group leader");
            exit(EXIT_FAILURE);
        }
        tcsetpgrp(shell_terminal, shell_pgid);
        tcgetattr(shell_terminal, &shell_tmodes);
    } else {
        perror("Could not make shell interactive. Exiting..\n");
        exit(EXIT_FAILURE);
    }
}

int main() {
    init_shell();
    shellPrompt();
    return EXIT_SUCCESS;
}

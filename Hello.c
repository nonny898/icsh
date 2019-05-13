#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <setjmp.h>

int main() {
    printf("HELLO!\n");
    usleep(2000000);
}

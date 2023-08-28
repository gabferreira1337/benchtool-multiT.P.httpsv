
#ifndef PROJETO_SO_THREADS_THREAD_FUNCTIONS_H
#define PROJETO_SO_THREADS_THREAD_FUNCTIONS_H


#include <stdio.h>
#include <stdlib.h>

///  Delete "benchmark_thread.txt" file history
void deleteFile() {
    if (remove("benchmark_thread.txt") == 0) {
        // printf("File deleted\n");
    }
}

int pexit(char * msg)
{
    perror(msg);
    exit(1);
}

#endif

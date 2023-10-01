#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <ctype.h>
#include <signal.h>
#include <sys/shm.h>
#include "functions.h"
#include "processes_structs.h"

#define LINESIZE 40
#define BUFSIZE 1024
#define TIMER_START() gettimeofday(&tv1, NULL)
#define TIMER_STOP() \
gettimeofday(&tv2, NULL);    \
timersub(&tv2, &tv1, &tv);   \
time_delta = (float)tv.tv_sec + tv.tv_usec / 1000000.0



void execRparent();             //parent process reads from the pipe and print the data into a file named benchmarkparent.txt
void printResult(int num_lines);
void execResult(char *buff);
void freeMem();
void getDatafile(int number_lines);
int dataReader();



struct Data *data ;                   /// array of struct to save all data from each child process (pid,bsn, rsn, rc, t)
pid_t *child_pid;
int fds[2];

int N, batch_number, request_number;

int sockfd, file_fd , parentFile_size;


/// signal handler for SIGINT , this handler terminates every child process and present the execution statistics collected so far about the time of eah request
void grcShtdwn(int sig){

    close(fds[1]);
    close(file_fd);                 //close fd from the child process that was interrupted
    close(sockfd);                  //close socket from the child process that was interrupted

   // printf("I won't die\n");

   // SEND A SIGTERM TO EVERY CHILD PROCESS RUNNING //
    for (int i = 0; i < batch_number; ++i) {
        if(child_pid[i] > 0){
            kill(child_pid[i], SIGTERM);
        }

    }

    int  wait_n, lines_number;

    // wait for all child process to terminate if they don't the parent process sends a SIGKILL (if waitpid return < 0)

    for (int i = 0; i < batch_number; ++i) {
        if (child_pid[i] > 0) {
            if ((wait_n = waitpid(child_pid[i], NULL, 0)) > 0) {
               // printf("Child %d terminated\n", child_pid[i]);
            } else
                kill(child_pid[i], SIGKILL);
        }
    }

    // EXECUTION STATISTICS //

    lines_number = dataReader();    // read data to the struct and get the number of lines
    getDatafile(lines_number);
    printResult(lines_number);

    freeMem();

    exit (0);
}


int main(int argc, char *argv[]) {

    int result,n;
    char buffer[BUFSIZE], rcBuffer[BUFSIZE];
    char *token;
    static struct sockaddr_in serv_addr;
    struct timeval tv1, tv2, tv;
    float time_delta;


    // if missing arguments from command line
    if(argc < 5 || argc > 5){
        printf("Usage: ./client <SERVER IP ADDRESS> <LISTENING PORT> <N HTTP REQUESTS> <BATCHES>\n");
        printf("Example: ./client 127.0.0.1 8141 1000 10\n");
        exit(1);
    }

    if (argc == 5) {
        printf("client trying to connect to IP = %s PORT = %s\n", argv[1], argv[2]);
        sprintf(buffer, "GET /index.html HTTP/1.0 \r\n\r\n");
        /* Note: spaces are delimiters and VERY important */

    } else {
        printf("client trying to connect to IP = %s PORT = %s retrieving FILE= %s\n", argv[1], argv[2],
               argv[3]);
        sprintf(buffer, "GET /%s HTTP/1.0 \r\n\r\n", argv[3]);
        /* Note: spaces are delimiters and VERY important */
    }



     N = atoi(argv[3]);                      //number of requests //
     batch_number = atoi(argv[4]);          // number of batches //

     request_number = (N / batch_number);   // number of times that we need to send each batch //


    /* CHECKS INVALID ARGUMENTS FROM TERMINAL */

     if(N == 0 || batch_number == 0){           //atoi returns 0 if argument is not an integer
         pexit("argument of invalid type (must be int)");
     }



    //array with  the pids from child processes created so the parent process waits for each batch //
     child_pid = (pid_t *) malloc(batch_number * sizeof(pid_t));

     if (child_pid < 0){                        // checks if malloc fails
         pexit("child_pid malloc");
     }


     if(pipe(fds) < 0){                               // checks if pipe fails
        pexit("pipe");
    }


    deleteFile();                           // delete past data from child file

    struct sigaction sa;
    sa.sa_handler = &grcShtdwn;         //function pointer to the signal handler
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);           //using sigaction so it runs in every unix environment

    printf("\n\tSTARTING BENCHMARK...\t\n");

    TIMER_START();

    char fBuffer[BUFSIZE];

    for (int i = 0; i < request_number; ++i) {

        for (int k = 0; k < batch_number; ++k) {  /* BATCHES */


            if ((child_pid[k] = fork()) < 0) {
                pexit("fork");
            }

            if (child_pid[k] == 0) {     /* CHILD PROCESS */

                TIMER_START();

                if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                    pexit("socket() failed");

                serv_addr.sin_family = AF_INET;
                serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
                serv_addr.sin_port = htons(atoi(argv[2]));

                if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
                    pexit("connect() failed");


                /* Now the sockfd can be used to communicate to the server the GET request */
                printf("Connection established Send bytes=%d %s pid = %d\n", (int) strlen(buffer), buffer, getpid());
                write(sockfd, buffer, strlen(buffer));

                /* This displays the raw HTML file (if index.html) as received by the browser*/
                 while ((n = read(sockfd, buffer, BUFSIZE)) > 0) {

                     //write(1, buffer, n);
                 }

                close(sockfd);              // close connection

               /* char *http_start = NULL;

                 http_start =  strstr(buffer, "HTTP/1.1");      //point after HTTPS....

                 char *code_start = http_start + 9;             // pointer to where the rc starts
                 char *code_end = strchr(code_start,' ');       // pointer where the rc code ends
                 char respcode[4];

                memcpy(respcode, code_start, 3);                // save to respcode
                respcode[3] = '\0';

                int rc = atoi(respcode);

                printf("%d",  rc);*/

                int rc = 200;

                TIMER_STOP();

                // Copies the string representation to fbuffer //
                sprintf(fBuffer,"%d;%d;%d;%d;%f\n", getpid(), k+1, i+1, rc,time_delta);
                // printf(" CHILD PROCESS BUFFER : %s\n",fBuffer);

                execResult(fBuffer);

                // CHILD PROCESS WRITES TO THE WRITE END OF PIPE THE EXECUTION RESULT  //

                close(fds[0]);
                writen(fds[1], fBuffer, strlen(fBuffer));
                close(fds[1]);

                exit(0);

            }

        }

        // waits for all child processes (waits for each batch) //
        for (int j = 0; j < batch_number; ++j) {

            waitpid(child_pid[j], NULL, 0);

        }

    }

    TIMER_STOP();
    // PARENT PROCESS //

    close(fds[1]);                    // Close pipe write end descriptor

    // First approach , parent process reads from the pipe , put the data into an array of structs , writes to a file //
    // and print some statistics                                                                                      //
    execRparent();
    printResult(N);

    // Second approach , parent process while reading the pipe writes to the file and after writing to the file     //
    // puts the data into an array of structs and prints some statistics                                            //

     //int line_n=0;
     //line_n = dataReader();
     //getDatafile(line_n);
     // printResult(line_n);


    //fprintf(stdout, "%f secs\n", time_delta);

    freeMem();

}



/// Reads the data from the pipe and stores that data in a array of structs and after that saves that data into "benchmarkparent.txt" file

void execRparent(){

    int fd2, i ,total_size;
    char *end_str;
    char *token;

    total_size = N * LINESIZE;      // total size of all lines

    char tbuff[total_size], buff[total_size];

    data = (struct Data*) malloc(N * sizeof(struct Data));

    if (data == NULL){
        pexit("execRparent memory allocation");
    }

    readn(fds[0], buff, N * LINESIZE);
    strcpy(tbuff, buff);

    // strtok_r allows the processing of multiple token sequences simultaneously ,
    // and the &end_str pointer maintains the state of the character sequence being parse

    // Split each line into tokens based on "\n" using strtok_r
    // and store into the array of struct after splitting each line with the delimiter ";"

    token= strtok_r(tbuff, "\n", &end_str);

    for(int j = 0; token != NULL; j++) {
        i = 0;
        char *end_token;
        char *token2 = strtok_r(token, ";", &end_token);

        while (token2 != NULL) {

            switch (i) {

                case 0:
                    data[j].child_pid = atoi(token2);

                    break;

                case 1:
                    data[j].bsn = atoi(token2);

                    break;
                case 2:
                    data[j].rsn = atoi(token2);

                    break;
                case 3:
                    data[j].rc = atoi(token2);

                    break;
                case 4:
                    data[j].time = atof(token2);

                    break;
                default:
                    break;

        }
            token2 = strtok_r(NULL, ";", &end_token);
            i++;
    }
        token= strtok_r(NULL, "\n", &end_str);  // point to the next line
    }

    // WRITE TO A FILE THE DATA READ FROM THE PIPE

    fd2 = open("benchmarkparent.txt", O_WRONLY | O_CREAT | O_TRUNC,
              0644);              //descritor ficheiro , abrir ficheiro com permissoes 644

    if (fd2 < 0) {
        pexit("fd2");
    }

    writen(fd2, buff, strlen(buff));

    close(fd2);
    close(fds[0]);

}

/// Function to calculate and print the time statistics
void printResult(int num_lines) {

    float tmin, tmax, tmed, ttotal = 0.0;

    tmax = data[0].time;
    tmin = data[0].time;
    for (int l = 0; l < num_lines; ++l) {

        if (data[l].time < tmin) {

            tmin = data[l].time;

        }
        if(data[l].time > tmax){
            tmax = data[l].time;
        }

        ttotal += data[l].time;

      /*  printf("%d;", data[l].child_pid);
        printf("%d;", data[l].bsn);
        printf("%d;", data[l].rsn);
        printf("%d;", data[l].rc);
        printf("%f\n", data[l].time);*/
    }
    tmed = (ttotal / (float) num_lines);

    printf("\n\t\tSTATISTICS COLLECTED\t\t\n");
    printf("time total: %f seconds\ntime mean: %f seconds\ntime min %f seconds\ntime max %f seconds\n", ttotal, tmed, tmin, tmax );

}

/// Function for child process save the line with the execution result into "benchmarkchild.txt" file
void execResult(char *buff) {

    file_fd = open("benchmarkchild.txt",  O_CREAT | O_WRONLY| O_APPEND, 0644);

    if (file_fd < 0) {
        pexit("execResult fd");
    }

    writen(file_fd, buff, strlen(buff));

    close(file_fd);

}

/// Save to "benchmarkparent.txt" file the data read from the pipe, calculates the size of that file
/// and  return the number of lines read from the pipe
int dataReader(){

    close(fds[1]);

    char buffer[BUFSIZE];
    int fd_3, n, number_lines=0;

    fd_3 = open("benchmarkparent.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if(fd_3 < 0){
        pexit("dataReader fd_3");
    }

    while ((n = (readn(fds[0], buffer, sizeof(buffer)))) > 0){
        write(fd_3, buffer, n);
        // COUNT NUMBER OF LINES TO STORE IN THE STRUCT//
        for (int i = 0; i < n; ++i) {
            if(buffer[i] == '\n'){
                number_lines++;
            }
        }
    }

    close(fds[0]);          // close read end pipe

    // get size of file with lseek , SEEK_END option sets offset to the size of the file (till EOF)

    parentFile_size = lseek(fd_3,0, SEEK_END);

    close(fd_3);

    return number_lines; // return number of lines written to the file

}


/// Reads "benchmarkparent.txt" file and stores the data from the file in a array of structs

void getDatafile(int number_lines){

    int i , fd_3;
    char *end_str;
    char *token;
    char buffer[parentFile_size];

    data = (struct Data*) malloc(number_lines * sizeof(struct Data));           // allocate memory to the array of structs

    if (data == NULL){
        pexit("getDatafile memory allocation");
    }

    fd_3 = open("benchmarkparent.txt", O_RDONLY);

    if(fd_3 < 0){
        pexit("Read fd_3 getDatafile");
    }

    readn( fd_3, buffer, sizeof(buffer));

        // strtok_r allows the processing of multiple token sequences simultaneously ,
        // and the &end_str pointer maintains the state of the character sequence being parse

        token = strtok_r(buffer, "\n", &end_str);

        // Split each line into tokens based on "\n" using strtok_r
        // and store into the array of struct after splitting each line with the delimiter ";"

    for (int j = 0;  token != NULL ; ++j) {          // index of the array of structs will be j

        i = 0;
        char *end_token;
        char *token_2 = strtok_r(token, ";", &end_token);

        while (token_2 != NULL) {

            switch (i) {

                case 0:
                    data[j].child_pid = atoi(token_2);

                    break;

                case 1:
                    data[j].bsn = atoi(token_2);

                    break;
                case 2:
                    data[j].rsn = atoi(token_2);

                    break;
                case 3:
                    data[j].rc = atoi(token_2);

                    break;
                case 4:
                    data[j].time = atof(token_2);
                    // printf("%f", data[j].time);

                    break;
                default:
                    break;

            }
            token_2 = strtok_r(NULL, ";", &end_token);  // point to the next ";" occurrence
            i++;
        }
        token = strtok_r(NULL, "\n", &end_str);     // point to the next line
    }

    close(fd_3);

}

/// free memory stored in the heap
void freeMem(){
    free(data);
    free(child_pid);
}






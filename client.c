#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>
#include "thread_structs.h"
#include "thread_functions.h"

#define BUFSIZE  8096
#define TIMER_START() gettimeofday(&tv1, NULL)
#define TIMER_STOP() \
gettimeofday(&tv2, NULL);    \
timersub(&tv2, &tv1, &tv);   \
time_delta = (float)tv.tv_sec + tv.tv_usec / 1000000.0



int total_requests , M , batch_number;
int request_number;     /// number of requests
pthread_t *tids;

char buffer[BUFSIZE];   /// data from file (HTML)

char client_ip[16];
int client_port;



void getDatafile();

pthread_mutex_t mutex_client = PTHREAD_MUTEX_INITIALIZER;




void *th_requests(void *params){

    struct R_DATA  *r_data = (struct R_DATA *) params;
    struct timeval tv1, tv2, tv;
    float time_delta;

    int n, fd;
    int sockfd;
    char readbuffer[BUFSIZE];

    static struct sockaddr_in serv_addr;

    TIMER_START();

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        pexit("socket() failed");

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(client_ip);
    serv_addr.sin_port = htons(client_port);

    /*Connect to the socket offered by the web server */
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        pexit("connect() failed");

  //  printf("%d\n", sockfd);

    /*Now the sockfd can be used to communicate to the server the GET request */
    printf("Send bytes=%d %s\n", (int) strlen(buffer), buffer);
    write(sockfd, buffer, strlen(buffer));


    char buff[BUFSIZE] = "";       // buffer to save data from sockerfd

   // printf("%d\n", sockfd);


    /*This displays the raw HTML file (if index.html) as received by the browser */
    while (( n = read(sockfd, readbuffer, BUFSIZE)) > 0){

      //  printf("%s\n", readbuffer);
        readbuffer[n] = '\0';
        strcat(buff, readbuffer);

        //write(1, readbuffer, n);
    }

    close(sockfd);



   // printf("%s\n", buff);

    char *http_start = NULL;

    http_start =  strstr(buff, "HTTP/1.1");      //point after HTTP....

    printf("%s", http_start);

    if(http_start == NULL){
        pexit("ERROR\n");
    }

    char *code_start = http_start + 9;             // pointer to where the rc starts
    char respcode[4];

    memcpy(respcode, code_start, 3);                // save to respcode
    respcode[3] = '\0';

    int rc = atoi(respcode);


   // int rc = 0;



    TIMER_STOP();

    char fbuffer[1024];

    //snprintf for preventing buffer overflow
    int bytes_written = snprintf(fbuffer,sizeof(fbuffer), "%lu;%d;%d;%d;%f\n", (unsigned long) pthread_self(),r_data->bsn +1,r_data->rsn+1, rc, time_delta);

    /// write to the file the request data
    fd = open("benchmark_thread.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);

    if(fd < 0){

        pexit("couldn't open the file");
    }

    int result;

    result = write(fd, fbuffer, bytes_written);

    if(result < 0){
        pexit("couldn't write to the file");

    }

    close(fd);

    free(r_data);

    pthread_exit(NULL);
}




int main(int argc, char *argv[]){

    struct timeval tv1, tv2, tv;
    float time_delta;

    pthread_attr_t attr;
    pthread_attr_init(&attr);


    deleteFile(); ///delete history from file

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
        pthread_mutex_lock(&mutex_client);
        sprintf(buffer, "GET /%s HTTP/1.0 \r\n\r\n", argv[3]);
        pthread_mutex_unlock(&mutex_client);
        /* Note: spaces are delimiters and VERY important */

    }

    strncpy(client_ip, argv[1], 16);
    client_port = atoi(argv[2]);
    total_requests = atoi(argv[3]);                      ///number of requests
    batch_number = atoi(argv[4]);                        /// number of batches

    request_number = (total_requests / batch_number);   /// number of times that we need to send each batch


    /* CHECKS INVALID ARGUMENTS FROM TERMINAL */

    if(total_requests == 0 || batch_number == 0){           //atoi returns 0 if argument is not an integer
        pexit("argument of invalid type (must be int)");
    }


    tids = (pthread_t *) malloc(batch_number * sizeof(pthread_t));

    if (tids < 0){                        // checks if malloc fails
        pexit("tids malloc");
    }


    printf("\n\tSTARTING BENCHMARK...\t\n");


    TIMER_START();


    for (int i = 0; i < request_number; ++i) {

        for (int j = 0; j < batch_number; ++j) {

            //create request_data pointer for each thread

            struct R_DATA *request_data = malloc(sizeof(struct R_DATA));


            if (request_data < 0){
                pexit("request_data malloc: ");
            }

            request_data->rsn = i;
            request_data->bsn = j;


            if( (pthread_create(&tids[j], NULL, &th_requests, (void*) request_data ))!= 0){
                pexit("error creating thread: ");
            }



        }

        for (int j = 0; j < batch_number; ++j) {
            pthread_join(tids[j],NULL);
        }

    }

    TIMER_STOP();

    fprintf(stderr, "%f secs\n", time_delta);

    getDatafile();

    free(tids);


    pthread_exit(NULL);
}

/// Reads "benchmark_thread.txt" file ,  calculate and print the time statistics

void getDatafile(){

    int i , fd_3;
    char *end_str;
    char *token;
    char getDatabuffer[BUFSIZE];

    struct FILE_DATA f_data[total_requests];

    float tmin, tmax, tmed , ttotal = 0.0;


    fd_3 = open("benchmark_thread.txt", O_RDONLY);

    if(fd_3 < 0){
        pexit("Read fd_3 getDatafile");
    }

    read( fd_3, getDatabuffer, sizeof(getDatabuffer));

    //printf("%s\n", getDatabuffer);

    // strtok_r allows the processing of multiple token sequences simultaneously ,
    // and the &end_str pointer maintains the state of the character sequence being parse

    token = strtok_r(getDatabuffer, "\n", &end_str);

    // Split each line into tokens based on "\n" using strtok_r
    // and store into the array of struct after splitting each line with the delimiter ";"

    for (int j = 0;  token != NULL ; ++j) {          // index of the array of structs will be j

        i = 0;
        char *end_token;
        char *token_2 = strtok_r(token, ";", &end_token);

        while (token_2 != NULL) {

            switch(i) {

                case 0:
                    f_data[j].thread_id = (pthread_t) atol(token_2);
                    //printf("%lu\n", f_data[j].thread_id);

                    break;

                case 1:
                    f_data[j].bsn = atoi(token_2);
                    //printf("%d\n", f_data[j].bsn);

                    break;
                case 2:
                    f_data[j].rsn = atoi(token_2);
                    //printf("%d\n", f_data[j].rsn);

                    break;
                case 3:
                    f_data[j].rc = atoi(token_2);
                    //printf("%d\n", f_data[j].rc);

                    break;
                case 4:
                    f_data[j].time = atof(token_2);
                    //printf("%f\n", f_data[j].time);

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

    tmin = f_data[0].time ;
    tmax = f_data[0].time;


    for (int l = 0; l < total_requests; ++l) {

        if (f_data[l].time < tmin) {

            tmin = f_data[l].time;

        }
        if(f_data[l].time > tmax){
            tmax = f_data[l].time;
        }

        ttotal += f_data[l].time;

    }
    tmed = (ttotal / (float) total_requests);

    printf("\n\t\tSTATISTICS COLLECTED\t\t\n");
    printf("time total: %f seconds\ntime mean: %f seconds\ntime min %f seconds\ntime max %f seconds\n", ttotal, tmed, tmin, tmax );


}




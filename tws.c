#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <dispatch/dispatch.h>
#include "thread_structs.h"
#include "thread_functions.h"

#define BUFSIZE 8191
#define ERROR   42
#define LOG 44
#define FORBIDDEN   403
#define NOTFOUND    404
#define VERSION 1
#define MAX_R 100
#define POOL_SIZE 10
#define SOCK_BUFF 5


void *th_fsm(void *arg);
void *fsm_consumidor();
void th_web(int fd, int sock_hit);





struct PROD_CONS socketfd_buffer;


struct {
    char *ext;
    char *filetype;
} extensions [] = {
        {"gif", "image/gif" },
        {"jpg", "image/jpg" },
        {"jpeg","image/jpeg"},
        {"png", "image/png" },
        {"ico", "image/ico" },
        {"zip", "image/zip" },
        {"gz",  "image/gz"  },
        {"tar", "image/tar" },
        {"htm", "text/html" },
        {"html","text/html" },
        {0,0} };



/* Deals with error messages and logs everything to disk */

void logger(int type, char *s1, char *s2, int socket_fd)
{
    int fd ;
    char logbuffer[BUFSIZE*2];

    //printf("%d\n", type);

    switch (type) {
        case ERROR: (void)sprintf(logbuffer,"ERROR: %s:%s Errno=%d exiting pid=%d",s1, s2, errno,getpid());
            break;
        case FORBIDDEN:

            (void)write(socket_fd, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n",271);
            (void)sprintf(logbuffer,"FORBIDDEN: %s:%s",s1, s2);
            break;
        case NOTFOUND:
            (void)write(socket_fd, "HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>\n",224);
            (void)sprintf(logbuffer,"NOT FOUND: %s:%s",s1, s2);
            break;
        case LOG: (void)sprintf(logbuffer," INFO: %s:%s:%d",s1, s2,socket_fd); break;
    }
    /* No checks here, nothing can be done with a failure anyway */
    if((fd = open("tws.log", O_CREAT| O_WRONLY | O_APPEND,0644)) >= 0) {
        (void)write(fd,logbuffer,strlen(logbuffer));
        (void)write(fd,"\n",1);
        (void)close(fd);
    }
}

/* this is the web server function implementing a tiny portion of the HTTP 1.1 specification */

int web(int fd, int hit)
{
    int j, file_fd, buflen;
    long i, ret, len;
    char * fstr;
    char buffer[BUFSIZE+1];

    ret =read(fd,buffer,BUFSIZE); 	/* read Web request in one go */

    printf("%s\n", buffer);

    if(ret == 0 || ret == -1) {	/* read failure stop now */
        logger(FORBIDDEN,"failed to read browser request","",fd);
        close(fd);
        return 1;
    }
    if(ret > 0 && ret < BUFSIZE)	/* return code is valid chars */
        buffer[ret]=0;		/* terminate the buffer */
    else buffer[0]=0;
    for(i=0;i<ret;i++)	/* remove CF and LF characters */
        if(buffer[i] == '\r' || buffer[i] == '\n')
            buffer[i]='*';
    logger(LOG,"request",buffer,hit);
    if( strncmp(buffer,"GET ",4) && strncmp(buffer,"get ",4) ) {

        logger(FORBIDDEN,"Only simple GET operation supported",buffer,fd);
        close(fd);
        return 1;
    }
    for(i=4;i<BUFSIZE;i++) { /* null terminate after the second space to ignore extra stuff */
        if(buffer[i] == ' ') { /* string is "GET URL " +lots of other stuff */
            buffer[i] = 0;
            break;
        }
    }
    for(j=0;j<i-1;j++) 	/* check for illegal parent directory use .. */
        if(buffer[j] == '.' && buffer[j+1] == '.') {

            logger(FORBIDDEN,"Parent directory (..) path names not supported",buffer,fd);
            close(fd);
            return 1;
        }
    if( !strncmp(&buffer[0],"GET /\0",6) || !strncmp(&buffer[0],"get /\0",6) ) /* convert no filename to index file */
        (void)strcpy(buffer,"GET /index.html");

    /* work out the file type and check we support it */
    buflen=strlen(buffer);
    fstr = (char *)0;
    for(i=0;extensions[i].ext != 0;i++) {
        len = strlen(extensions[i].ext);
        if( !strncmp(&buffer[buflen-len], extensions[i].ext, len)) {
            fstr =extensions[i].filetype;
            break;
        }
    }
    if(fstr == 0){

        logger(FORBIDDEN,"file extension type not supported",buffer,fd);
        close(fd);
        return 1;
    }

    if(( file_fd = open(&buffer[5],O_RDONLY)) == -1) {  /* open the file for reading */
        logger(NOTFOUND, "failed to open file",&buffer[5],fd);
        close(fd);
        return 1;
    }
    logger(LOG,"SEND",&buffer[5],hit);
    len = (long)lseek(file_fd, (off_t)0, SEEK_END); /* lseek to the file end to find the length */
    (void)lseek(file_fd, (off_t)0, SEEK_SET); /* lseek back to the file start ready for reading */
    (void)sprintf(buffer,"HTTP/1.1 200 OK\nServer: tws/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", VERSION, len, fstr); /* Header + a blank line */
    logger(LOG,"Header",buffer,hit);
    (void)write(fd,buffer,strlen(buffer));

    /* send file in 8KB block - last block may be smaller */
    while (	(ret = read(file_fd, buffer, BUFSIZE)) > 0 ) {
        (void)write(fd,buffer,ret);
    }
    sleep(1);	/* allow socket to drain before signalling the socket is closed */
    close(fd);
    return 0;
}


/// Produto Consumidor
dispatch_semaphore_t can_prod, can_cons;

int N ;

pthread_mutex_t trinco_p, trinco_c;
pthread_mutex_t buffer_trinco = PTHREAD_MUTEX_INITIALIZER;


//char *buffer;

int M;                  /// tamanho buffer
int prodptr = 0, constptr = 0;      //apontador para o produtor produzir num lugar onde o consumidor nao esteja a consumir





void *th_ondemand( void *params) {

    struct ON_DEMAND  *on_data = (struct ON_DEMAND *) params;

    //printf("socketfd: %d hit: %d\n", on_data->sock_fd, on_data->hit);

    web(on_data->sock_fd, on_data->hit);

    free(on_data);

    pthread_exit(NULL);
}



void *consumidor(){


        while(1){

            int socketfd;


            dispatch_semaphore_wait(can_cons, DISPATCH_TIME_FOREVER);

            pthread_mutex_lock(&trinco_c);


            socketfd = socketfd_buffer.sock_fd[constptr];

            constptr = (constptr + 1) % M;

            socketfd_buffer.hit++;

            pthread_mutex_unlock(&trinco_c);

            dispatch_semaphore_signal(can_prod);


            //printf("socketfd: %d socketfd_buffer.hit :%d\n", socketfd, socketfd_buffer.hit);

            web(socketfd, socketfd_buffer.hit);


        }

        pthread_exit(NULL);

    }


void *printBufferState() {

    while(1) {
        pthread_mutex_lock(&buffer_trinco);

        printf("Buffer State: [");
        for (int i = 0; i < M; i++) {

            printf("%d ", socketfd_buffer.sock_fd[(prodptr + i) % M]);
        }
        printf("]\n");

        pthread_mutex_unlock(&buffer_trinco);

        sleep(1);
    }
}


    /* just checks command line arguments, setup a listening socket and block on accept waiting for clients */

int main(int argc, char **argv) {
        int i, port, listenfd, socketfd, hit;
        socklen_t length;
        static struct sockaddr_in cli_addr; /* static = initialised to zeros */
        static struct sockaddr_in serv_addr; /* static = initialised to zeros */


        /// Produtor Consumidor
        if (argc < 5 || argc > 5 || !strcmp(argv[1], "-?")) {
            (void) printf("\n\nhint: ./tws Port-Number Top-Directory\t\tversion %d\n\n"
                          "\ttws is a small and very safe mini web server\n"
                          "\ttws only serves out file/web pages with extensions named below\n"
                          "\tand only from the named directory or its sub-directories.\n"
                          "\tThere are no fancy features = safe and secure.\n\n"
                          "\tExample: ./tws 8181 ./webdir 5 2\n\n"
                          "\tOnly Supports:", VERSION);
            for (i = 0; extensions[i].ext != 0; i++)
                (void) printf(" %s", extensions[i].ext);

            (void) printf("\n\tNot Supported: URLs including \"..\", Java, Javascript, CGI\n"
                          "\tNot Supported: directories / /etc /bin /lib /tmp /usr /dev /sbin \n\n");
            exit(0);
        }



        /* if (argc < 3 || argc > 3 || !strcmp(argv[1], "-?")) {
             (void) printf("\n\nhint: ./tws Port-Number Top-Directory\t\tversion %d\n\n"
                           "\ttws is a small and very safe mini web server\n"
                           "\ttws only serves out file/web pages with extensions named below\n"
                           "\tand only from the named directory or its sub-directories.\n"
                           "\tThere are no fancy features = safe and secure.\n\n"
                           "\tExample: ./tws 8181 ./webdir \n\n"
                           "\tOnly Supports:", VERSION);
             for (i = 0; extensions[i].ext != 0; i++)
                 (void) printf(" %s", extensions[i].ext);

             (void) printf("\n\tNot Supported: URLs including \"..\", Java, Javascript, CGI\n"
                           "\tNot Supported: directories / /etc /bin /lib /tmp /usr /dev /sbin \n\n");
             exit(0);
         }*/
        if (!strncmp(argv[2], "/", 2) || !strncmp(argv[2], "/etc", 5) ||
            !strncmp(argv[2], "/bin", 5) || !strncmp(argv[2], "/lib", 5) ||
            !strncmp(argv[2], "/tmp", 5) || !strncmp(argv[2], "/usr", 5) ||
            !strncmp(argv[2], "/dev", 5) || !strncmp(argv[2], "/sbin", 6)) {
            (void) printf("ERROR: Bad top directory %s, see tws -?\n", argv[2]);
            exit(3);
        }
        if (chdir(argv[2]) == -1) {
            (void) printf("ERROR: Can't Change to directory %s\n", argv[2]);
            exit(4);
        }

        logger(LOG, "tws starting", argv[1], getpid());

        /* setup the network socket */
        if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            logger(ERROR, "system call", "socket", 0);
        port = atoi(argv[1]);
        if (port < 0 || port > 60000)
            logger(ERROR, "Invalid port number (try 1->60000)", argv[1], 0);
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        serv_addr.sin_port = htons(port);
        if (bind(listenfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
            logger(ERROR, "system call", "bind", 0);
        if (listen(listenfd, 64) < 0)
            logger(ERROR, "system call", "listen", 0);


        /// default - parent process continuously waiting for incoming client connections using accept()

/*
        for (hit = 1;; hit++) {
            length = sizeof(cli_addr);
            // block waiting for clients //
            socketfd = accept(listenfd, (struct sockaddr *) &cli_addr, &length);
            if (socketfd < 0)
                logger(ERROR, "system call", "accept", 0);
            else
                web(socketfd, hit);

        }
    }*/



        /// A) continuously listens for incoming client connections using accept() function, and for each new connection
        /// it creates a thread to handle the client request "on demand"

      /*  pthread_t tid;
        for (hit = 1;; hit++) {


            /// allocate memory for each iteration so each struct has its own memory space
            struct ON_DEMAND* onDemand_data = malloc(sizeof(struct ON_DEMAND));

            if(onDemand_data == NULL){
                logger(ERROR, "", "malloc", 0);
                pexit("onDemand_data malloc: ");
            }

            onDemand_data->hit = hit;

            length = sizeof(cli_addr);

            socketfd = accept(listenfd, (struct sockaddr *) &cli_addr, &length);

            if (socketfd < 0) {
                logger(ERROR, "system call", "accept", 0);
                pexit("socketfd");
                // accept primeiro
            }


            onDemand_data->sock_fd = socketfd;

            if ((pthread_create(&tid, NULL, &th_ondemand, onDemand_data)) != 0) {
                logger(ERROR, "", "pthread_create", 0);
                pexit("pthread_create");
            }

        }

       pthread_join(tid, NULL);
    }*/



       /// Problema produtor consumidor

        M = atoi(argv[3]);       /// Shared buffer size
        N = atoi(argv[4]);      ///  Number of worker threads

        if(N == 0 || M == 0){           //atoi returns 0 if argument is not an integer
         pexit("argument of invalid type (must be int)");
     }


        pthread_t consids[N];   /// Array of thread consumer ids
        pthread_t printid;      /// thread id of print_thread


        pthread_mutex_init(&trinco_p, NULL);
        pthread_mutex_init(&trinco_c, NULL);


        /// Allocate memory for the array shared between threads
        socketfd_buffer.sock_fd = malloc( M * sizeof(struct PROD_CONS));


        if(socketfd_buffer.sock_fd == NULL){
            logger(ERROR, "", "malloc", 0);
            pexit("malloc socketfd_buffer");
        }


        can_prod = dispatch_semaphore_create(N);

        can_cons = dispatch_semaphore_create(0);


        for (int j = 0; j < N; ++j) {

            if (pthread_create(&consids[j], NULL, &consumidor, NULL) != 0) {
                logger(ERROR, "", "pthread_create", 0);
                pexit("error creating threads : ");
            }
        }

        pthread_create(&printid, NULL, printBufferState, NULL);

        for (socketfd_buffer.hit = 0;;) {

            length = sizeof(cli_addr);
            // block waiting for clients //
              socketfd = accept(listenfd, (struct sockaddr *) &cli_addr, &length);
            if (socketfd < 0) {
                logger(ERROR, "system call", "accept", 0);
                pexit("socketfd: ");
            }



            dispatch_semaphore_wait(can_prod, DISPATCH_TIME_FOREVER);


            pthread_mutex_lock(&trinco_p);


            socketfd_buffer.sock_fd[prodptr] = socketfd;

            prodptr = (prodptr + 1) % M;


            pthread_mutex_unlock(&trinco_p);


            dispatch_semaphore_signal(can_cons);


            //printf("produced: %d\n", socketfd);

        }




        for (int i = 0; i < N; ++i) {
            pthread_join(consids[i], NULL);
        }

        pthread_join(printid, NULL);

        free(socketfd_buffer.sock_fd);

    }




   /*    /// Máquina de estados finitos (FSM)

        pthread_t consids[POOL_SIZE];   /// Array of thread consumer ids
        pthread_t printid;      /// thread id of print_thread


        pthread_mutex_init(&trinco_p, NULL);
        pthread_mutex_init(&trinco_c, NULL);


        /// Allocate memory for the array shared between threads
        socketfd_buffer.sock_fd = malloc( SOCK_BUFF * sizeof(struct PROD_CONS));


        if(socketfd_buffer.sock_fd == NULL){
            logger(ERROR, "", "malloc", 0);
            pexit("malloc socketfd_buffer");
        }


        can_prod = dispatch_semaphore_create(POOL_SIZE);

        can_cons = dispatch_semaphore_create(0);


        for (int j = 0; j < POOL_SIZE; ++j) {

            if (pthread_create(&consids[j], NULL, &fsm_consumidor, NULL) != 0) {
                logger(ERROR, "", "pthread_create", 0);
                pexit("error creating threads : ");
            }
        }

       // pthread_create(&printid, NULL, printBufferState, NULL);

        for (socketfd_buffer.hit = 0;;) {

            length = sizeof(cli_addr);
            // block waiting for clients //
            socketfd = accept(listenfd, (struct sockaddr *) &cli_addr, &length);
            if (socketfd < 0) {
                logger(ERROR, "system call", "accept", 0);
                pexit("socketfd: ");
            }


            dispatch_semaphore_wait(can_prod, DISPATCH_TIME_FOREVER);


            pthread_mutex_lock(&trinco_p);


            socketfd_buffer.sock_fd[prodptr] = socketfd;

            prodptr = (prodptr + 1) % SOCK_BUFF;


            pthread_mutex_unlock(&trinco_p);


            dispatch_semaphore_signal(can_cons);


            //printf("produced: %d\n", socketfd);

        }




        for (int i = 0; i < N; ++i) {
            pthread_join(consids[i], NULL);
        }

        pthread_join(printid, NULL);

        free(socketfd_buffer.sock_fd);

    }

pthread_mutex_t hit_mutex = PTHREAD_MUTEX_INITIALIZER;  // Declare a mutex for hit variable

void *fsm_consumidor(){


    while(1){

        int socketfd;

        dispatch_semaphore_wait(can_cons, DISPATCH_TIME_FOREVER);

        pthread_mutex_lock(&trinco_c);


        socketfd = socketfd_buffer.sock_fd[constptr];

        constptr = (constptr + 1) % POOL_SIZE;

        pthread_mutex_lock(&hit_mutex);  // Lock the mutex before accessing hit
        socketfd_buffer.hit++;
        int hit = socketfd_buffer.hit;  // Store the current hit value locally
        pthread_mutex_unlock(&hit_mutex);  // Unlock the mutex


        pthread_mutex_unlock(&trinco_c);

        dispatch_semaphore_signal(can_prod);


        //printf("socketfd: %d socketfd_buffer.hit :%d\n", socketfd, socketfd_buffer.hit);

        th_web(socketfd, hit);


    }

    pthread_exit(NULL);

}



void th_web(int fd, int sock_hit){

    int socketfd = fd;
    int hit = sock_hit;

    printf("Sck_fd = %d hit: %d \n", socketfd, hit);

    int j, file_fd, buflen;
    long i, ret, len;
    char *fstr;
    char buffer[BUFSIZE + 1];

   // printf("%d\n", socketfd);
    STATE state = READY;


    while (state != END) {

        switch (state) {

            case READY:
                printf("READY\n");

                state = RECEIVING;
                break;

            case RECEIVING:
                printf("RECEIVING\n");

                ret = read(socketfd,buffer,BUFSIZE); 	// read Web request in one go //


                 printf("%s\n", buffer);

                if(ret == 0 || ret == -1) {	// read failure stop now //
                  //  printf("Hello\n");
                    logger(FORBIDDEN,"failed to read browser request","",socketfd);
                    //close(socketfd);
                    state = END;
                    break;
                }

                if(ret > 0 && ret < BUFSIZE)	// return code is valid chars //
                    buffer[ret]=0;		// terminate the buffer //
                else buffer[0]=0;

                for(i=0;i<ret;i++)	// remove CF and LF characters //
                    if(buffer[i] == '\r' || buffer[i] == '\n')
                        buffer[i]='*';


                logger(LOG,"request",buffer,hit);
                if( strncmp(buffer,"GET ",4) && strncmp(buffer,"get ",4) ) {
                    logger(FORBIDDEN,"Only simple GET operation supported",buffer,socketfd);
                    //close(socketfd);
                    state = END;
                    break;
                }
                for(i=4;i<BUFSIZE;i++) { // null terminate after the second space to ignore extra stuff /
                    if(buffer[i] == ' ') { // string is "GET URL " +lots of other stuff //
                        buffer[i] = 0;
                        break;
                    }
                }
                for(j=0;j<i-1;j++) 	// check for illegal parent directory use .. //
                    if(buffer[j] == '.' && buffer[j+1] == '.') {
                        logger(FORBIDDEN,"Parent directory (..) path names not supported",buffer,socketfd);
                       // close(socketfd);
                        state = END;
                        break;
                    }
                if( !strncmp(&buffer[0],"GET /\0",6) || !strncmp(&buffer[0],"get /\0",6) ) // convert no filename to index file /
                    (void)strcpy(buffer,"GET /index.html");


                state = PROCESSING;
                break;


            case PROCESSING:
                printf("PROCESSING\n");

                // work out the file type and check we support it //
                buflen=strlen(buffer);
                fstr = (char *)0;
                for(i=0;extensions[i].ext != 0;i++) {
                    len = strlen(extensions[i].ext);
                    if( !strncmp(&buffer[buflen-len], extensions[i].ext, len)) {
                        fstr =extensions[i].filetype;
                        break;
                    }
                }
                if(fstr == 0){
                    logger(FORBIDDEN,"file extension type not supported",buffer,socketfd);
                  //  close(socketfd);
                    state = END;
                    break;
                }

                if(( file_fd = open(&buffer[5],O_RDONLY)) == -1) {  // open the file for reading //
                    logger(NOTFOUND, "failed to open file",&buffer[5],socketfd);
                   // close(socketfd);
                    state = END;
                    break;
                }
                logger(LOG,"SEND",&buffer[5],hit);
                len = (long)lseek(file_fd, (off_t)0, SEEK_END); // lseek to the file end to find the length //
                (void)lseek(file_fd, (off_t)0, SEEK_SET); // lseek back to the file start ready for reading //
                (void)sprintf(buffer,"HTTP/1.1 200 OK\nServer: tws/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", VERSION, len, fstr); // Header + a blank line //
                logger(LOG,"Header",buffer,hit);
                (void)write(socketfd,buffer,strlen(buffer));


                state = SENDING;
                break;


            case SENDING:
                printf("SENDING\n");

                // send file in 8KB block - last block may be smaller //
                while (	(ret = read(file_fd, buffer, BUFSIZE)) > 0 ) {
                    (void)write(socketfd,buffer,ret);
                }

                state = END;
                break;

            default:

                break;


        }


    }
    printf("END\n");

    //memset(buffer, 0, sizeof(buffer));

    sleep(1);           // allow socket to drain before signalling the socket is closed //
    close(socketfd);



}*/







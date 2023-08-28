#ifndef PROJECTO_SO_THREADS_THREAD_STRUCTS_H
#define PROJECTO_SO_THREADS_THREAD_STRUCTS_H

#include <pthread.h>



struct R_DATA {

    int bsn;                  /// batch response number
    int rsn;                 /// request response number

};



struct FILE_DATA {

    pthread_t thread_id;
    int bsn;                  /// batch response number
    int rsn;                 /// request response number
    int rc;                 /// response code from server
    float time;            /// time per request

};



struct ON_DEMAND{

    int sock_fd;            /// socket fd
    int hit;               /// hit to send to the web function

};



struct FSM{

    int sock_fd;
    int hit;
};

struct PROD_CONS{

    int *sock_fd;                   /// Array to store the socket fds
    int hit;                       /// number of hits to send to the web function



};


typedef enum{

    READY,                  /// 0
    RECEIVING,             /// 1
    PROCESSING,           /// 2
    SENDING,             /// 3
    END                 /// 4
}STATE;



#endif





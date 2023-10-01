#ifndef PROJETO_SO_SO_STRUCTS_H
#define PROJETO_SO_SO_STRUCTS_H


struct Data {

    pid_t child_pid;
    int bsn;                /// batch response number
    int rsn;                /// request response number
    int rc;             /// response code from server
    float time;             /// time per request
};







#endif //PROJETO_SO_SO_STRUCTS_H

#ifndef PROJETO_SO_FUNCTIONS_H
#define PROJETO_SO_FUNCTIONS_H



/// function to print error message and exit form program
int pexit(char * msg)
{
    perror(msg);
    exit(1);
}

/*
int respCode(char *buffer){


    char *status_line_start = strstr(buffer, "HTTP/1.1 "); // find the start of the status line

    status_line_start += strlen("HTTP/1.1 "); // move past the "HTTP/1.1 " string

    char *status_line_end = strchr(status_line_start, ' '); //find first space chr after response code
    int status_len = status_line_end - status_line_start;      //lenght of rc substring

    char buffer2[status_len +1];

    strncpy(buffer2, status_line_start, status_len); // copy rc substring to buffer

    buffer2[status_len] = '\0';

    int rc1 = atoi(buffer2);


    return rc1;


}

*/

ssize_t ///Read "n" bytes from a descriptor
readn(int fd, void *ptr, size_t n)
{
    size_t nleft;
    ssize_t nread;
    nleft = n;
    while (nleft > 0) {
        if ((nread = read(fd, ptr, nleft)) < 0) {
            if (nleft == n)
                return(-1); /* error, return -1 */
            else
                break; /* error, return amount read so far */
        } else if (nread == 0) {
            break; /* EOF */
        }
        nleft -= nread;
        ptr += nread;
    }
    return(n - nleft); /* return >= 0 */
}

ssize_t /// Write "n" bytes to a descriptor
writen(int fd, const void *ptr, size_t n)
{
    size_t nleft;
    ssize_t nwritten;
    nleft = n;
    while (nleft > 0) {
        if ((nwritten = write(fd, ptr, nleft)) < 0) {
            if (nleft == n)
                return(-1); /* error, return -1 */
            else
                break; /* error, return amount written so far */
        } else if (nwritten == 0) {
            break;
        }
        nleft -= nwritten;
        ptr += nwritten;
    }
    return(n - nleft); /* return >= 0 */
}




///  Delete "benchmarkchild.txt" file history
void deleteFile() {
    if (remove("benchmarkchild.txt") == 0) {
        // printf("File deleted\n");
    }
}





#endif //PROJETO_SO_FUNCTIONS_H

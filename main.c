#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include "set.c"

void node(struct set_node *pNode);

void error(char *message) {
    perror(message);
    exit(0);
}

int handle_listen(int sock, struct sockaddr_in from, int request, int status, int from_length) {
    status = recvfrom(sock, &request, sizeof(int),0, (struct sockaddr *)&from, &from_length);
    if (status < 0) {
        error("recvfrom()");
    }

    switch (request) {
        case -1:
            //configure
            break;
        case 1:
            //get address
            break;
        case 2:
            //return address
            break;
        case 3:
            //renew lease (return + get address)
            break;
        case 0:
            //shutdown
            break;
    }

    return request;
}

int main(int argc, char *argv[]) {
//
//    int sock, server_length, from_length, n, message_code = 1, request, status;
//    struct sockaddr_in server;
//    struct sockaddr_in from;
//    char buf[1024];
//
//
//    sock = socket(AF_INET, SOCK_DGRAM, 0);
//    if (sock < 0){
//        error("socket()");
//    }
//
//    server_length = sizeof server;
//    bzero(&server, server_length);
//    server.sin_family = AF_INET;
//    server.sin_addr.s_addr = INADDR_ANY;
//    server.sin_port = htons(atoi("8888"));
//    if (bind(sock, (struct sockaddr*)&server, server_length) < 0) {
//        error("bind()");
//    }
//
//    from_length = sizeof from;
//    while(message_code) {
//        message_code = handle_listen(sock, from, request, status, from_length);
//    }
    struct set_node* start;
    struct set_node *last;
    struct set_node *three;
    struct set_node *two;
    struct set_node *one;

    allocate_set_node(start);
    allocate_set_node(one);
    allocate_set_node(two);
    allocate_set_node(three);
    allocate_set_node(last);

    struct in_addr* d1;
    struct in_addr* d2;
    struct in_addr* d3;
    d1->s_addr = 1;
    d2->s_addr = 2;
    d3->s_addr = 3;
    add_node(NULL, start);
    add_node(start, one);
    add_node(start, two);
    add_node(start, three);
    add_node(start, last);



    return 0;
}




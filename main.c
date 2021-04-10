#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


struct set_node{
    struct in_addr *data;
    struct set_node *next;
};

struct DLL {
    struct set_node *head;
    struct set_node *tail;
};

void init_DLL(struct DLL *list) {
    list->head = NULL;
    list->tail = NULL;
}

struct set_node* create_node(struct in_addr* data) {
    struct set_node *new_node;

    new_node = (struct set_node*) malloc(sizeof (struct set_node));

    new_node->data = data;
    new_node->next = NULL;

    return new_node;
}

void add_at_tail(struct DLL *list, struct in_addr *data) {
    struct set_node* new_node = create_node(data);

    if (list->head == NULL) {
        list->head = new_node;
        list->tail = new_node;
    } else {
        list->tail->next = new_node;
        list->tail = list->tail->next;
    }
}

void add_at_head(struct DLL *list, struct in_addr *data) {
    struct set_node* new_node = create_node(data);

    if (list->head == NULL) {
        list->head = new_node;
        list->tail = new_node;
    } else {
        new_node->next = list->head;
        list->head = new_node;
    }
}


in_addr_t delete_from_tail(struct DLL *list) {
    struct set_node *aux;
    in_addr_t data = -1;

    if (list->tail != NULL) {
        aux = list->head;
        while (aux->next != list->tail)
            aux = aux->next;

        data = list->tail->data->s_addr;

        free(list->tail->data);
        free(list->tail);

        list->tail = aux;
        list->tail->next = NULL;
    }
    return data;
}

in_addr_t delete_from_head(struct DLL *list) {
    in_addr_t data = -1;

    if(list->head != NULL) {
        struct set_node *to_delete = list->head;
        data = list->head->data->s_addr;
        list->head = list->head->next;

        free(to_delete->data);
        free(to_delete);
    }

    return data;
}

void print_list(struct DLL *list) {
    if (list->head != NULL) {
        struct set_node *aux;

        aux = list->head;

        while (aux != NULL) {
            printf("| %d |", aux->data->s_addr);
            aux = aux->next;

            if (aux != NULL)
                printf(" -----> ");
        }
    }
}

void empty_lsit(struct DLL *list) {
    while (list->head != NULL && list->tail != NULL)
        delete_from_head(list);

    free(list);
}
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
//    set_node* start;
//    allocate_set_node(start);
//    set_node *one;
//    allocate_set_node(one);
//    set_node *two;
//    allocate_set_node(two);
//    set_node *three;
//    allocate_set_node(three);
//    set_node *last;
//    allocate_set_node(last);
//
    struct in_addr *d1 = (struct in_addr*) malloc(sizeof (struct in_addr));
    struct in_addr *d2 = (struct in_addr*) malloc(sizeof (struct in_addr));
    struct in_addr *d3 = (struct in_addr*) malloc(sizeof (struct in_addr));
    d1->s_addr = 1;
    d2->s_addr = 2;
    d3->s_addr = 3;

    struct DLL *list = (struct DLL *) malloc(sizeof (struct DLL));
    init_DLL(list);

    add_at_tail(list, d1);
    add_at_tail(list, d2);
    add_at_tail(list, d3);

    print_list(list);

    delete_from_head(list);
    print_list(list);

    delete_from_tail(list);
    print_list(list);

    empty_lsit(list);
    return 0;
}




#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

/**
 * Node used in SLL
 *  - data - *in_addr
 *  - next - *node
 */
struct node{
    struct in_addr *data;
    struct node *next;
};

/**
 * SLL structure
 *  - head - *node: first element in SLL
 *  - tail - *node: last element in SLL
 *  - start_address - *in_addr: first address that can be given
 *  - end_address - *in_addr: last address that can be given
 *  - next_free_address - *in_addr: next address that will be given
 */
struct SLL {
    struct node *head;
    struct node *tail;
    struct in_addr *start_address;
    struct in_addr *end_address;
    struct in_addr *next_free_address;

};

/**
 * Initializes SLL
 * @param list - *SLL: address of list that needs initialization
 * @return -
 */
void init_SLL(struct SLL *list) {
    list->head = NULL;
    list->tail = NULL;
    list->start_address = NULL;
    list->end_address = NULL;
    list->next_free_address = NULL;
}

/**
 * Creates a node that wraps in_addr* data given as input
 * @param data - *in_addr: data that needs to be wrapped inside a node
 * @return *node which contains data given as @param
 */
struct node* create_node(struct in_addr* data) {
    struct node *new_node;

    new_node = (struct node*) malloc(sizeof (struct node));

    new_node->data = data;
    new_node->next = NULL;

    return new_node;
}

/**
 * Creates a node with input data and adds it to the end of the SLL
 * @param list - *SLL: list to which we add
 * @param data - in_addr*: data that needs to be added to the SLL
 * @return -
 */
void add_at_tail(struct SLL *list, struct in_addr *data) {
    struct node* new_node = create_node(data);

    if (list->head == NULL) {
        list->head = new_node;
        list->tail = new_node;
    } else {
        list->tail->next = new_node;
        list->tail = list->tail->next;
    }
}

/**
 * Creates a node with input data and adds it to the beggining of the SLL
 * @param list - *SLL: list to which we add
 * @param data - in_addr*: data that needs to be added to the SLL
 * @return -
 */
void add_at_head(struct SLL *list, struct in_addr *data) {
    struct node* new_node = create_node(data);

    if (list->head == NULL) {
        list->head = new_node;
        list->tail = new_node;
    } else {
        new_node->next = list->head;
        list->head = new_node;
    }
}

/**
 * Finds wether element is in SLL or not
 * @param list - *SLL: list where the search takes place
 * @param element - *in_addr: searched value
 * @return - True, if element was found
 *          - False, otherwise
 */
bool find_element(struct SLL *list, struct in_addr *element) {
    if (list->head == NULL)
        return false;

    struct node *current = list->head;

    while (current != NULL) {
        if (current->data->s_addr == element->s_addr)
            return true;

        current = current->next;
    }

    return false;
}

/**
 * Deletes last element of the SLL
 * @param list - *SLL: list from which we need to delete, also deallocates the memory occupied by the node
 * @return -
 */
in_addr_t delete_from_tail(struct SLL *list) {
    struct node *aux;
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

/**
 * Deletes first element of the SLL
 * @param list - *SLL: list from which we need to delete, also deallocates the memory occupied by the node
 * @return -
 */
in_addr_t delete_from_head(struct SLL *list) {
    in_addr_t data = -1;

    if (list->head != NULL) {
        struct node *to_delete = list->head;
        data = list->head->data->s_addr;
        list->head = list->head->next;

        free(to_delete->data);
        free(to_delete);
    }

    return data;
}

/**
 * Deletes element if exists
 * @param list
 * @param element to be deleted
 * @return value of element, if found
 *         -1, otherwise
 */
in_addr_t delete_element(struct SLL *list, struct in_addr *element) {
    in_addr_t found = -1;
    if (list->head == NULL)
        return found;

    if (list->head == list->tail) {
        if (list->head == element) {
            free(list->head);
            free(list->tail);
            list->head = NULL;
            list->tail = NULL;

            found = element->s_addr;
        }
    } else if (list->head->data->s_addr == element->s_addr) {
        return delete_from_head(list);
    } else if (list->tail->data->s_addr == element->s_addr) {
        return delete_from_tail(list);
    } else if (find_element(list, element)) {
        struct node *current = list->head->next;
        struct node *to_delete;

        while (current->next->data->s_addr != element->s_addr)
            current = current->next;

        to_delete = current->next;
        current->next = to_delete->next;
        found = to_delete->data->s_addr;

        free(to_delete);
    }

    return found;
}

/**
 * Returns random number in range (lower, upper)
 * @param lower bound
 * @param upper bound
 * @return random number inside (lower, upper)
 */
in_addr_t get_random_in_range(in_addr_t lower, in_addr_t upper) {
    srand(time(0));
    return (rand() % (upper - lower  + 1)) + upper;
}

/**
 * Changes first_free_address param of list to another one that was not used
 * @param list
 * @return -
 */
void change_free_address(struct SLL *list) {
    struct in_addr *first_free = (struct in_addr*) malloc(sizeof (struct in_addr));
    first_free->s_addr = get_random_in_range(list->start_address->s_addr,list->end_address->s_addr);

    while (find_element(list, first_free))
        first_free->s_addr = get_random_in_range(list->start_address->s_addr, list->end_address->s_addr);

    list->next_free_address = first_free;
}

/**
 * Prints all elements of SLL
 * @param list - *SLL: list that needs to be printed
 * @return -
 */
void print_list(struct SLL *list) {
    if (list->head != NULL) {
        struct node *aux;

        aux = list->head;

        while (aux != NULL) {
            printf("| %d |", aux->data->s_addr);
            aux = aux->next;

            if (aux != NULL)
                printf(" -----> ");
        }
    }
}

/**
 * Deletes all elements of the list and deallocates memory of list
 * @param list - *SLL: list that needs to be deleted
 * @return -
 */
void empty_list(struct SLL *list) {
    while (list->head != NULL && list->tail != NULL)
        delete_from_head(list);

    free(list);
}

void error(char *message) {
    perror(message);
    exit(0);
}

/**
 * Receieves configuration for ip range
 * @param sock - int: socket used
 * @param from - sockaddr_in: we get data from here
 * @param from_length - int: length of
 * @return -
 */
void receive_configuration(int sock, struct sockaddr_in from, int from_length, struct SLL *list) {
    printf("Receiving configuration...\n");
    init_SLL(list);

    if (recvfrom(sock, list->start_address, sizeof (struct in_addr), 0, (struct sockaddr*) &from, &from_length) < 0)
        error("recvfrom() - receive_configuration -> receive start address");
    else
        printf("\tReceived: start address: %d\n", list->start_address->s_addr);

    if (recvfrom(sock, list->end_address, sizeof (struct in_addr), 0, (struct sockaddr*) &from, &from_length) < 0)
        error("recvfrom() - receive_configuration -> receive end address");
    else
        printf("\tReceived: end address: %d\n-----------------\n", list->end_address->s_addr);

    struct in_addr *first_free = (struct in_addr*) malloc(sizeof (struct in_addr));
    first_free->s_addr = get_random_in_range(list->start_address->s_addr, list->end_address->s_addr);

    list->next_free_address = first_free;
}

/**
 * Used to send address to client
 * @param sock - int: socket used
 * @param from - sockaddr_in: we get data from here
 * @param from_length - int: length of
 * @return -
 */
void send_address(int sock, struct sockaddr_in from, int from_length, struct SLL *list) {
    printf("Sending address...\n");

    if( sendto(sock, list->next_free_address, sizeof (struct in_addr), 0, (struct sockaddr *)&from, from_length) < 0 )
        error("sendto() - send_address -> send of address failed");
    else
        printf("\tSend: address: %d\n-----------------\n", list->next_free_address->s_addr);

    add_at_tail(list, list->next_free_address);
    change_free_address(list);
}

/**
 * Used to send address to client
 * @param sock - int: socket used
 * @param from - sockaddr_in: we get data from here
 * @param from_length - int: length of
 * @return -
 */
void return_address(int sock, struct sockaddr_in from, int from_length, struct SLL *list) {
    printf("Getting returned address...\n");

    struct in_addr *returned_address = (struct in_addr*) malloc(sizeof (struct in_addr));

    if (recvfrom(sock, returned_address, sizeof (struct in_addr), 0, (struct sockaddr*) &from, &from_length) < 0)
        error("recvfrom() - return_address -> receive returned address");
    else
        printf("\tReceived: returned address: %d\n-----------------\n", returned_address->s_addr);


    if (delete_element(list, returned_address) < 0)
        error("delete_element() - address not found");

    free(returned_address);
}

/**
 * Listens for a request and send it to the handler
 * @param sock
 * @param from
 * @param request
 * @param status
 * @param from_length
 * @param list
 * @return
 */
int handle_listen(int sock, struct sockaddr_in from, int request, int status, int from_length, struct SLL* list) {
    status = recvfrom(sock, &request, sizeof(int),0, (struct sockaddr *)&from, &from_length);
    if (status < 0)
        error("recvfrom() - handle_listen -> request failed");

    switch (request) {
        case -1:
            //configure
            receive_configuration(sock, from, from_length, list);
            break;
        case 1:
            //get address
            send_address(sock, from, from_length, list);
            break;
        case 2:
            //return address
            return_address(sock, from, from_length, list);
            break;
        case 3:
            //renew lease (return + get address)
            //might not do it this way, because, lease time might be incremented from client side
            //only will do that if we want to change the address
            break;
        case 0:
            //shutdown
            empty_list(list);
            break;
    }

    return request;
}

void SLL_usage() {
    struct in_addr *d1 = (struct in_addr*) malloc(sizeof (struct in_addr));
    struct in_addr *d2 = (struct in_addr*) malloc(sizeof (struct in_addr));
    struct in_addr *d3 = (struct in_addr*) malloc(sizeof (struct in_addr));
    d1->s_addr = 1;
    d2->s_addr = 2;
    d3->s_addr = 3;

    struct SLL *list = (struct SLL *) malloc(sizeof (struct SLL));
    init_SLL(list);

    add_at_tail(list, d1);
    add_at_tail(list, d2);
    add_at_tail(list, d3);

    print_list(list);

    delete_from_head(list);
    print_list(list);

    delete_from_tail(list);
    print_list(list);

    empty_list(list);
}


int main(int argc, char *argv[]) {

    int sock, server_length, from_length, n, message_code = 1, request, status;
    struct sockaddr_in server;
    struct sockaddr_in from;
    struct SLL *list = (struct SLL *) malloc(sizeof (struct SLL));



    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        error("socket()");

    server_length = sizeof server;
    bzero(&server, server_length);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(atoi("8888"));
    if (bind(sock, (struct sockaddr*)&server, server_length) < 0)
        error("bind()");

    from_length = sizeof from;
    while(message_code)
        message_code = handle_listen(sock, from, request, status, from_length, list);

    return 0;
}
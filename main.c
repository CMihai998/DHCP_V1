#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <ifaddrs.h>
#include <arpa/inet.h>

#define WG_INTERFACE_NAME "wg0"
#define DHCP_PORT 6969
#define CONFIG_FILE "/etc/wireguard/wg0.conf"
#define START_INTERFACE_COMMAND "wg-quick up wg0"
#define STOP_INTERFACE_COMMAND "wg-quick down wg0"


bool SHUTDOWN = false;

struct Configuration {
    char PUBLIC_KEY[256];
    char ALLOWED_IPS[256];
};


/**
 * Node used in SLL
 *  - data - *in_addr
 *  - next - *node
 */
struct node{
    struct in_addr *data;
    struct node *next;
};

struct State {
    struct SLL *list;
    struct in_addr *start_address;
    struct in_addr *end_address;
    struct in_addr *next_free_address;
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
};

/**
 * Initializes SLL
 * @param list - *SLL: address of list that needs initialization
 * @return -
 */
void init_SLL(struct State *state) {
    state->list = (struct SLL *) malloc(sizeof (struct SLL));
    state->list->head = NULL;
    state->list->tail = NULL;
    state->start_address = NULL;
    state->end_address = NULL;
    state->next_free_address = NULL;
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
        if (list->head->data->s_addr == element->s_addr) {
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

        if (current->data->s_addr == element->s_addr) {
            list->head->next = current->next;
            found = current->data->s_addr;
            free(current);

            goto end;
        }
        while (current->next->data->s_addr != element->s_addr)
            current = current->next;

        to_delete = current->next;
        current->next = to_delete->next;
        found = to_delete->data->s_addr;

        free(to_delete);
    }
    end:
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
    return (rand() % (upper - lower  + 1)) + lower;
}

/**
 * Changes first_free_address param of list to another one that was not used
 * @param list
 * @return -
 */
void change_free_address(struct State *state) {
    struct in_addr *first_free = (struct in_addr*) malloc(sizeof (struct in_addr));
    first_free->s_addr = get_random_in_range(state->start_address->s_addr,state->end_address->s_addr);

    while (find_element(state->list, first_free))
        first_free->s_addr = get_random_in_range(state->start_address->s_addr, state->end_address->s_addr);

    state->next_free_address = first_free;
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
void receive_configuration(int sock, struct sockaddr_in *from, int from_length, struct State *state) {
    printf("Receiving configuration...\n");
    init_SLL(state);
    state->start_address = (struct in_addr*) malloc(sizeof (struct in_addr));
    state->end_address = (struct in_addr*) malloc(sizeof (struct in_addr));
    if (recvfrom(sock, &state->start_address->s_addr, sizeof (in_addr_t), 0, (struct sockaddr*)from, &from_length) < 0)
        error("recvfrom() - receive_configuration -> receive start address");
    else
        printf("\tReceived: start address: %d\n", state->start_address->s_addr);

    if (recvfrom(sock, &state->end_address->s_addr, sizeof (in_addr_t), 0, (struct sockaddr*)from, &from_length) < 0)
        error("recvfrom() - receive_configuration -> receive end address");
    else
        printf("\tReceived: end address: %d\n-----------------\n", state->end_address->s_addr);

    struct in_addr *first_free = (struct in_addr*) malloc(sizeof (struct in_addr));
    first_free->s_addr = get_random_in_range(state->start_address->s_addr, state->end_address->s_addr);

    state->next_free_address = first_free;
}

/**
 * Used to send address to client
 * @param sock - int: socket used
 * @param from - sockaddr_in: we get data from here
 * @param from_length - int: length of
 * @return -
 */
void send_address(int sock, struct sockaddr_in *from, int from_length, struct State *state) {
    printf("Sending address...\n");

    if(sendto(sock, &state->next_free_address->s_addr, sizeof (in_addr_t), 0, (const struct sockaddr *) from, from_length) < 0 )
        error("sendto() - send_address -> send of address failed");
    else
        printf("\tSend: address: %d\n-----------------\n", state->next_free_address->s_addr);

    add_at_tail(state->list, state->next_free_address);
    change_free_address(state);
}

/**
 * Used to send address to client
 * @param sock - int: socket used
 * @param from - sockaddr_in: we get data from here
 * @param from_length - int: length of
 * @return -
 */
void return_address(int sock, struct sockaddr_in *from, int from_length, struct State *state) {
    printf("Getting returned address...\n");

    struct in_addr *returned_address = (struct in_addr*) malloc(sizeof (struct in_addr));
    char command[256] = "route del ", address[256];

    if (recvfrom(sock, &returned_address->s_addr, sizeof (in_addr_t), 0, (struct sockaddr*)from, &from_length) < 0)
        error("recvfrom() - return_address -> receive returned address");
    else
        printf("\tReceived: returned address: %d\n-----------------\n", returned_address->s_addr);


    if (delete_element(state->list, returned_address) < 0)
        error("delete_element() - address not found");

    inet_ntop(AF_INET, returned_address, address, 255);
    strcat(command, address);
    system(command);

    free(returned_address);
}

/**
 * Receiving real IP address of the client
 * @param sock
 * @param from
 * @param from_length
 * @return address of the client
 */
struct in_addr* receive_address(int sock, struct sockaddr_in *from, int from_length) {
    printf("Getting client's IP address...\n");

    struct in_addr *client_address = (struct in_addr*) malloc(sizeof (struct in_addr));

    if (recvfrom(sock, &client_address->s_addr, sizeof (in_addr_t), 0, (struct sockaddr*)from, &from_length) < 0)
        error("recvfrom() - receive_address -> receive client's address");
    else
        printf("\tReceived: client's address: %d\n-----------------\n", client_address->s_addr);

    return client_address;
}

/**
 * Shuts server down
 * @param sock
 * @param list
 */
void shutdown_server(int sock, struct State *state) {
    print_list(state->list);
    empty_list(state->list);
    close(sock);
    free(state);
}

void start_interface() {
    system(START_INTERFACE_COMMAND);
}

void stop_interface() {
    system(STOP_INTERFACE_COMMAND);
}



/**
 * Checks if wireguard interface is down and if so, stops the application
 */
void *check_for_shutdown() {
    struct ifaddrs *ifaddr;
    bool found;


    LOOP:
    sleep(120);

    found = false;
    if (getifaddrs(&ifaddr) == -1)
        error("getifaddrs() - check_for_shutdown()");

    for (struct ifaddrs *current = ifaddr; current != NULL && found == false; current = current->ifa_next) {
        if (current->ifa_addr == NULL)
            continue;

        if (strcmp(current->ifa_name, WG_INTERFACE_NAME) == 0)
            found = true;
    }

    freeifaddrs(ifaddr);

    if (found == false) {
        SHUTDOWN = true;
        return NULL;
    }
    goto LOOP;
}


/**
 * Listens for a request and send it to the handler
 * @param sock
 * @param from
 * @param status
 * @param from_length
 * @param list
 * @return
 */
int handle_listen(int sock, struct sockaddr_in *from, struct sockaddr_in *server, int status, int from_length, struct State* state) {
    int request = 0;
    status = recvfrom(sock, &request, sizeof(int),0, (struct sockaddr*)from, &from_length);
    if (status < 0)
        error("recvfrom() - handle_listen -> request failed");

    switch (request) {
        case -1:
            //configure
            receive_configuration(sock, from, from_length, state);
            break;
        case 1:
            //get address
            send_address(sock, from, from_length, state);
            break;
        case 2:
            //return address
            return_address(sock, from, from_length, state);
            break;
        case 3:
            //receive address of the client
            receive_address(sock, from, from_length);
            break;
        case 4:
            //renew lease (return + get address)
            //might not do it this way, because, lease time might be incremented from client side
            //only will do that if we want to change the address
            break;
        case 0:
            //shutdown
            shutdown_server(sock, state);
            break;
    }

    return request;
}

bool is_auto_configurable() {
    char line[512], *word_list[64], delimit[] = " ";
    FILE *config_file;
    int words_per_line;
    config_file = fopen(CONFIG_FILE, "r");

    if (config_file == NULL)
        error("fopen() - is_auto_configurable - CONFIG_FILE");

    while (fgets (line, 512, config_file)) {
        words_per_line = 0;
        word_list[words_per_line] = strtok(line, delimit);
        while (word_list[words_per_line] != NULL)
            word_list[++words_per_line] = strtok(NULL, delimit);

        if (strcmp(word_list[0], "AutoConfigurable") == 0 && strcmp(word_list[2], "True") == 0) {
            fclose(config_file);
            return true;
        }
    }
    fclose(config_file);
    return false;
}

void configure_state(struct State *state) {
    char line[512], *word_list[64], delimit[] = " ";
    FILE *config_file;
    int words_per_line;
    config_file = fopen(CONFIG_FILE, "r");

    if (config_file == NULL)
        error("fopen() - configure_state - CONFIG_FILE");

    while (fgets (line, 512, config_file)) {
        words_per_line = 0;
        word_list[words_per_line] = strtok(line, delimit);
        while (word_list[words_per_line] != NULL)
            word_list[++words_per_line] = strtok(NULL, delimit);

        if (strcmp(word_list[0], "Address") == 0) {
            char *address, *mask;
            address = strtok(word_list[2], "/");
            mask = strtok(NULL, "/");

            int msk = atoi(mask);
            struct sockaddr_in a1, a2;
            inet_pton(AF_INET, address, &(a1.sin_addr));

            printf("-------  %d\n", a1.sin_addr.s_addr);

            a1.sin_addr.s_addr = a1.sin_addr.s_addr;

            char aux[INET_ADDRSTRLEN];

            inet_ntop(AF_INET, &(a1.sin_addr), aux, INET_ADDRSTRLEN);

            printf("addr: %s\nmask: %d", address, msk);


            a2.sin_addr.s_addr = 0;
            for (int i = 0; i < msk; i++) {
                a2.sin_addr.s_addr <<= 1;
                a2.sin_addr.s_addr |= 1;
            }

            inet_ntop(AF_INET, &(a2.sin_addr), aux, INET_ADDRSTRLEN);
            printf("-------  %s\n", aux);

            fclose(config_file);
            return;
        }
    }
    fclose(config_file);
    error("configure_state - "
          "config file should contain the address of the interface together with the mask to determine allowed peers");
}

int run() {
    start_interface();

    if (!is_auto_configurable())
        goto END;

    int sock, server_length, from_length, n, message_code = 1, request, status;
    struct sockaddr_in *server = (struct sockaddr_in*) malloc(sizeof (struct sockaddr_in));
    struct sockaddr_in *from = (struct sockaddr_in*) malloc(sizeof (struct sockaddr_in));
    struct State *state = (struct State *) malloc(sizeof (struct State));



    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        error("socket()");
    server->sin_family = AF_INET;
    server->sin_addr.s_addr = INADDR_ANY;
    server->sin_port = htons(DHCP_PORT);
    server_length = sizeof (struct sockaddr_in);

    if (bind(sock, (struct sockaddr*)server, server_length) < 0)
        error("bind()");

    configure_state(state);

    pthread_t thread;
    pthread_create(&thread, NULL, check_for_shutdown, NULL);

    while(!SHUTDOWN)
        handle_listen(sock, from, server, status, from_length, state);

    pthread_join(thread, NULL);
    shutdown_server(sock, state);

    END:
    return 0;
}

int main(int argc, char *argv[]) {

    run();

}


struct Configuration *receive_client_configuration(int sock, struct sockaddr_in *from, int from_length) {
    struct Configuration *received_configuration = (struct Configuration*) malloc(sizeof (struct Configuration));

    printf("Receiving configuration...\n");
    if (recvfrom(sock, &received_configuration, sizeof (received_configuration), 0, (struct sockaddr*)from, &from_length) < 0)
        error("recvfrom() - receive_client_configuration -> receival of new client configuration");
    else
        printf("\tReceived: Configuration:\n"
               "\tPublic Key: %s\n"
               "\tAllowed IPs: %s\n",
               received_configuration->PUBLIC_KEY, received_configuration->ALLOWED_IPS);

    return received_configuration;
}

void add_new_peer(struct Configuration *new_client, struct in_addr *client_address) {
    char command[256] = "route add ", address[256];

    //write to file maybe
    system("sudo wg addconf wg0 <(wg-quick strip wg0)");
    inet_ntop(AF_INET, client_address, address, 255);
    strcat(command, address);
    strcat(command, " wg0");
    system(command);

}

void run_loop(int sock, struct sockaddr_in *from, struct sockaddr_in *server, int status, int from_length, struct State* state) {
    struct Configuration *new_client = receive_client_configuration(sock, from, from_length);
    send_address(sock, from, from_length, state);
    add_new_peer(new_client, state->list->tail->data);

}




//TODO: Modify wg0.conf file when new client joins
//TODO: Remove client from wg0.conf when he logs off
//TODO: use dummy wg.conf file with AutoSave = False in the case of dynamic usage
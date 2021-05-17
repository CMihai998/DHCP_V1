#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <ifaddrs.h>
#include <arpa/inet.h>

#define WG_INTERFACE_NAME "wg0"
#define WG_DUMMY_INTERFACE_NAME "wg_dummmy"

#define DHCP_PORT 8888

#define CONFIG_FILE "/etc/wireguard/wg0.conf"
#define CONFIG_DUMMY_FILE "/etc/wireguard/wg_dummmy.conf"
#define AUX_FILE "/etc/wireguard/aux.conf"

#define CREATE_DUMMY_FILE_COMMAND "touch /etc/wireguard/wg_dummmy.conf"
#define START_INTERFACE_COMMAND "wg-quick up wg0"
#define START_DUMMY_INTERFACE_COMMAND "wg-quick up wg_dummmy"
#define STOP_INTERFACE_COMMAND "wg-quick down wg_dummmy"
#define REMOVE_OLD_DUMMY_CONFIG_FILE_COMMAND "sudo rm /etc/wireguard/wg_dummmy.conf"
#define CREATE_AUX_FILE_COMMAND "touch /etc/wireguard/aux.conf"
#define REPLACE_OLD_CONFIG_FILE_COMMAND "sudo rm /etc/wireguard/wg_dummmy.conf && sudo cp /etc/wireguard/aux.conf /etc/wireguard/wg_dummmy.conf && sudo rm /etc/wireguard/aux.conf"

bool SHUTDOWN = false;
bool DUMMY_INTERFACE_CONFIGURED = false;

struct Message {
    int OPTION;
    char PUBLIC_KEY[256];
    char ALLOWED_IPS[256];
    in_addr_t ADDRESS;
    char ENDPOINT[30];
    char PORT[10];
};

int NET_MASK;

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
 * Used to send address to client
 * @param sock - int: socket used
 * @param from - sockaddr_in: we get data from here
 * @param from_length - int: length of
 * @return -
 */
void send_address_and_mask(int sock, struct sockaddr_in *from, int from_length, struct State *state) {
    char *readable_address = inet_ntoa(*state->next_free_address);

    printf("Sending address...\n");

    if(sendto(sock, &state->next_free_address->s_addr, sizeof (in_addr_t), 0, (const struct sockaddr *) from, from_length) < 0 )
        error("sendto() - send_address_and_mask -> send of address failed");
    else
        printf("\tSent: address: %s\n-----------------\n",  readable_address);

    if(sendto(sock, &NET_MASK, sizeof (int ), 0, (const struct sockaddr *) from, from_length) < 0 )
        error("sendto() - send_address_and_mask -> send of mask failed");
    else
        printf("\tSent: mask: %d\n-----------------\n", NET_MASK);

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
void return_address(struct State *state, in_addr_t address_data) {

    struct in_addr *returned_address = (struct in_addr*) malloc(sizeof (struct in_addr));
    char command[256] = "route del ", address[256];

    returned_address->s_addr = address_data;

    if (delete_element(state->list, returned_address) < 0)
        error("delete_element() - address not found");

    inet_ntop(AF_INET, returned_address, address, 255);
    strcat(command, address);
    system(command);

    free(returned_address);
}

void configure_dummy_interface() {
    char line[512], *word_list[64], delimit[] = " ", new_line[512];
    FILE *config_file, *dummy_config_file;
    int words_per_line;

    system(REMOVE_OLD_DUMMY_CONFIG_FILE_COMMAND);
    config_file = fopen(CONFIG_FILE, "r");
    system(CREATE_DUMMY_FILE_COMMAND);
    dummy_config_file = fopen(CONFIG_DUMMY_FILE, "w");

    if (config_file == NULL)
        error("fopen() - configure_state - CONFIG_FILE");

    while (fgets (line, 512, config_file)) {
        words_per_line = 0;
        word_list[words_per_line] = strtok(line, delimit);
        while (word_list[words_per_line] != NULL)
            word_list[++words_per_line] = strtok(NULL, delimit);

        if (strcmp(word_list[0], "SaveConfig") == 0 && strcmp(word_list[2], "true"))
            strcpy(line, "SaveConfig = false\n");
        if (strcmp(word_list[0], "AutoConfigurable") != 0) {
            strcpy(new_line, "");
            for (int i = 0; i < words_per_line - 1; i++) {
                strcat(new_line, word_list[i]);
                strcat(new_line, " ");
            }
            strcat(new_line, word_list[words_per_line - 1]);
            fprintf(dummy_config_file, "%s", new_line);
        }
    }
    fclose(config_file);
    fclose(dummy_config_file);
}

void start_interface(char *interface_name) {
    if (strcmp(interface_name, WG_INTERFACE_NAME) == 0) {
        system(START_INTERFACE_COMMAND);
        return;
    }
    if (!DUMMY_INTERFACE_CONFIGURED) {
        configure_dummy_interface();
        DUMMY_INTERFACE_CONFIGURED = true;
    }
    system(START_DUMMY_INTERFACE_COMMAND);
}

void stop_interface() {
    system(STOP_INTERFACE_COMMAND);
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
    stop_interface();
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

        if (strcmp(current->ifa_name, WG_DUMMY_INTERFACE_NAME) == 0)
            found = true;
    }

    freeifaddrs(ifaddr);

    if (found == false) {
        SHUTDOWN = true;
        return NULL;
    }
    goto LOOP;
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

        if (strcmp(word_list[0], "AutoConfigurable") == 0 && strcmp(word_list[2], "True\n") == 0) {
            fclose(config_file);
            return true;
        }
    }
    fclose(config_file);
    return false;
}

void initialize_state(struct State *state) {
    state->start_address = (struct in_addr*) malloc(sizeof (struct in_addr));
    state->start_address->s_addr = (in_addr_t*) malloc(sizeof (in_addr_t));

    state->end_address = (struct in_addr*) malloc(sizeof (struct in_addr));
    state->end_address->s_addr = (in_addr_t*) malloc(sizeof (in_addr_t));

    state->next_free_address = (struct in_addr*) malloc(sizeof (struct in_addr));
    state->next_free_address->s_addr = (in_addr_t*) malloc(sizeof (in_addr_t));

    init_SLL(state);
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

            NET_MASK= atoi(mask);
            struct sockaddr_in a1, a2;
            inet_pton(AF_INET, address, &(a1.sin_addr));

            printf("-------  %d\n", a1.sin_addr.s_addr);

            a1.sin_addr.s_addr = a1.sin_addr.s_addr;

            char aux[INET_ADDRSTRLEN];

            inet_ntop(AF_INET, &(a1.sin_addr), aux, INET_ADDRSTRLEN);

            printf("addr: %s\nmask: %d", address, NET_MASK);


            a2.sin_addr.s_addr = 0;
            for (int i = 0; i < NET_MASK; i++) {
                a2.sin_addr.s_addr <<= 1;
                a2.sin_addr.s_addr |= 1;
            }
            initialize_state(state);
            state->start_address = &a1.sin_addr;
            state->end_address = &a2.sin_addr;
            change_free_address(state);

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


struct Message *receive_client_configuration(int sock, struct sockaddr_in *from, int from_length) {
    struct Message *received_configuration = (struct Message*) malloc(sizeof (struct Message));

    printf("Receiving configuration...\n");
    if (recvfrom(sock, received_configuration, sizeof (struct Message), 0, (struct sockaddr*)from, &from_length) < 0)
        error("recvfrom() - receive_client_configuration -> receival of new client configuration");
    else
        printf("Successfully Received: MY_CONFIGURATION (struct Configuration)"
               "\n\t\tOPTION (int) : %d"
               "\n\t\tPUBLIC_KEY (char[256]) : %s"
               "\n\t\tALLOWED_IPS (char[256] : %s"
               "\n\t\tADDRESS (in_addr_t) : %d"
               "\n\t\tENDPOINT (char[30]) : %s"
               "\n\t\tPORT (char[6]) : %s\n",
               received_configuration->OPTION, received_configuration->PUBLIC_KEY, received_configuration->ALLOWED_IPS, received_configuration->ADDRESS, received_configuration->ENDPOINT, received_configuration->PORT);

    return received_configuration;
}

void refresh_interface() {
    sleep(5);
    system(STOP_INTERFACE_COMMAND);
    system(START_DUMMY_INTERFACE_COMMAND);
}

void add_new_peer(struct Message *new_client, struct in_addr *client_address) {
    char command[256] = "route add ", address[256], buffer[2048];
    struct in_addr endpoint;
    FILE *config_file;

    strcpy(buffer, "\n[Peer]\nPublicKey = ");
    strcat(buffer, new_client->PUBLIC_KEY);
    strcat(buffer, "AllowedIPs = ");
    strcat(buffer, new_client->ALLOWED_IPS);
    strcat(buffer, "Endpoint = ");
    strcat(buffer, new_client->ENDPOINT);
    strcat(buffer, ":");
    strcat(buffer, new_client->PORT);

    config_file = fopen(CONFIG_DUMMY_FILE, "a");
    if (config_file == NULL)
        error("fopen() - add_new_peer - couldn't open config file");

    fprintf(config_file, "%s", buffer);
    fclose(config_file);

    //    system("sudo wg addconf wg_dummmy <(wg-quick strip wg_dummmy)");
    inet_ntop(AF_INET, client_address, address, 255);
    fprintf("ADDR: %s/n", address);
    strcat(command, new_client->ENDPOINT);
    strcat(command, " wg_dummmy");
    system(command);
    refresh_interface();
}

void remove_peer(struct Message *peer_information) {
    uint start_index = 0, end_index = 0, words_per_line, current_index = 0, comparable_endpoint[40];
    char line[512], *word_list[64], delimit[] = " ";
    bool found = false;
    FILE *config_file, *aux_file;

    config_file = fopen(CONFIG_DUMMY_FILE, "r");
    if (config_file == NULL)
        error("fopen() - remove_peer - CONFIG_DUMMY_FILE");

    //read until we get to a [Peer]
    while (!found && fgets(line, 512, config_file)) {
        if (strcmp(line, "[Peer]\n") == 0) {
            found = true;
            start_index = current_index;
            end_index = current_index;
        }
        current_index++;
    }

    found = false;
    while (!found && fgets(line, 512, config_file)) {
        words_per_line = 0;
        word_list[words_per_line] = strtok(line, delimit);
        while (word_list[words_per_line] != NULL)
            word_list[++words_per_line] = strtok(NULL, delimit);

        if (strcmp(word_list[0], "[Peer]\n") == 0) {
            start_index = current_index;
            end_index = current_index;
        }
        if (strcmp(word_list[0], "Endpoint") == 0) {
            strcpy(comparable_endpoint, "");
            strcat(comparable_endpoint, peer_information->ENDPOINT);
            strcat(comparable_endpoint, ":");
            strcat(comparable_endpoint, peer_information->PORT);
            if (strcmp (word_list[2], comparable_endpoint) == 0) {
                bool peer_indices_set = false;
                found = true;

                while (!peer_indices_set && fgets(line, 512, config_file)) {
                    if (strcmp(line, "[Peer]\n") == 0)
                        peer_indices_set = true;
                    else
                        current_index++;
                }
                end_index = current_index;
            }
        }
        current_index++;
    }
    fclose(config_file);
    //write to AUX_FILE
    system(CREATE_AUX_FILE_COMMAND);
    config_file = fopen(CONFIG_DUMMY_FILE, "r");
    if (config_file == NULL)
        error("fopen() - remove_peer - 2nd CONFIG_DUMMY_FILE");
    aux_file = fopen(AUX_FILE, "w");
    if (aux_file == NULL)
        error("fopen() - remove_peer - AUX_FILE");
    current_index = 0;
    while (fgets(line, 512, config_file)) {
        if (current_index < start_index || current_index > end_index)
            fprintf(aux_file, "%s", line);
        current_index++;
    }
    fclose(config_file);
    fclose(aux_file);
    system(REPLACE_OLD_CONFIG_FILE_COMMAND);
    refresh_interface();
}

void run_loop(int sock, struct sockaddr_in *from, struct sockaddr_in *server, int from_length, struct State* state) {
    struct Message *new_message = receive_client_configuration(sock, from, from_length);

    switch (new_message->OPTION) {
        case 0:
            send_address_and_mask(sock, from, from_length, state);
            add_new_peer(new_message, state->list->tail->data);
            break;
        case 1:
            return_address(state, new_message->ADDRESS);
            remove_peer(new_message);
            break;
    }
}

void usage() {

    if (!is_auto_configurable()) {
        start_interface(WG_INTERFACE_NAME);
        goto END;
    }

    struct sockaddr_in *si_me = (struct sockaddr_in*) malloc(sizeof (struct sockaddr_in));
    struct sockaddr_in *si_other = (struct sockaddr_in*) malloc(sizeof (struct sockaddr_in));

        int s, slen = sizeof(struct sockaddr_in);

        if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
            error("socket");


        si_me->sin_family = AF_INET;
        si_me->sin_port = htons(DHCP_PORT);
        si_me->sin_addr.s_addr = htonl(INADDR_ANY);
        //bind socket to port
        if( bind(s , (struct sockaddr*)si_me, slen ) == -1)
            error("bind");

    struct State *state = (struct State *) malloc(sizeof (struct State));
    configure_state(state);
    start_interface(WG_DUMMY_INTERFACE_NAME);
    pthread_t thread;
    pthread_create(&thread, NULL, check_for_shutdown, NULL);

    while(!SHUTDOWN)
        run_loop(s, si_other, si_me, slen, state);

    pthread_join(thread, NULL);
    shutdown_server(s, state);
    close(s);

    END:
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    usage();
}
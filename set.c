//
// Created by mihai on 4/9/21.
//

#include <stdlib.h>


struct set_node {
    struct in_addr* address;
    struct set_node* next;
};

void allocate_set_node(struct set_node *node) {
    calloc(&node, sizeof (struct set_node));
    node->next = NULL;
}

void deallocate_set_node(struct set_node *node) {
    free(&node->address);
    free(node);
}

struct set_node* add_node(struct set_node* start, struct in_addr* new_data) {
    struct set_node* new_node;
    allocate_set_node(new_node);


    new_node->address = new_data;
    new_node->next = start;
}

struct set_node* delete_last(struct set_node* start) {
    if (start == NULL)
        return start;

    struct set_node* current = start;
    while (current->next->next != NULL) {
        current = current->next;
    }

    deallocate_set_node(current->next);

    current->next = NULL;
    return current;
}
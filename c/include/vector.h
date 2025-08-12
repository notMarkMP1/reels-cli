#ifndef VECTOR_H
#define VECTOR_H

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

typedef struct {
    char **data;
    size_t size;
    size_t capacity;
} string_vector;

void vector_init(string_vector *v);

void vector_push_back(string_vector *v, const char *str);

char* vector_get(const string_vector *v, size_t index);

void vector_free(string_vector *v);

#endif // VECTOR_H
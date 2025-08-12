#include "vector.h"

void vector_init(string_vector *v){
    v->size = 0;
    v->capacity = 10;
    v->data = malloc(v->capacity * sizeof(char*));
}

void vector_push_back(string_vector *v, const char *str) {
    if (v->size >= v->capacity) {
        v->capacity *= 2;
        v->data = realloc(v->data, v->capacity * sizeof(char*));
    }
    v->data[v->size] = strdup(str);
    v->size++;
}

char* vector_get(const string_vector *v, size_t index) {
    if (index < v->size) {
        return v->data[index];
    }
    return NULL;
}

void vector_free(string_vector *v) {
    for (size_t i = 0; i < v->size; i++) {
        free(v->data[i]);
    }
    free(v->data);
}
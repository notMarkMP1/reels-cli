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

int vector_contains(const string_vector *v, const char *str) {
    if (!v || !str) {
        return 0;
    }
    
    for (size_t i = 0; i < v->size; i++) {
        if (v->data[i] && strcmp(v->data[i], str) == 0) {
            return 1; // found
        }
    }
    return 0;
}

void vector_push_back_unique(string_vector *v, const char *str) {
    if (!vector_contains(v, str)) {
        vector_push_back(v, str);
    }
}

void vector_free(string_vector *v) {
    for (size_t i = 0; i < v->size; i++) {
        free(v->data[i]);
    }
    free(v->data);
}
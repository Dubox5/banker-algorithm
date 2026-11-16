/*
 * banker.c - Algoritmo do Banqueiro (Banker's Algorithm)
 * Infraestrutura de Software - CESAR School
 *
 * Uso: ./banker <instancias_r0> <instancias_r1> ... <instancias_rN>
 * Entradas: customer.txt (maximo de cada cliente) e commands.txt (RQ, RL, *)
 * Saida:    result.txt
 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CUSTOMER_FILE "customer.txt"
#define COMMANDS_FILE "commands.txt"
#define RESULT_FILE   "result.txt"

typedef enum { CMD_RQ, CMD_RL, CMD_PRINT } CommandType;

typedef struct {
    CommandType type;
    int customer;
    int *resources;
} Command;

/* Estado global do banqueiro */
static int n_resources = 0;     /* NUMBER_OF_RESOURCES */
static int n_customers = 0;     /* NUMBER_OF_CUSTOMERS */
static int *available = NULL;   /* available[m] */
static int **maximum = NULL;    /* maximum[n][m] */
static int **allocation = NULL; /* allocation[n][m] */
static int **need = NULL;       /* need[n][m] */

static Command *commands = NULL;
static int n_commands = 0;

/* ------------------------------------------------------------------ */
/* Utilitarios                                                         */
/* ------------------------------------------------------------------ */

static void free_all(void) {
    int i;
    for (i = 0; i < n_customers; i++) {
        if (maximum) free(maximum[i]);
        if (allocation) free(allocation[i]);
        if (need) free(need[i]);
    }
    free(maximum);
    free(allocation);
    free(need);
    free(available);
    for (i = 0; i < n_commands; i++) free(commands[i].resources);
    free(commands);
}

/* Imprime a mensagem de erro, libera a memoria e encerra o programa */
static void fail(const char *fmt, const char *arg, int line) {
    fprintf(stderr, "Error: ");
    fprintf(stderr, fmt, arg);
    if (line > 0) fprintf(stderr, " (line %d)", line);
    fprintf(stderr, "\n");
    free_all();
    exit(EXIT_FAILURE);
}

static void *xmalloc(size_t size) {
    void *p = malloc(size);
    if (p == NULL) fail("%s", "out of memory", 0);
    return p;
}

/* Converte uma string inteira para um inteiro >= 0. Retorna 0 se invalida. */
static int parse_non_negative(const char *s, int *out) {
    char *end;
    long v;
    if (s == NULL || *s == '\0') return 0;
    if (*s == '+' || *s == '-') return 0; /* apenas digitos */
    errno = 0;
    v = strtol(s, &end, 10);
    if (errno != 0 || *end != '\0' || v < 0 || v > INT_MAX) return 0;
    *out = (int)v;
    return 1;
}

/* Remove '\n' e '\r' do final da linha (aceita arquivos criados no Windows) */
static void strip_newline(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) s[--len] = '\0';
}

static int is_blank(const char *s) {
    while (*s == ' ' || *s == '\t') s++;
    return *s == '\0';
}

static void print_vector(FILE *out, const int *v) {
    int j;
    for (j = 0; j < n_resources; j++) fprintf(out, j == 0 ? "%d" : " %d", v[j]);
}

/* ------------------------------------------------------------------ */
/* Leitura e validacao das entradas                                    */
/* ------------------------------------------------------------------ */

static void parse_arguments(int argc, char *argv[]) {
    int j;
    if (argc < 2)
        fail("%s", "usage: ./banker <instances of each resource> (e.g. ./banker 10 5 7)", 0);
    n_resources = argc - 1;
    available = xmalloc(sizeof(int) * n_resources);
    for (j = 0; j < n_resources; j++) {
        if (!parse_non_negative(argv[j + 1], &available[j]))
            fail("invalid argument '%s': resource instances must be a non-negative integer",
                 argv[j + 1], 0);
    }
}

static void read_customers(void) {
    FILE *f = fopen(CUSTOMER_FILE, "r");
    char *line = NULL;
    size_t cap = 0;
    int line_no = 0, blank_seen = 0, capacity = 0;

    if (f == NULL) fail("could not open file '%s'", CUSTOMER_FILE, 0);

    while (getline(&line, &cap, f) != -1) {
        char *tok, *save, *p;
        int *row, count = 0, commas = 0;

        line_no++;
        strip_newline(line);
        if (is_blank(line)) { blank_seen = 1; continue; } /* so aceito no final */
        if (blank_seen) {
            free(line); fclose(f);
            fail("invalid format in '%s': empty line between customers", CUSTOMER_FILE, line_no - 1);
        }

        /* conta virgulas para detectar campos vazios, ex.: "7,,3" ou "7,5,3," */
        for (p = line; *p; p++) if (*p == ',') commas++;

        row = xmalloc(sizeof(int) * n_resources);
        for (tok = strtok_r(line, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
            /* remove espacos em volta do numero */
            char *end;
            while (*tok == ' ' || *tok == '\t') tok++;
            end = tok + strlen(tok);
            while (end > tok && (end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';

            if (count >= n_resources) { count++; break; }
            if (!parse_non_negative(tok, &row[count])) {
                free(row); free(line); fclose(f);
                fail("invalid format in '%s': values must be non-negative integers separated by commas",
                     CUSTOMER_FILE, line_no);
            }
            count++;
        }
        if (count != n_resources || commas != n_resources - 1) {
            free(row); free(line); fclose(f);
            fail("number of resources in '%s' does not match the number of command line arguments",
                 CUSTOMER_FILE, line_no);
        }

        if (n_customers == capacity) {
            capacity = capacity ? capacity * 2 : 8;
            maximum = realloc(maximum, sizeof(int *) * capacity);
            allocation = realloc(allocation, sizeof(int *) * capacity);
            need = realloc(need, sizeof(int *) * capacity);
            if (!maximum || !allocation || !need) fail("%s", "out of memory", 0);
        }
        maximum[n_customers] = row;
        allocation[n_customers] = calloc(n_resources, sizeof(int));
        need[n_customers] = xmalloc(sizeof(int) * n_resources);
        if (!allocation[n_customers]) fail("%s", "out of memory", 0);
        memcpy(need[n_customers], row, sizeof(int) * n_resources);
        n_customers++;
    }
    free(line);
    fclose(f);

    if (n_customers == 0) fail("file '%s' is empty", CUSTOMER_FILE, 0);
}

static void read_commands(void) {
    FILE *f = fopen(COMMANDS_FILE, "r");
    char *line = NULL;
    size_t cap = 0;
    int line_no = 0, capacity = 0;

    if (f == NULL) fail("could not open file '%s'", COMMANDS_FILE, 0);

    while (getline(&line, &cap, f) != -1) {
        char *tok, *save;
        Command cmd;
        int count = 0;

        line_no++;
        strip_newline(line);
        if (is_blank(line)) continue;

        tok = strtok_r(line, " \t", &save);
        if (strcmp(tok, "*") == 0) {
            if (strtok_r(NULL, " \t", &save) != NULL) {
                free(line); fclose(f);
                fail("invalid format in '%s': '*' must be alone in its line", COMMANDS_FILE, line_no);
            }
            cmd.type = CMD_PRINT;
            cmd.customer = -1;
            cmd.resources = NULL;
        } else if (strcmp(tok, "RQ") == 0 || strcmp(tok, "RL") == 0) {
            cmd.type = (strcmp(tok, "RQ") == 0) ? CMD_RQ : CMD_RL;

            tok = strtok_r(NULL, " \t", &save);
            if (!parse_non_negative(tok, &cmd.customer)) {
                free(line); fclose(f);
                fail("invalid format in '%s': customer number must be a non-negative integer",
                     COMMANDS_FILE, line_no);
            }
            if (cmd.customer >= n_customers) {
                free(line); fclose(f);
                fail("invalid customer in '%s': customer does not exist in " CUSTOMER_FILE,
                     COMMANDS_FILE, line_no);
            }

            cmd.resources = xmalloc(sizeof(int) * n_resources);
            while ((tok = strtok_r(NULL, " \t", &save)) != NULL) {
                if (count >= n_resources) { count++; break; }
                if (!parse_non_negative(tok, &cmd.resources[count])) {
                    free(cmd.resources); free(line); fclose(f);
                    fail("invalid format in '%s': resources must be non-negative integers",
                         COMMANDS_FILE, line_no);
                }
                count++;
            }
            if (count != n_resources) {
                free(cmd.resources); free(line); fclose(f);
                fail("number of resources in '%s' does not match the number of command line arguments",
                     COMMANDS_FILE, line_no);
            }
        } else {
            free(line); fclose(f);
            fail("invalid format in '%s': unknown command (expected RQ, RL or *)",
                 COMMANDS_FILE, line_no);
        }

        if (n_commands == capacity) {
            capacity = capacity ? capacity * 2 : 16;
            commands = realloc(commands, sizeof(Command) * capacity);
            if (!commands) fail("%s", "out of memory", 0);
        }
        commands[n_commands++] = cmd;
    }
    free(line);
    fclose(f);
}

/* ------------------------------------------------------------------ */
/* Algoritmo do banqueiro                                              */
/* ------------------------------------------------------------------ */

/* Algoritmo de seguranca: retorna 1 se o estado atual e seguro */
static int is_safe(void) {
    int *work = xmalloc(sizeof(int) * n_resources);
    int *finish = calloc(n_customers, sizeof(int));
    int i, j, done = 0, progress = 1;

    if (!finish) { free(work); fail("%s", "out of memory", 0); }
    memcpy(work, available, sizeof(int) * n_resources);

    while (progress) {
        progress = 0;
        for (i = 0; i < n_customers; i++) {
            if (finish[i]) continue;
            for (j = 0; j < n_resources; j++)
                if (need[i][j] > work[j]) break;
            if (j == n_resources) { /* need[i] <= work */
                for (j = 0; j < n_resources; j++) work[j] += allocation[i][j];
                finish[i] = 1;
                done++;
                progress = 1;
            }
        }
    }
    free(work);
    free(finish);
    return done == n_customers;
}

static void request_resources(FILE *out, int c, const int *req) {
    int j;

    /* 1. Request <= Need ? */
    for (j = 0; j < n_resources; j++) {
        if (req[j] > need[c][j]) {
            fprintf(out, "The customer %d request ", c);
            print_vector(out, req);
            fprintf(out, " was denied because exceed its maximum allocation\n");
            return;
        }
    }
    /* 2. Request <= Available ? */
    for (j = 0; j < n_resources; j++) {
        if (req[j] > available[j]) {
            fprintf(out, "The resources ");
            print_vector(out, available);
            fprintf(out, " are not enough to customer %d request ", c);
            print_vector(out, req);
            fprintf(out, "\n");
            return;
        }
    }
    /* 3. Simula a alocacao e testa a seguranca */
    for (j = 0; j < n_resources; j++) {
        available[j] -= req[j];
        allocation[c][j] += req[j];
        need[c][j] -= req[j];
    }
    if (is_safe()) {
        fprintf(out, "Allocate to customer %d the resources ", c);
        print_vector(out, req);
        fprintf(out, "\n");
    } else { /* desfaz a simulacao */
        for (j = 0; j < n_resources; j++) {
            available[j] += req[j];
            allocation[c][j] -= req[j];
            need[c][j] += req[j];
        }
        fprintf(out, "The customer %d request ", c);
        print_vector(out, req);
        fprintf(out, " was denied because result in an unsafe state\n");
    }
}

static void release_resources(FILE *out, int c, const int *rel) {
    int j;
    for (j = 0; j < n_resources; j++) {
        if (rel[j] > allocation[c][j]) {
            fprintf(out, "The customer %d release ", c);
            print_vector(out, rel);
            fprintf(out, " was denied because exceed its allocation\n");
            return;
        }
    }
    for (j = 0; j < n_resources; j++) {
        available[j] += rel[j];
        allocation[c][j] -= rel[j];
        need[c][j] += rel[j];
    }
    fprintf(out, "Release from customer %d the resources ", c);
    print_vector(out, rel);
    fprintf(out, "\n");
}

static void print_state(FILE *out) {
    int i;
    fprintf(out, "MAXIMUM | ALLOCATION | NEED\n");
    for (i = 0; i < n_customers; i++) {
        print_vector(out, maximum[i]);
        fprintf(out, " | ");
        print_vector(out, allocation[i]);
        fprintf(out, " | ");
        print_vector(out, need[i]);
        fprintf(out, "\n");
    }
    fprintf(out, "AVAILABLE ");
    print_vector(out, available);
    fprintf(out, "\n");
}

int main(int argc, char *argv[]) {
    FILE *out;
    int k;

    /* Toda a entrada e validada antes de qualquer processamento */
    parse_arguments(argc, argv);
    read_customers();
    read_commands();

    out = fopen(RESULT_FILE, "w");
    if (out == NULL) fail("could not create file '%s'", RESULT_FILE, 0);

    for (k = 0; k < n_commands; k++) {
        switch (commands[k].type) {
            case CMD_RQ:    request_resources(out, commands[k].customer, commands[k].resources); break;
            case CMD_RL:    release_resources(out, commands[k].customer, commands[k].resources); break;
            case CMD_PRINT: print_state(out); break;
        }
    }

    fclose(out);
    free_all();
    return EXIT_SUCCESS;
}

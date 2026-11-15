#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    DD_KEY_UP = 1 << 0, DD_KEY_DOWN = 1 << 1, DD_KEY_LEFT = 1 << 2, DD_KEY_RIGHT = 1 << 3,
    DD_KEY_A = 1 << 4, DD_KEY_B = 1 << 5, DD_KEY_L = 1 << 6, DD_KEY_R = 1 << 7, DD_KEY_QUIT = 1 << 8,
};

typedef enum { DD_PASS, DD_FAIL, DD_UNKNOWN } DdState;

typedef struct {
    const char *title;
    DdState state;
    char detail[160];
} DdCheck;

#define DD_MAX_CHECKS 8
#define DD_RUNTIME_BYTES (3ull << 30)

typedef struct {
    DdCheck checks[DD_MAX_CHECKS];
    int check_count;
    bool runtime_can_start;
    char vkinfo[4096];
    char firmware[64];
    char gpu[128];
} DdHost;

void dd_host_init(void);
void dd_host_exit(void);
bool dd_host_running(void);
uint32_t dd_host_keys_down(void);
void dd_host_probe(DdHost *host);
const void *dd_host_font(size_t *size);
const char *dd_host_asset(const char *name, char *path, size_t cap);

void dd_vkinfo_query(char *out, size_t cap);
const char *dd_report_get(const char *report, const char *key, char *value, size_t cap);

// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Zygmunt Krynicki

#pragma once

#include <errno.h>
#include <stdint.h>
#include <stdio.h>

struct errslot;
struct errslot_info;
struct errslot_domain;
typedef int errslot_index_t;

// errslot_make allocates a new error slot and returns a negative index.
struct errslot_info errslot_make(const struct errslot_domain *domain);
// errslot_unref dellocates an error slot.
void errslot_unref(errslot_index_t idx);
// errslot_at returns a pointer to the slot at the given negative index.
struct errslot *errslot_at(errslot_index_t idx);
// errslot_print writes the error slot to the given file.
int errslot_print(FILE *f, errslot_index_t idx);
// errslot_domain_errno returns the domain for system errors based on errno.
const struct errslot_domain *errslot_domain_errno(void);
// errslot_domain_plain returns the domain for errors related to command line arguments.
const struct errslot_domain *errslot_domain_plain(void);
// errslot_domain_TODO returns the domain for errors without a dedicated domain.
const struct errslot_domain *errslot_domain_TODO(void);

struct errslot
{
    const struct errslot_domain *es_domain;
    int es_value;
    // Static message that does not need to be freed.
    const char *es_msg;
    // Source code location.
    const char *es_function;
    const char *es_filename;
    int es_lineno;
    errslot_index_t es_cause;
    // Data attached to the slot and managed by the domain.
    void *es_data;
};

struct errslot_domain
{
    void (*esd_free)(void *data);
    int (*esd_print)(FILE *f, const struct errslot *slot, int depth);
};

struct errslot_info
{
    struct errslot *es_ptr;
    errslot_index_t es_index; // Always negative
};

static inline int errslot_make_inline(const struct errslot_domain *domain, int value, const char *msg,
                                      errslot_index_t cause, const char *function, const char *filename, int lineno)
{
    struct errslot_info info = errslot_make(domain);
    info.es_ptr->es_value = value;
    info.es_ptr->es_msg = msg;
    info.es_ptr->es_cause = cause;
    info.es_ptr->es_function = function;
    info.es_ptr->es_filename = filename;
    info.es_ptr->es_lineno = lineno;
    return info.es_index;
}

#define errslot_plain(msg) errslot_make_inline(errslot_domain_plain(), 0, (msg), 0, __FUNCTION__, __FILE__, __LINE__)
#define errslot_plain_cause(msg, cause)                                                                                \
    errslot_make_inline(errslot_domain_plain(), 0, (msg), (cause), __FUNCTION__, __FILE__, __LINE__)

#define errslot_errno(msg)                                                                                             \
    errslot_make_inline(errslot_domain_errno(), errno, (msg), 0, __FUNCTION__, __FILE__, __LINE__)

#define errslot_TODO(msg) errslot_make_inline(errslot_domain_TODO(), 0, (msg), 0, __FUNCTION__, __FILE__, __LINE__)
#define errslot_TODO_cause(msg, cause)                                                                                 \
    errslot_make_inline(errslot_domain_TODO(), 0, (msg), (cause), __FUNCTION__, __FILE__, __LINE__)

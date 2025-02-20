// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Zygmunt Krynicki

#include "errslot.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define ERRSLOT_MAX 32

static struct errslot errslot_main[ERRSLOT_MAX];

struct errslot_info errslot_make(const struct errslot_domain *domain)
{
    struct errslot empty = {0};
    struct errslot_info info;

    if (domain == NULL)
    {
        domain = errslot_domain_TODO();
    }

    for (int slot_index = 0; slot_index < ERRSLOT_MAX; slot_index++)
    {
        info.es_ptr = &errslot_main[slot_index];
        if (memcmp(info.es_ptr, &empty, sizeof(struct errslot)) == 0)
        {
            info.es_ptr->es_domain = domain;
            info.es_index = -slot_index - 1;
            return info;
        }
    }

    abort();
}

struct errslot *errslot_at(int slot_index)
{
    if (slot_index < 0 && slot_index > -ERRSLOT_MAX - 1)
        return &errslot_main[-slot_index - 1];

    return NULL;
}

void errslot_unref(int idx)
{
    struct errslot *slot = errslot_at(idx);
    if (slot == NULL)
        return;

    if (slot->es_data != NULL && slot->es_domain != NULL && slot->es_domain->esd_free != NULL)
        slot->es_domain->esd_free(slot->es_data);

    if (slot->es_cause != 0)
        errslot_unref(slot->es_cause);

    memset(slot, 0, sizeof *slot);
}

static int errslot_indent(int depth)
{
    int n = 0;
    for (int i = 0; i <= depth; ++i)
        n += fprintf(stderr, "  ");
    return n;
}

static int errslot_print_depth(FILE *f, errslot_index_t idx, int depth)
{
    struct errslot *slot = errslot_at(idx);
    if (slot != NULL && slot->es_domain != NULL && slot->es_domain->esd_print != NULL)
        return slot->es_domain->esd_print(f, slot, depth);
    return 0;
}

int errslot_print(FILE *f, errslot_index_t idx)
{
    return errslot_print_depth(f, idx, 0);
}

static int errslot_print_errno(FILE *f, const struct errslot *slot, int depth)
{
    int n = 0;
    if (slot->es_msg)
    {
        n += fputs(slot->es_msg, f);
        n += fputs(": ", f);
    }
    n += fprintf(f, "%s (%d)", strerror(slot->es_value), slot->es_value);
    if (slot->es_cause != 0)
    {
        n += fputs(": ", f);
        n += errslot_print_depth(f, slot->es_cause, depth + 1);
    }
    if (depth == 0)
        n += fputs("\n", f);
    return n;
}

const struct errslot_domain *errslot_domain_errno(void)
{
    static struct errslot_domain domain = {
        .esd_print = errslot_print_errno,
    };
    return &domain;
}

static int errslot_print_plain(FILE *f, const struct errslot *slot, int depth)
{
    int n = 0;
    n += fputs(slot->es_msg ? slot->es_msg : "???", f);
    if (slot->es_cause != 0)
    {
        n += fputs(": ", f);
        n += errslot_print_depth(f, slot->es_cause, depth + 1);
    }
    if (depth == 0)
        n += fputs("\n", f);
    return n;
}

const struct errslot_domain *errslot_domain_plain(void)
{
    static struct errslot_domain domain = {
        .esd_print = errslot_print_plain,
    };
    return &domain;
}

static int errslot_print_TODO(FILE *f, const struct errslot *slot, int depth)
{
    if (slot == NULL)
        return 0;
    int n = 0;
    if (depth == 0)
        n += fprintf(f, "An error had occured:\n");
    if (slot->es_msg != NULL)
    {
        n += errslot_indent(depth);
        n += fprintf(f, "Message: %s\n", slot->es_msg);
    }
    if (slot->es_filename != NULL || slot->es_lineno != 0)
    {
        n += errslot_indent(depth);
        n += fprintf(f, "Source code location: %s:%d\n", slot->es_filename, slot->es_lineno);
    }
    if (slot->es_function != NULL)
    {
        n += errslot_indent(depth);
        n += fprintf(f, "Function name: %s\n", slot->es_function);
    }
    if (slot->es_cause != 0)
    {
        n += errslot_indent(depth);
        n += fprintf(f, "Caused by error:\n");
        n += errslot_print_depth(f, slot->es_cause, depth + 1);
    }

    return n;
}

const struct errslot_domain *errslot_domain_TODO(void)
{
    static struct errslot_domain domain = {
        .esd_print = errslot_print_TODO,
    };
    return &domain;
}

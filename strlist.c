// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Zygmunt Krynicki

#include "strlist.h"

#include <string.h>

int strlist_each(const char *list, char sep, int (*func)(void *p, const char *item, size_t item_len), void *p)
{
    const char *curr = list;
    while (curr != NULL && *curr != '\0')
    {
        const char *next = strchr(curr, sep);
        if (next == NULL)
            next = strchr(curr, '\0');

        int ret = func(p, curr, next - curr);
        if (ret < 0)
            return ret;

        curr = next;
        if (*curr == sep)
            curr++;
    }

    return 0;
}

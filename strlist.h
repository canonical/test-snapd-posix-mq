// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Zygmunt Krynicki

#pragma once

#include <stddef.h>

int strlist_each(const char *list, char sep, int (*func)(void *p, const char *item, size_t item_len), void *p);

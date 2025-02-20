// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Zygmunt Krynicki

#include <fcntl.h>
#include <mqueue.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

#include "errslot.h"
#include "strlist.h"

static int visit_open_flag(void *data, const char *item, size_t item_len)
{
    int *flag = data;

    if (strncmp(item, "read-only", item_len) == 0)
    {
        *flag &= ~O_RDWR;
        *flag |= O_RDONLY;
    }
    else if (strncmp(item, "write-only", item_len) == 0)
    {
        *flag &= ~O_RDWR;
        *flag |= O_WRONLY;
    }
    else if (strncmp(item, "read-write", item_len) == 0)
        *flag |= O_RDWR;
    else if (strncmp(item, "create", item_len) == 0)
        *flag |= O_CREAT;
    else if (strncmp(item, "excl", item_len) == 0)
        *flag |= O_EXCL;
    else if (strncmp(item, "nonblock", item_len) == 0)
        *flag |= O_NONBLOCK;
    else
        return errslot_plain("unknown open flag, expected one of: read-only, write-only, read-write, excl or nonblock");

    return 0;
}

static errslot_index_t visit_mq_attr(void *data, const char *item, size_t item_len)
{
    struct mq_attr *attr = data;

    if (strncmp(item, "max-size=", MIN(item_len, strlen("max-size="))) == 0)
    {
        if (sscanf(item + strlen("max-size="), "%ld", &attr->mq_msgsize) != 1)
            return errslot_plain("cannot parse maximum message size");
    }
    else if (strncmp(item, "max-count=", MIN(item_len, strlen("max-count="))) == 0)
    {
        if (sscanf(item + strlen("max-count="), "%ld", &attr->mq_maxmsg) != 1)
            return errslot_plain("cannot parse maximum message count");
    }
    else
        return errslot_plain("unrecognized attribute, expected one of max-size=N or max-count=N");

    return 0;
}

struct cmd_open_args
{
    const char *name;
    int flag;
};

struct cmd_create_args
{
    mode_t mode;
    struct mq_attr attr;
};

static const char *consume_arg(int *argcp, char ***argvp)
{
    const char *arg = *argvp[0];
    (*argcp)--;
    (*argvp)++;
    return arg;
}

static errslot_index_t parse_queue_name(const char **name, int *argcp, char ***argvp)
{
    if (*argcp == 0)
        return errslot_plain("insufficient arguments: expected queue name");
    *name = consume_arg(argcp, argvp);
    if (*name == NULL || **name != '/')
        return errslot_plain("queue name must start with '/'");
    return 0;
}

static errslot_index_t parse_open_args(struct cmd_open_args *args, int *argcp, char ***argvp)
{
    // Name
    errslot_index_t err = parse_queue_name(&args->name, argcp, argvp);
    if (err < 0)
        return err;
    // Flag
    if (*argcp == 0)
        return errslot_plain("insufficient arguments: only queue name given, expected open flag list");
    const char *flag_list = consume_arg(argcp, argvp);
    args->flag = 0;
    err = strlist_each(flag_list, ',', visit_open_flag, &args->flag);
    if (err < 0)
        return errslot_plain_cause("cannot parse open flag list", err);

    return 0;
}

static errslot_index_t cmd_open(int argc, char **argv)
{
    if (argc == 0)
        return errslot_plain("usage: mqctl open NAME OPEN-FLAG-LIST");
    struct cmd_open_args args;
    errslot_index_t err = parse_open_args(&args, &argc, &argv);
    if (err < 0)
        return errslot_plain_cause("cannot parse arguments", err);
    if (args.flag & O_CREAT)
        return errslot_plain("Use the create command to create a message queue");
    if (argc > 0)
        return errslot_plain("too many arguments");

    mqd_t mqd = mq_open(args.name, args.flag);
    if (mqd == (mqd_t)-1)
        return errslot_errno("mq_open failed");
    printf("mq_open did not fail\n");

    if (mq_close(mqd) == -1)
        return errslot_errno("mq_close failed");
    printf("mq_close did not fail\n");

    return 0;
}

static errslot_index_t parse_create_args(struct cmd_create_args *create_args, int *argcp, char ***argvp)
{
    // Mode
    if (*argcp == 0)
        return errslot_plain("insufficient arguments: only queue name and flags given, expected mode");
    const char *mode_str = consume_arg(argcp, argvp);
    create_args->mode = 0;
    if (sscanf(mode_str, "%o", &create_args->mode) != 1)
        return errslot_plain("cannot parse queue creation mode");
    // Attributes
    if (*argcp == 0)
        return errslot_plain("insufficient arguments: only queue name, flags and "
                             "creation mode given, expected attributes");
    const char *attr_list = consume_arg(argcp, argvp);
    create_args->attr.mq_maxmsg = 0;
    create_args->attr.mq_msgsize = 0;
    errslot_index_t err = strlist_each(attr_list, ',', visit_mq_attr, &create_args->attr);
    if (err < 0)
        return errslot_plain_cause("cannot parse attribute list", err);

    return 0;
}

static errslot_index_t cmd_create(int argc, char **argv)
{
    if (argc == 0)
        return errslot_plain("usage: mqctl create NAME OPEN-FLAG-LIST MODE ATTR-LIST");

    struct cmd_open_args open_args;
    errslot_index_t err = parse_open_args(&open_args, &argc, &argv);
    if (err < 0)
        return err;

    struct cmd_create_args create_args;
    err = parse_create_args(&create_args, &argc, &argv);
    if (err < 0)
        return errslot_plain_cause("cannot parse arguments", err);
    if (argc > 6)
        return errslot_plain("too many arguments");

    open_args.flag |= O_CREAT;
    if (create_args.attr.mq_maxmsg == 0)
        return errslot_plain("maximum number of messages must be greater than zero");
    if (create_args.attr.mq_msgsize == 0)
        return errslot_plain("maximum message size must be greater than zero");

    mqd_t mqd = mq_open(open_args.name, open_args.flag, create_args.mode, &create_args.attr);
    if (mqd == (mqd_t)-1)
        return errslot_errno("mq_open failed");
    printf("mq_open did not fail\n");

    if (mq_close(mqd) == -1)
        return errslot_errno("mq_close failed");
    printf("mq_close did not fail\n");

    return 0;
}

static errslot_index_t cmd_getattr(int argc, char **argv)
{
    if (argc == 0)
        return errslot_plain("usage: mqctl getattr NAME OPEN-FLAG-LIST");
    struct cmd_open_args args;
    errslot_index_t err = parse_open_args(&args, &argc, &argv);
    if (err < 0)
        return errslot_plain_cause("cannot parse arguments", err);
    if (args.flag & O_CREAT)
        return errslot_plain("Use the create command to create a message queue");
    if (argc > 0)
        return errslot_plain("too many arguments");

    mqd_t mqd = mq_open(args.name, args.flag);
    if (mqd == (mqd_t)-1)
        return errslot_errno("mq_open failed");
    printf("mq_open did not fail\n");

    struct mq_attr attr;
    if (mq_getattr(mqd, &attr) == -1)
        return errslot_errno("mq_getattr failed");
    printf("mq_getattr did not fail\n");

    printf("mq_flags: %ld%s\n", attr.mq_flags, attr.mq_flags & O_NONBLOCK ? " (non-blocking)" : "");
    printf("mq_maxmsg: %ld\n", attr.mq_maxmsg);
    printf("mq_msgsize: %ld\n", attr.mq_msgsize);
    printf("mq_curmsgs: %ld\n", attr.mq_curmsgs);

    if (mq_close(mqd) == -1)
        return errslot_errno("mq_close failed");
    printf("mq_close did not fail\n");

    return 0;
}

static errslot_index_t cmd_setattr(int argc, char **argv)
{
    if (argc == 0)
        return errslot_plain("usage: mqctl setattr NAME OPEN-FLAG-LIST [nonblock]");
    struct cmd_open_args args;
    errslot_index_t err = parse_open_args(&args, &argc, &argv);
    if (err < 0)
        return errslot_plain_cause("cannot parse arguments", err);
    if (args.flag & O_CREAT)
        return errslot_plain("Use the create command to create a message queue");
    struct mq_attr attr = {.mq_flags = 0};
    if (argc > 0)
    {
        const char *opt = consume_arg(&argc, &argv);
        if (strcmp(opt, "nonblock") == 0)
            attr.mq_flags |= O_NONBLOCK;
        else
            return errslot_plain("expected nonblock or no more arguments");
    }
    if (argc > 0)
        return errslot_plain("too many arguments");

    mqd_t mqd = mq_open(args.name, args.flag);
    if (mqd == (mqd_t)-1)
        return errslot_errno("mq_open failed");
    printf("mq_open did not fail\n");

    if (mq_setattr(mqd, &attr, NULL) == -1)
        return errslot_errno("mq_setattr failed");
    printf("mq_setattr did not fail\n");

    if (mq_close(mqd) == -1)
        return errslot_errno("mq_close failed");
    printf("mq_close did not fail\n");

    return 0;
}

static void cmd_notify_action(int signum, siginfo_t *info, __attribute__((unused)) void *context)
{
    printf("Pause interrupted by signal %d, sender pid %d, sender uid %d\n", signum, info->si_pid, info->si_uid);
}

static errslot_index_t cmd_notify(int argc, char **argv)
{
    if (argc == 0)
        return errslot_plain("usage: mqctl notify NAME OPEN-FLAG-LIST");

    struct cmd_open_args args;
    errslot_index_t err = parse_open_args(&args, &argc, &argv);
    if (err < 0)
        return errslot_plain_cause("cannot parse arguments", err);
    if (args.flag & O_CREAT)
        return errslot_plain("Use the create command to create a message queue");
    if (argc > 0)
        return errslot_plain("too many arguments");

    mqd_t mqd = mq_open(args.name, args.flag);
    if (mqd == (mqd_t)-1)
        return errslot_errno("mq_open failed");
    printf("mq_open did not fail\n");

    struct sigevent sev = {
        .sigev_notify = SIGEV_SIGNAL,
        .sigev_signo = SIGUSR1,
    };

    struct sigaction act = {
        .sa_flags = SA_RESETHAND | SA_SIGINFO,
        .sa_sigaction = cmd_notify_action,
    };

    if (sigaction(SIGUSR1, &act, NULL) < 0)
        return errslot_errno("sigaction failed");

    if (mq_notify(mqd, &sev) < 0)
        return errslot_errno("mq_notify failed");
    printf("mq_notify did not fail\n");

    (void)pause();
    printf("Pause returned\n");

    if (mq_close(mqd) == -1)
        return errslot_errno("mq_close failed");
    printf("mq_close did not fail\n");

    return 0;
}

static errslot_index_t cmd_recv(int argc, char **argv)
{
    if (argc == 0)
        return errslot_plain("usage: mqctl recv NAME OPEN-FLAG-LIST");

    struct cmd_open_args args;
    errslot_index_t err = parse_open_args(&args, &argc, &argv);
    if (err < 0)
        return errslot_plain_cause("cannot parse arguments", err);
    if (args.flag & O_CREAT)
        return errslot_plain("Use the create command to create a message queue");
    if (argc > 0)
        return errslot_plain("too many arguments");

    mqd_t mqd = mq_open(args.name, args.flag);
    if (mqd == (mqd_t)-1)
        return errslot_errno("mq_open failed");
    printf("mq_open did not fail\n");

    char buf[1024];
    unsigned int prio = 0;
    ssize_t len = mq_receive(mqd, buf, sizeof(buf), &prio);
    if (len == -1)
        return errslot_errno("mq_receive failed");
    printf("mq_receive did not fail\n");

    printf("Received message with priority %u: %.*s\n", prio, (int)len, buf);

    if (mq_close(mqd) == -1)
        return errslot_errno("mq_close failed");
    printf("mq_close did not fail\n");

    return 0;
}

static errslot_index_t cmd_send(int argc, char **argv)
{
    if (argc == 0)
        return errslot_plain("usage: mqctl send NAME OPEN-FLAG-LIST MESSAGE PRIORITY");

    struct cmd_open_args args;
    errslot_index_t err = parse_open_args(&args, &argc, &argv);
    if (err < 0)
        return errslot_plain_cause("cannot parse arguments", err);
    if (args.flag & O_CREAT)
        return errslot_plain("Use the create command to create a message queue");

    if (argc == 0)
        return errslot_plain("insufficient arguments: only queue name and flags given"
                             ", expected message and priority");
    const char *msg = consume_arg(&argc, &argv);

    if (argc == 0)
        return errslot_plain("insufficient arguments: only queue name, flags and message given"
                             ", expected priority");
    const char *prio_str = consume_arg(&argc, &argv);
    unsigned int prio = 0;
    if (sscanf(prio_str, "%u", &prio) != 1)
        return errslot_plain("cannot parse message priority");

    if (argc > 0)
        return errslot_plain("too many arguments");

    mqd_t mqd = mq_open(args.name, args.flag);
    if (mqd == (mqd_t)-1)
        return errslot_errno("mq_open failed");
    printf("mq_open did not fail\n");

    if (mq_send(mqd, msg, strlen(msg), prio) < 0)
        return errslot_errno("mq_send failed");
    printf("mq_send did not fail\n");

    printf("Sent message: %s\n", msg);
    if (mq_close(mqd) == -1)
        return errslot_errno("mq_close failed");
    printf("mq_close did not fail\n");

    return 0;
}

static errslot_index_t cmd_unlink(int argc, char **argv)
{
    const char *name = NULL;
    if (argc == 0)
        return errslot_plain("usage: mqctl unlink NAME");
    errslot_index_t err = parse_queue_name(&name, &argc, &argv);
    if (err < 0)
        return err;
    if (argc > 0)
        return errslot_plain("too many arguments");

    if (mq_unlink(name) < 0)
        return errslot_errno("mq_unlink failed");
    printf("mq_unlink did not fail\n");

    return 0;
}

static errslot_index_t errslot_main(int argc, char **argv)
{
    (void)consume_arg(&argc, &argv); // Eat program name.
    if (argc == 0)
        return errslot_plain("usage: mqctl {create,open,recv,send,notify,getattr,setattr,unlink} ...");
    const char *cmd = consume_arg(&argc, &argv);

    if (strcmp(cmd, "open") == 0)
        return cmd_open(argc, argv);
    else if (strcmp(cmd, "create") == 0)
        return cmd_create(argc, argv);
    else if (strcmp(cmd, "recv") == 0)
        return cmd_recv(argc, argv);
    else if (strcmp(cmd, "send") == 0)
        return cmd_send(argc, argv);
    else if (strcmp(cmd, "notify") == 0)
        return cmd_notify(argc, argv);
    else if (strcmp(cmd, "getattr") == 0)
        return cmd_getattr(argc, argv);
    else if (strcmp(cmd, "setattr") == 0)
        return cmd_setattr(argc, argv);
    else if (strcmp(cmd, "unlink") == 0)
        return cmd_unlink(argc, argv);
    else
        return errslot_plain("unknown command");
}

int main(int argc, char **argv)
{
    errslot_index_t err = errslot_main(argc, argv);
    if (err != 0)
    {
        errslot_print(stderr, err);
        errslot_unref(err);
        return 1;
    }

    return 0;
}

#pragma once
#ifndef BECOME_DAEMON_H
#define BECOME_DAEMON_H

#define D_NO_FILE_CLOSE 0b01
#define D_NO_STD_TO_NULL 0b10
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <stdbool.h>
#include <sys/stat.h>
// Turns process into daemon.
void become_daemon(bool use_flags);
#define _POSIX_C_SOURCE 199309L // for POSIX timers

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <syslog.h>

void handler(int signo, siginfo_t * info, void * context) {
    printf("Timer expired!\n");
    syslog(LOG_NOTICE,"Handler hit.");
    // Perform the action you want here
}

void handler1(int signo) {
    printf("Timer expired!\n");
    syslog(LOG_NOTICE,"Handler hit.");
    // Perform the action you want here
}
int main() {
    timer_t timerid;
    struct sigaction act;

    // Set up signal handler
    act.sa_flags = SA_SIGINFO;
    act.sa_handler = &handler1;
    act.sa_sigaction = &handler;
    sigemptyset(&act.sa_mask);
    if (sigaction(SIGRTMIN, &act, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    struct sigevent sev;
    // Set up timer
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN;
    sev.sigev_value.sival_ptr = &timerid;
    if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1) {
        perror("timer_create");
        exit(EXIT_FAILURE);
    }
    printf("%d\n", timerid);
    struct itimerspec its;
    // Set timer expiration time
    its.it_value.tv_sec = time(NULL) + 5; // 5 seconds from now
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;

    // Start the timer
    if (timer_settime(timerid, 0, &its, NULL) == -1) {
        perror("timer_settime");
        exit(EXIT_FAILURE);
    }
    printf("Here\n");
    // Wait forever
    while (1) {
        sleep(1);
    }

    return 0;
}
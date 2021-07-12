#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "my_signal.h"
#include "my_socket.h"
#include "get_num.h"
#include "logUtil.h"
#include "set_timer.h"

int debug = 0;
int enable_quick_ack = 0;
int set_so_sndbuf_size = 0;
volatile sig_atomic_t has_alrm = 0;

int print_result(struct timeval start, struct timeval stop, int so_snd_buf, unsigned long long send_bytes)
{
    struct timeval diff;
    double elapse;

    timersub(&stop, &start, &diff);
    fprintfwt(stderr, "server-ntimes: SO_SNDBUF: %d (final)\n", so_snd_buf);
    elapse = diff.tv_sec + 0.000001*diff.tv_usec;
    fprintfwt(stderr, "server-ntimes: %.3f MB/s ( %lld bytes %ld.%06ld sec )\n",
        (double) send_bytes / elapse  / 1024.0 / 1024.0,
        send_bytes, diff.tv_sec, diff.tv_usec);

    return 0;
}

void sig_alrm()
{
    return;
}

int child_proc(int connfd, int bufsize, int n_write, int sleep_usec, char *interval_sec_str, int tcp_nodelay)
{
    int n;
    unsigned char *buf;
    struct timeval start, stop;

    buf = malloc(bufsize);
    if (buf == NULL) {
        err(1, "malloc sender buf in child_proc");
    }

    pid_t pid = getpid();
    fprintfwt(stderr, "server-ntimes: pid: %d\n", pid);
    int so_snd_buf = get_so_sndbuf(connfd);
    fprintfwt(stderr, "server-ntimes: SO_SNDBUF: %d (init)\n", so_snd_buf);

    my_signal(SIGALRM, sig_alrm);
    gettimeofday(&start, NULL);
    struct timeval interval;
    if (conv_str2timeval(interval_sec_str, &interval) < 0) {
        exit(1);
    }

    set_timer(interval.tv_sec, interval.tv_usec, interval.tv_sec, interval.tv_usec);

    if (tcp_nodelay) {
        if (set_so_nodelay(connfd) < 0) {
            warnx("set_so_nodelay failed");
        }
    }

    for ( ; ; ) {
        for (int i = 0; i < n_write; ++i) {
            n = write(connfd, buf, bufsize);
            if (n < 0) {
                if (errno == ECONNRESET) {
                    fprintfwt(stderr, "server-ntimes: connection reset by client\n");
                    exit(0);
                }
                else if (errno == EPIPE) {
                    fprintfwt(stderr, "server-ntimes: connection reset by client\n");
                    exit(0);
                }
                else {
                    err(1, "write");
                }
            }
            else if (n == 0) {
                warnx("write returns 0");
                exit(0);
            }
            if (sleep_usec > 0) {
                usleep(sleep_usec);
            }
        }

        wait_alarm();
    }

    return 0;
}

void sig_chld(int signo)
{
    pid_t pid;
    int   stat;

    while ( (pid = waitpid(-1, &stat, WNOHANG)) > 0) {
        ;
    }
    return;
}

int usage(void)
{
    char *msg =
"Usage: server [-b bufsize (1460)] [-r rate]\n"
"-b bufsize:    one send size (may add k for kilo, m for mega)\n"
"-p port:       port number (1234)\n"
"-N:            set TCP_NODELAY\n"
"-i interval_sec: default 1 seconds\n"
"-s sleep_usec:   sleep usec between each write()'s\n"
"-n write_count:  write() n times.  Default 10\n";

    fprintf(stderr, msg);

    return 0;
}

int main(int argc, char *argv[])
{
    int port = 1234;
    pid_t pid;
    struct sockaddr_in remote;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int listenfd;
    int c;
    int bufsize = 1460;
    int tcp_nodelay = 0;
    char *interval_sec_str = "1.0";
    int n_write = 10;
    int sleep_usec = 0;

    while ( (c = getopt(argc, argv, "b:dhi:Nn:p:r:")) != -1) {
        switch (c) {
            case 'b':
                bufsize = get_num(optarg);
                break;
            case 'd':
                debug += 1;
                break;
            case 'h':
                usage();
                exit(0);
            case 'i':
                interval_sec_str = optarg;
                break;
            case 'N':
                tcp_nodelay = 1;
                break;
            case 'n':
                n_write = strtol(optarg, NULL, 0);
                break;
            case 'p':
                port = strtol(optarg, NULL, 0);
                break;
            case 's':
                sleep_usec = strtol(optarg, NULL, 0);
            default:
                break;
        }
    }
    argc -= optind;
    argv += optind;

    my_signal(SIGCHLD, sig_chld);
    my_signal(SIGPIPE, SIG_IGN);

    listenfd = tcp_listen(port);
    if (listenfd < 0) {
        errx(1, "tcp_listen");
    }

    for ( ; ; ) {
        int connfd = accept(listenfd, (struct sockaddr *)&remote, &addr_len);
        if (connfd < 0) {
            err(1, "accept");
        }
        
        pid = fork();
        if (pid == 0) { //child
            if (close(listenfd) < 0) {
                err(1, "close listenfd");
            }
            if (child_proc(connfd, bufsize, n_write, sleep_usec, interval_sec_str, tcp_nodelay) < 0) {
                errx(1, "child_proc");
            }
            exit(0);
        }
        else { // parent
            if (close(connfd) < 0) {
                err(1, "close for accept socket of parent");
            }
        }
    }
        
    return 0;
}

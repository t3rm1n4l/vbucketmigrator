#include <string>
#include <sstream>
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include "stats.h"

#define STAT_SOCK_PATH  "/var/tmp/vbs/vbm.sock."

VbStats* VbStats::stats = NULL;

void VbStats::init_stats(std::vector<uint16_t> &buckets) {

    std::vector<uint16_t>::iterator iter;
    for (iter = buckets.begin(); iter != buckets.end(); ++iter) {
        VbCount vbc = {0 ,0};
        vbstats[*iter] = vbc;
    }
}

VbStats * VbStats::instance() {
    if (!stats) {
        stats = new VbStats();
    }
    return stats;
}

void VbStats::update_sent_stats(uint16_t vb) {
    std::map<uint16_t, VbCount>::iterator iter;
    if ((iter = vbstats.find(vb)) != vbstats.end()) {
        VbCount *vbc = &(iter->second);
        vbc->sent++;
    }
}

void VbStats::update_rcvd_stats(uint16_t vb) {
    std::map<uint16_t, VbCount>::iterator iter;
    if ((iter = vbstats.find(vb)) != vbstats.end()) {
        VbCount *vbc = &(iter->second);
        vbc->rcvd++;
    }
}

std::string VbStats::get_stats_str() {
    std::stringstream sstm;
    std::map<uint16_t, VbCount>::iterator iter;

    for (iter = vbstats.begin(); iter != vbstats.end(); ++iter) {
        uint16_t vbid = iter->first;
        VbCount *vbc = &(iter->second);
        sstm << "vb:" << vbid  << " rcvd:" << vbc->rcvd << " sent:" << vbc->sent << "\n";
    } 

    return  sstm.str();
}


static int new_socket_unix(void) {
    int sfd;

    if ((sfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket()");
        return -1;
    }

    /*
    int flags;
    if ((flags = fcntl(sfd, F_GETFL, 0)) < 0 ||
            fcntl(sfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("setting O_NONBLOCK");
        close(sfd);
        return -1;
    }
    */
    return sfd;
}

int server_socket_unix(const char *path, int access_mask) {
    int sfd;
    struct linger ling = {0, 0};
    struct sockaddr_un addr;
    struct stat tstat;
    int flags =1;
    int old_umask;

    if (!path) {
        return -1;
    }

    if ((sfd = new_socket_unix()) == -1) {
        return -1;
    }

    /*
     * Clean up a previous socket file if we left it around
     */
    if (lstat(path, &tstat) == 0) {
        if (S_ISSOCK(tstat.st_mode))
            unlink(path);
    }

    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (void *)&flags, sizeof(flags));
    setsockopt(sfd, SOL_SOCKET, SO_KEEPALIVE, (void *)&flags, sizeof(flags));
    setsockopt(sfd, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling));

    /*
     * the memset call clears nonstandard fields in some impementations
     * that otherwise mess things up.
     */
    memset(&addr, 0, sizeof(addr));

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    old_umask = umask( ~(access_mask&0777));
    if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind()");
        close(sfd);
        umask(old_umask);
        return -1;
    }
    umask(old_umask);
    if (listen(sfd, 10) == -1) {
        perror("listen()");
        close(sfd);
        return -1;
    }

   return sfd;
}

void * stats_thread (void *arg) {

    std::string stat_sock_path(STAT_SOCK_PATH);
    if (arg != NULL) {
        std::string host((char *)arg);
        host.erase(host.find_first_of(':'));
        stat_sock_path.append(host);
    }

    int sfd = server_socket_unix(stat_sock_path.c_str(), 0700);
    
    while(1) {
        struct sockaddr_un claddr;
        socklen_t cllen = sizeof(claddr);
        char buffer[256];
        int newsockfd;
        int n;

        printf("Listening on sock %d \n", sfd);
        newsockfd = accept(sfd, 
                (struct sockaddr *) &claddr, &cllen);
        if (newsockfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("errno %d \n", errno);
                sleep(1);
                continue;
            }
            else {
                printf("ERROR on accept errno %d \n", errno);
                continue;
            }
        } 

        printf("Accepted!");
        n = read(newsockfd, buffer, 255);
        if (n < 0) {
            close(newsockfd);
        }
        else if (strncmp(buffer, "stats", sizeof("stats")-1) != 0) {
            write(newsockfd, "Unknown command", strlen("Unknown command"));
            close(newsockfd);
        }
        else {
            std::string stats_str = VbStats::instance()->get_stats_str();
            write(newsockfd, stats_str.c_str(), stats_str.length());
            close(newsockfd);
        }
    }
    return NULL;
}


#ifndef _EPOLL_H
#define _EPOLL_H

#include <stdlib.h>
#include <sys/epoll.h>

struct epoll_socket;
struct epoll_socket_spec;
typedef struct epoll_socket epoll_socket_t;
typedef struct epoll_socket_spec epoll_socket_spec_t;

struct ddhcp_config;
typedef struct ddhcp_config ddhcp_config;

typedef int (*ddhcpd_socket_event_t)(epoll_socket_t *, ddhcp_config *);

struct epoll_socket_spec {
  const char *ifname;
  ddhcpd_socket_event_t setup;
  ddhcpd_socket_event_t epollin;
  ddhcpd_socket_event_t epollhup;
  ddhcpd_socket_event_t ifindex_changed;
  uint32_t events;
};

struct epoll_socket {
  int socket;
  unsigned ifindex;
  epoll_socket_spec_t *spec;
  void *ctx;
};

#include "types.h"

/**
 * Initialize epoll socket.
 */
void epoll_init(ddhcp_config* config);

/**
 * Hangup epoll socket
 */
static inline void epoll_socket_hup(epoll_socket_t *sock, ddhcp_config *config) {
  sock->spec->epollhup(sock, config);
}

/**
 * Remove a file descriptor from an epoll instance.
 */
void del_fd(int efd, int fd);

/**
 * Free a ddhcpd_epoll_socket instance
 */
static inline void ddhcp_epoll_socket_free(epoll_socket_t *sock) {
  free(sock);
}

/**
 * Create socket from spec and add it to epoll fd
 */
int epoll_create_and_add(epoll_socket_t *instance, epoll_socket_spec_t *spec, ddhcp_config *config, void *ctx);

/**
 * Allocate socket based on spec and add it to epoll fd
 */
int epoll_alloc_and_add(epoll_socket_t **instance, epoll_socket_spec_t *spec, ddhcp_config *config, void *ctx);

#endif

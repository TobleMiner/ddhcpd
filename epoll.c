#include <errno.h>
#include <net/if.h>
#include <stdio.h>
#include <unistd.h>

#include "epoll.h"
#include "logger.h"
#include "types.h"

void epoll_init(ddhcp_config* config) {
  int efd;

  efd = epoll_create1(0);

  if (efd == -1) {
    ERROR("Unable to initialize epoll socket\n");
    perror("epoll_create");
    abort();
  }

  config->epoll_fd = efd;
}

static int hdl_epoll_hup(epoll_socket_t *sock, ddhcp_config* config) {
  int fd = sock->socket;
  DEBUG("hdl_epoll_hup(fd,config): Removing epoll fd:%i\n",fd);
  del_fd(config->epoll_fd, fd);
  close(fd);
  return 0;
}

void del_fd(int efd, int fd) {
  int s = epoll_ctl(efd, EPOLL_CTL_DEL, fd, NULL);

  if (s < 0) {
    int errsv = errno;
    FATAL("%i", errsv);
    perror("epoll_ctl");
    exit(1);   //("epoll_ctl");
  }
}

int epoll_add_socket(int efd, epoll_socket_t *instance) {
  struct epoll_event event = { 0 };
  event.events = instance->spec->events;
  event.data.ptr = instance;

  return epoll_ctl(efd, EPOLL_CTL_ADD, instance->socket, &event);
}

int epoll_update_socket(epoll_socket_t *instance, ddhcp_config *config) {
  unsigned new_ifindex;

  if (!instance->spec->ifname) {
    return 0;
  }

  new_ifindex = if_nametoindex(instance->spec->ifname);

  if (!new_ifindex) {
    return -errno;
  }

  if (new_ifindex != instance->ifindex) {
    // The ifindex changed, we need to notify the application
    if (instance->spec->ifindex_changed) {
      return instance->spec->ifindex_changed(instance, config);
    }
  }

  return 0;
}

int epoll_create_and_add(epoll_socket_t *instance, epoll_socket_spec_t *spec, ddhcp_config *config, void *ctx) {
  instance->spec = spec;
  instance->ctx = ctx;

  if (!spec->epollhup) {
    spec->epollhup = hdl_epoll_hup;
  }

  if (spec->setup) {
    int err = spec->setup(instance, config);
    if (err) {
      ERROR("Epoll socket setup call failed: %d\n", err);
      return err;
    }

    err = epoll_add_socket(config->epoll_fd, instance);
    if (err) {
      ERROR("Failed to add socket to epoll: %d\n", err);
      return err;
    }
  }

  return 0;
}

int epoll_alloc_and_add(epoll_socket_t **instance, epoll_socket_spec_t *spec, ddhcp_config *config, void *ctx) {
  int err;
  epoll_socket_t *sock = calloc(1, sizeof(epoll_socket_t));

  if (!sock) {
    return -ENOMEM;
  }

  err = epoll_create_and_add(sock, spec, config, ctx);
  if (err) {
    free(sock);
  } else if (instance) {
    *instance = sock;
  }

  return err;
}

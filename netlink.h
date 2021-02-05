#ifndef _NETLINK_H
#define _NETLINK_H

#include "types.h"
#include "epoll.h"

ATTR_NONNULL_ALL int netlink_init(epoll_socket_t *sockt, ddhcp_config* config);
ATTR_NONNULL_ALL int netlink_in(epoll_socket_t *sockt, ddhcp_config* config);
ATTR_NONNULL_ALL int netlink_close(epoll_socket_t *sockt, ddhcp_config* config);
#endif

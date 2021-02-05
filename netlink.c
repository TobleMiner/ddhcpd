#include <net/if.h>
#include <netlink/msg.h>
#include <netlink/netlink.h>
#include <netlink/socket.h>
#include <stdbool.h>

#include "epoll.h"
#include "logger.h"
#include "tools.h"
#include "types.h"
#include "util.h"

#ifndef NETLINK_RX_BUF_SIZE
#define NETLINK_RX_BUF_SIZE 128 * 1024
#endif

#ifndef NETLINK_TX_BUF_SIZE
#define NETLINK_TX_BUF_SIZE 8 * 1024
#endif

static int callback(struct nl_msg *msg, void* vcfg) {
  ddhcp_config *config = (ddhcp_config*) vcfg;
  struct nlmsghdr* hdr = nlmsg_hdr(msg);

  DEBUG("netlink_callback(...): callback triggered\n");

  if (hdr->nlmsg_type == RTM_NEWLINK) {
    struct ifinfomsg* data = NLMSG_DATA(hdr);
    char *ifname, ifnamebuf[IF_NAMESIZE + 1] = { 0 };
    if (data->ifi_index < 0) {
      DEBUG("netlink_callback(...): Invalid interface index\n");
      return 0;
    }

    ifname = if_indextoname((unsigned)data->ifi_index, ifnamebuf);
    if (!ifname) {
      DEBUG("netlink_callback(...): No ifname for ifindex %d\n", data->ifi_index);
      return 0;
    }

    for (unsigned i = 0; i < ARRAY_SIZE(config->sockets); i++) {
      epoll_socket_t *sock = &config->sockets[i];
      if (sock->spec->ifname &&
          strcmp(ifname, sock->spec->ifname) == 0 &&
          (unsigned)data->ifi_index != sock->ifindex) {
        DEBUG("netlink_callback(...): %s interface changed\n", socket_id_to_name(i));
        epoll_update_socket(sock, config);
      }
    }

    if (DDHCP_SKT_SERVER(config)->ifindex == (unsigned)data->ifi_index) {
      DEBUG("netlink_callback(...): action on server interface\n");
    }
    if (data->ifi_flags & IFF_UP) {
      DEBUG("netlink_callback(...): iface(%i) up\n",data->ifi_index);
    } else {
      DEBUG("netlink_callback(...): iface(%i) down\n",data->ifi_index);
    }
  }

  if (hdr->nlmsg_type == RTM_DELLINK) {
    struct ifinfomsg* data = NLMSG_DATA(hdr);
    DEBUG("netlink_callback(...): iface(%i) deleted\n",data->ifi_index);
  }

  return 0;
}

ATTR_NONNULL_ALL int netlink_in(epoll_socket_t *sockt, ddhcp_config* config) {
  UNUSED(config);
  return nl_recvmsgs_default((struct nl_sock*)sockt->ctx);
}

ATTR_NONNULL_ALL int netlink_init(epoll_socket_t *sockt, ddhcp_config* config) {
  DEBUG("netlink_init(config)\n");
  struct nl_sock *sock;
  int one = 1;
  socklen_t one_len = sizeof(one);

  sock = nl_socket_alloc();

  if (sock == NULL) {
    FATAL("netlink_init(...): Unable to open netlink socket\n");
    return -1;
  }

  nl_socket_disable_seq_check(sock);
  nl_socket_set_nonblocking(sock);
  nl_socket_modify_cb(sock,NL_CB_VALID,NL_CB_CUSTOM,callback,(void*) config);

  if (nl_connect(sock, NETLINK_ROUTE) < 0) {
    FATAL("netlink_init(...): Unable to connect to netlink route module");
    return -1;
  } else {
    nl_socket_set_buffer_size(sock, NETLINK_RX_BUF_SIZE, NETLINK_TX_BUF_SIZE);
    sockt->socket = nl_socket_get_fd(sock);
    sockt->ctx = sock;
    setsockopt(sockt->socket, SOL_NETLINK, NETLINK_NO_ENOBUFS, &one, one_len);
  }

  nl_socket_add_memberships(sock, RTNLGRP_LINK, 0);

  return 0;
}

ATTR_NONNULL_ALL int netlink_close(epoll_socket_t *sockt, ddhcp_config* config) {
  UNUSED(config);
  nl_socket_free((struct nl_sock*)sockt->ctx);
  return 0;
}

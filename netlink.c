#include <net/if.h>
#include <netlink/msg.h>
#include <netlink/netlink.h>
#include <netlink/socket.h>
#include <stdbool.h>

#include "epoll.h"
#include "logger.h"
#include "tools.h"
#include "types.h"

static int callback(struct nl_msg *msg, void* vcfg) {
  ddhcp_config *config = (ddhcp_config*) vcfg;
  struct nlmsghdr* hdr = nlmsg_hdr(msg);

  DEBUG("netlink_callback(...): callback triggered\n");

  if (hdr->nlmsg_type == RTM_NEWLINK) {
    struct ifinfomsg* data = NLMSG_DATA(hdr);
    if (data->ifi_index < 0) {
      return 0;
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
    sockt->socket = nl_socket_get_fd(sock);
    sockt->ctx = sock;
  }

  nl_socket_add_memberships(sock, RTNLGRP_LINK, 0);

  return 0;
}

ATTR_NONNULL_ALL int netlink_close(epoll_socket_t *sockt, ddhcp_config* config) {
  UNUSED(config);
  nl_socket_free((struct nl_sock*)sockt->ctx);
  return 0;
}

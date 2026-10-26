/* Offline PXA platform for ENet. GPL-3.0-or-later.
 * Packet serialization stays upstream; UDP is explicitly unavailable. */
#include <enet/enet.h>
#include <errno.h>
#include <time.h>
#include "port.h"
static enet_uint32 time_base;
int enet_initialize(void) { port_log("PXA C-Dogs offline: UDP networking unavailable");return 0; }
void enet_deinitialize(void) {}
enet_uint32 enet_time_get(void) { return (enet_uint32)(port_time_us()/1000)-time_base; }
void enet_time_set(enet_uint32 value) { time_base=(enet_uint32)(port_time_us()/1000)-value; }
ENetSocket enet_socket_create(ENetSocketType type) { (void)type;errno=ENOTSUP;return ENET_SOCKET_NULL; }
int enet_socket_bind(ENetSocket s,const ENetAddress *a) { (void)s;(void)a;errno=ENOTSUP;return -1; }
int enet_socket_get_address(ENetSocket s,ENetAddress *a) { (void)s;(void)a;errno=ENOTSUP;return -1; }
int enet_socket_listen(ENetSocket s,int count) { (void)s;(void)count;errno=ENOTSUP;return -1; }
ENetSocket enet_socket_accept(ENetSocket s,ENetAddress *a) { (void)s;(void)a;errno=ENOTSUP;return ENET_SOCKET_NULL; }
int enet_socket_connect(ENetSocket s,const ENetAddress *a) { return enet_socket_bind(s,a); }
int enet_socket_send(ENetSocket s,const ENetAddress *a,const ENetBuffer *b,size_t n) { (void)b;(void)n;return enet_socket_bind(s,a); }
int enet_socket_receive(ENetSocket s,ENetAddress *a,ENetBuffer *b,size_t n) { (void)b;(void)n;return enet_socket_bind(s,a); }
int enet_socket_wait(ENetSocket s,enet_uint32 *condition,enet_uint32 timeout) { (void)s;(void)timeout;*condition=0;errno=ENOTSUP;return -1; }
int enet_socket_set_option(ENetSocket s,ENetSocketOption opt,int value) { (void)s;(void)opt;(void)value;errno=ENOTSUP;return -1; }
int enet_socket_get_option(ENetSocket s,ENetSocketOption opt,int *value) { (void)value;return enet_socket_set_option(s,opt,0); }
int enet_socket_shutdown(ENetSocket s,ENetSocketShutdown how) { (void)s;(void)how;errno=ENOTSUP;return -1; }
void enet_socket_destroy(ENetSocket s) { (void)s; }
int enet_socketset_select(ENetSocket s,ENetSocketSet *read,ENetSocketSet *write,enet_uint32 timeout) {
  (void)s;(void)read;(void)write;(void)timeout;errno=ENOTSUP;return -1;
}
int enet_address_set_host_ip(ENetAddress *a,const char *host) { (void)a;(void)host;errno=ENOTSUP;return -1; }
int enet_address_set_host(ENetAddress *a,const char *host) { return enet_address_set_host_ip(a,host); }
int enet_address_get_host_ip(const ENetAddress *a,char *host,size_t bytes) { (void)a;if(bytes)*host=0;errno=ENOTSUP;return -1; }
int enet_address_get_host(const ENetAddress *a,char *host,size_t bytes) { return enet_address_get_host_ip(a,host,bytes); }

/* Gameplay path-cache timeout; the guest clock is monotonic. */
clock_t clock(void) { return (clock_t)(port_time_us() / (1000000/CLOCKS_PER_SEC)); }
enet_uint32 enet_host_random_seed(void) { return (enet_uint32)port_time_us(); }

/* Campaign ZIPs are read-only in the packaged guest; no archive editing. */
int ftruncate(int fd, off_t size) { (void)fd;(void)size;errno=ENOTSUP;return -1; }

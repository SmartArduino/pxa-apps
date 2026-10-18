/* Offline loopback backend. GPL-2.0-or-later; based on net_udp.c. */
#include "port.h"
#include "qcommon/qcommon.h"
netadr_t net_local_adr;
#define MAX_LOOPBACK 4
typedef struct {
  byte data[MAX_MSGLEN];
  int datalen;
} loopmsg_t;
typedef struct {
  loopmsg_t msgs[MAX_LOOPBACK];
  int get, send;
} loopback_t;
static loopback_t loopbacks[2];
qboolean NET_GetLoopPacket(netsrc_t sock, netadr_t *net_from,
                           sizebuf_t *net_message) {
  int i;
  loopback_t *loop;

  loop = &loopbacks[sock];

  if (loop->send - loop->get > MAX_LOOPBACK)
    loop->get = loop->send - MAX_LOOPBACK;

  if (loop->get >= loop->send)
    return false;

  i = loop->get & (MAX_LOOPBACK - 1);
  loop->get++;

  memcpy(net_message->data, loop->msgs[i].data, loop->msgs[i].datalen);
  net_message->cursize = loop->msgs[i].datalen;
  *net_from = net_local_adr;
  return true;
}

void NET_SendLoopPacket(netsrc_t sock, int length, void *data, netadr_t to) {
  int i;
  loopback_t *loop;

  loop = &loopbacks[sock ^ 1];

  i = loop->send & (MAX_LOOPBACK - 1);
  loop->send++;

  memcpy(loop->msgs[i].data, data, length);
  loop->msgs[i].datalen = length;
}

//=============================================================================

qboolean NET_CompareAdr(netadr_t a, netadr_t b) {
  return a.type == b.type && a.port == b.port && !memcmp(a.ip, b.ip, 4);
}
qboolean NET_CompareBaseAdr(netadr_t a, netadr_t b) {
  return a.type == b.type && !memcmp(a.ip, b.ip, 4);
}
char *NET_AdrToString(netadr_t a) {
  (void)a;
  return "loopback";
}
char *NET_BaseAdrToString(netadr_t a) { return NET_AdrToString(a); }
qboolean NET_StringToAdr(char *s, netadr_t *a) {
  if (strcmp(s, "localhost") && strcmp(s, "loopback"))
    return false;
  memset(a, 0, sizeof(*a));
  a->type = NA_LOOPBACK;
  return true;
}
qboolean NET_IsLocalAddress(netadr_t a) { return a.type == NA_LOOPBACK; }
qboolean NET_GetPacket(netsrc_t sock, netadr_t *from, sizebuf_t *msg) {
  return NET_GetLoopPacket(sock, from, msg);
}
void NET_SendPacket(netsrc_t sock, int n, void *p, netadr_t to) {
  if (to.type == NA_LOOPBACK)
    NET_SendLoopPacket(sock, n, p, to);
}
void NET_Config(qboolean multi) { (void)multi; }
void NET_Init(void) {}
void NET_Shutdown(void) {}
/* net_udp.c returns immediately without a server socket. Sleeping here made
 * the local 10 Hz server also throttle rendering and input to about 10 fps. */
void NET_Sleep(int ms) { (void)ms; }

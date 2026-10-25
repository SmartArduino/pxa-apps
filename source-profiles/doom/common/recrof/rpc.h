#pragma once
#include "pxa_core.h"
const pxa_event_t *port_rpc(uint16_t service, uint16_t opcode,
                            const void *payload, uint32_t length);
const pxa_event_t *port_rpc_packet(const uint8_t *packet, uint32_t length);

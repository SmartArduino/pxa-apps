#ifndef PXA_LAB_MODULE_H
#define PXA_LAB_MODULE_H

#include <stdint.h>

#include "pxa_app_messages.h"

#ifndef PXA_LAB_MODULE_PREFIX
#error "PXA_LAB_MODULE_PREFIX must be defined before including this header"
#endif

#define PXA_LAB_JOIN_INNER(prefix, name) prefix##name
#define PXA_LAB_JOIN(prefix, name) PXA_LAB_JOIN_INNER(prefix, name)
#define PXA_LAB_EXPORT(name) PXA_LAB_JOIN(PXA_LAB_MODULE_PREFIX, name)

#define pxa_app_start PXA_LAB_EXPORT(start)
#define pxa_app_on_event PXA_LAB_EXPORT(on_event)
#define pxa_app_stop PXA_LAB_EXPORT(stop)

extern uint32_t pxa_lab_ui_generation;
const char *pxa_lab_message(pxa_i18n_message_id_t id);

#define PXA_LAB_MSG(id) pxa_lab_message((id))

#endif

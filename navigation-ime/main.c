// SPDX-License-Identifier: Apache-2.0
#include "pxa_ui.h"
#include <string.h>
int32_t pxa_app_start(const uint8_t *config,uint32_t length){
 (void)config;(void)length;uint8_t packet[1024];pxa_ui_transaction_t tx={0};
 const char *lines[]={"基础拼音键盘","请在系统输入法设置中选择本键盘。","支持全键盘英文、拼音逐字选字。","密码框直接输入，不显示拼音候选。"};
 if(!pxa_ui_transaction_begin(&tx,1,PXA_UI_TRANSACTION_REPLACE_SURFACE,packet,sizeof(packet)))return PXA_STATUS_INTERNAL;
 int ok=pxa_ui_create(&tx,1,0,0,PXA_UI_NODE_ROOT)&&pxa_ui_set_u8(&tx,1,PXA_UI_PROPERTY_LAYOUT,PXA_UI_LAYOUT_COLUMN)&&
  pxa_ui_set_padding(&tx,1,14,14,14,14)&&pxa_ui_set_theme_color(&tx,1,PXA_UI_PROPERTY_BACKGROUND,PXA_UI_THEME_BACKGROUND);
 for(uint32_t i=0;ok&&i<4;++i)ok=pxa_ui_create(&tx,i+2,1,0,PXA_UI_NODE_TEXT)&&
  pxa_ui_set_text(&tx,i+2,lines[i],strlen(lines[i]))&&pxa_ui_set_font_role(&tx,i+2,i?PXA_UI_FONT_ROLE_BODY:PXA_UI_FONT_ROLE_TITLE)&&
  pxa_ui_set_theme_color(&tx,i+2,PXA_UI_PROPERTY_FOREGROUND,PXA_UI_THEME_TEXT);
 if(!ok||!pxa_ui_transaction_commit(&tx))return PXA_STATUS_INTERNAL;return PXA_STATUS_OK;
}
int32_t pxa_app_on_event(const uint8_t *event,uint32_t length){(void)event;(void)length;return PXA_EVENT_UNHANDLED;}
void pxa_app_stop(uint32_t reason){(void)reason;}

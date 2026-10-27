/* GPL-2.0-or-later. Menu-only Host file import shared by all eight emulators.
 * ROM bytes never cross Guest memory during copying. */
enum { T_ROM_IMPORT=195 };
static __attribute__((noinline,cold)) void import_roms(void) {
    if(mode!=MENU || scan_pending || import_pending || file || reading_save)return;
    char filter[129];size_t size=strlen(ROM_IMPORT_EXTENSIONS);
    if(size>=sizeof(filter))return;
    for(size_t i=0;i<=size;++i)filter[i]=ROM_IMPORT_EXTENSIONS[i]=='|'?',':ROM_IMPORT_EXTENSIONS[i];
    menu_repeat_buttons=0;menu_repeat_at=0;
    const char *purpose=UI_IMPORT_PURPOSE;
    int status=pxa_fs_request_import(T_ROM_IMPORT,1,filter,size,purpose,strlen(purpose));
    import_pending=!status;
    snprintf(import_message,sizeof(import_message),"%s",!status?UI_IMPORTING:
        status==PXA_STATUS_UNSUPPORTED?UI_HOST_UNSUPPORTED:UI_IMPORT_FAILED);
    redraw=1;
}
static __attribute__((noinline,cold)) void import_completed(const pxa_event_t *event) {
    pxa_fs_import_result_t result={0};
    if(!import_pending)return;
    import_pending=0;
    if(!pxa_fs_parse_import(event,T_ROM_IMPORT,&result))result.status=PXA_STATUS_INVALID_ARGUMENT;
    if(!result.status) {
        const pxa_i18n_argument_t args[]={
            {.name="count",.name_size=5,.type=PXA_I18N_ARGUMENT_U32,.value.u32=result.imported},
            {.name="failed",.name_size=6,.type=PXA_I18N_ARGUMENT_U32,.value.u32=result.failed},
            {.name="skipped",.name_size=7,.type=PXA_I18N_ARGUMENT_U32,.value.u32=result.skipped}};
        pxa_i18n_format(&i18n,PXA_MSG_UI_IMPORT_RESULT,args,3,import_message,sizeof(import_message));
    } else snprintf(import_message,sizeof(import_message),"%s",result.status==PXA_STATUS_CANCELLED?UI_IMPORT_CANCELLED:
        result.status==PXA_STATUS_UNSUPPORTED?UI_HOST_UNSUPPORTED:UI_IMPORT_FAILED);
    /* Cancellation retains files already published. Refresh even on cancel. */
    source=1;open_library();redraw=1;
}

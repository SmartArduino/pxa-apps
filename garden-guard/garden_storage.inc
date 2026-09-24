static void garden_load(void){
    static const char key[]="garden.save";
    pxa_storage_get(STORAGE_GET_REQ,key,sizeof(key)-1,storage_payload,sizeof(storage_payload),packet,sizeof(packet));
}
static void garden_save(void){
#if GARDEN_DEBUG_UNLOCK_ALL
    /* Debug progress is session-only and must not replace the player's save. */
    return;
#endif
    static const char key[]="garden.save";
    uint8_t value[32];
    size_t len=0;
    value[len++]=SAVE_VERSION;
    value[len++]=max_unlocked;
    value[len++]=(uint8_t)(unlocked_mask);
    value[len++]=(uint8_t)(unlocked_mask>>8);
    value[len++]=(uint8_t)(unlocked_mask>>16);
    value[len++]=(uint8_t)(unlocked_mask>>24);
    for(uint8_t i=0;i<LEVELS_COUNT && len<sizeof(value);++i) value[len++]=level_stars[i];
    storage_pending=1;
    pxa_storage_set(STORAGE_SET_REQ,key,sizeof(key)-1,value,len,storage_payload,sizeof(storage_payload),packet,sizeof(packet));
}
static void apply_debug_unlocks(void){
#if GARDEN_DEBUG_UNLOCK_ALL
    max_unlocked=LEVELS_COUNT;
    unlocked_mask=(1u<<PLANT_TYPE_COUNT)-1u;
#endif
}
static void init_unlocks(void){
    unlocked_mask=(1u<<PLANT_SHOOTER)|(1u<<PLANT_SUNFLOWER);
    max_unlocked=1;
    for(uint8_t i=0;i<LEVELS_COUNT;++i) level_stars[i]=0;
    apply_debug_unlocks();
}
static void apply_level_unlock(uint8_t level_index){
    if(level_index>=LEVELS_COUNT) return;
    uint8_t p=levels[level_index].unlock_plant;
    if(p!=0 && p<PLANT_TYPE_COUNT) unlocked_mask|=(1u<<p);
}
static void rebuild_unlocks_from_progress(void){
    /* 重新推导已解锁植物；不得改动 max_unlocked（通关进度） */
    unlocked_mask=(1u<<PLANT_SHOOTER)|(1u<<PLANT_SUNFLOWER);
    for(uint8_t i=0;(uint8_t)(i+1)<max_unlocked && i<LEVELS_COUNT;++i){
        apply_level_unlock(i);
    }
    apply_debug_unlocks();
}

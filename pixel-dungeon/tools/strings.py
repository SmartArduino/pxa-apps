"""Pixel Dungeon user-visible strings (English / Simplified Chinese).

Kept as data so the font generator can extract exactly the CJK glyphs the game
needs. Chinese strings stay short enough for the 296 px panel: at most about
16 CJK characters per message line.
"""

# id, English, Chinese. `%s`/`%d` placeholders follow the English argument order.
STRINGS = [
    # --- messages -------------------------------------------------------
    ('ENTER_DUNGEON', 'You enter the dungeon.', '你进入了地牢。'),
    ('CLIMB_DOWN', 'You climb down to depth %d.', '你下到了第 %d 层。'),
    ('BOSS_STIRS', 'Something huge stirs below.', '下方有什么庞然大物在蠢动。'),
    ('HIT_MOB', 'You hit the %s for %d.', '你击中%s，造成 %d 点伤害。'),
    ('MISS_MOB', 'You miss the %s.', '你没击中%s。'),
    ('MOB_DIES', 'The %s dies!', '%s 死了！'),
    ('MOB_HITS_YOU', '%s hits you for %d.', '%s 击中你，造成 %d 点伤害。'),
    ('MOB_MISSES', 'The %s misses you.', '%s 没击中你。'),
    ('MOB_NOTICES', 'The %s notices you.', '%s 发现了你。'),
    ('YOU_DIE', 'You die...', '你死了……'),
    ('LEVEL_UP', 'You feel stronger! Level %d', '你变强了！等级 %d'),
    ('STRENGTH_UP', 'You feel stronger!', '你感觉更有力量了！'),
    ('HUNGRY', 'You are hungry.', '你有点饿了。'),
    ('STARVING', 'You are starving!', '你已经饥肠辘辘！'),
    ('STARVATION_DAMAGE', 'Starvation costs you %d HP.', '饥饿使你失去 %d 点生命。'),
    ('STARVED', 'You starved to death...', '你活活饿死了……'),
    ('DOOR_OPEN', 'You open the door.', '你打开了门。'),
    ('CHEST_OPEN', 'You open the chest.', '你打开了箱子。'),
    ('TRAP_HIT', 'A trap! You take %d damage.', '陷阱！你受到 %d 点伤害。'),
    ('PICK_GOLD', 'You pick up %d gold.', '你拾取了 %d 枚金币。'),
    ('PICK_ITEM', 'You pick up the %s.', '你拾取了%s。'),
    ('PACK_FULL', 'Your pack is full.', '背包已满。'),
    ('WAIT', 'You wait.', '你等待。'),
    ('SEARCH', 'You search the area.', '你搜索了四周。'),
    ('SEARCH_TRAP', 'You spot a trap nearby.', '你发现了附近的陷阱。'),
    ('NEED_STAIRS', 'Stand on the stairs first.', '需要先站在楼梯上。'),
    ('STAIRS_SEALED', 'The way down is sealed.', '向下的路被封住了。'),
    ('EXIT_SEALED', 'The way out is sealed.', '出口被封住了。'),
    ('NO_POTION', 'You have no healing potions.', '你没有治疗药水。'),
    ('DRINK', 'You drink the potion. +%d HP.', '你喝下药水。生命 +%d'),
    ('UPGRADED', 'The scroll upgrades your %s.', '卷轴强化了%s。'),
    ('NOTHING_UPGRADE', 'Nothing to upgrade.', '没有可强化的装备。'),
    ('MAPPED', 'The floor plan appears.', '本层地图显现了。'),
    ('EAT', 'You eat the ration.', '你吃掉了口粮。'),
    ('WIELD', 'You wield the %s.', '你装备了%s。'),
    ('WEAR', 'You put on the %s.', '你穿上了%s。'),
    ('DROP', 'You drop the %s.', '你丢下了%s。'),
    ('NO_ROOM_DROP', 'No room to drop it.', '没有地方放下它。'),
    ('NO_WAY', 'There is no way there.', '那里过不去。'),
    ('YOU_STOP', 'You stop.', '你停下了。'),
    ('WALL_BLOCKS', 'A wall blocks the way.', '墙挡住了去路。'),
    ('VICTORY_MSG', 'Yog-Dzewa falls. You win!', '尤格-兹瓦倒下了，你赢了！'),
    ('AUDIO_OFF', 'Audio unavailable.', '音频不可用。'),
    ('PICK_KEY', 'You pick up an iron key.', '你拾取了一把铁钥匙。'),
    ('NEED_KEY', 'This door needs an iron key.', '这扇门需要铁钥匙。'),
    ('KEY_UNLOCK', 'You unlock the door.', '你用钥匙打开了门。'),
    ('SHOP_BUY', 'You buy the potion for %d gold.', '你花费 %d 金币买下药水。'),
    ('SHOP_SHORT', 'You need %d gold for the potion.', '购买药水需要 %d 金币。'),
    # --- items ----------------------------------------------------------
    ('ITEM_GOLD', 'Gold', '金币'),
    ('ITEM_IRON_KEY', 'Iron Key', '铁钥匙'),
    ('ITEM_POTION_HEAL', 'Potion of Healing', '治疗药水'),
    ('ITEM_POTION_STRENGTH', 'Potion of Strength', '力量药水'),
    ('ITEM_SCROLL_UPGRADE', 'Scroll of Upgrade', '升级卷轴'),
    ('ITEM_SCROLL_MAP', 'Scroll of Magic Mapping', '探图卷轴'),
    ('ITEM_FOOD', 'Ration of Food', '口粮'),
    ('WEAPON_1', 'Dagger', '匕首'),
    ('WEAPON_2', 'Short Sword', '短剑'),
    ('WEAPON_3', 'Sword', '长剑'),
    ('WEAPON_4', 'Great Sword', '巨剑'),
    ('ARMOR_1', 'Cloth', '布甲'),
    ('ARMOR_2', 'Leather', '皮甲'),
    ('ARMOR_3', 'Chain', '锁子甲'),
    ('ARMOR_4', 'Plate', '板甲'),
    # --- item details ---------------------------------------------------
    ('DETAIL_GOLD', '%d gold', '%d 金币'),
    ('DETAIL_HEAL', 'heals %d HP', '恢复 %d 生命'),
    ('DETAIL_STRENGTH', '+1 strength', '力量 +1'),
    ('DETAIL_UPGRADE', 'upgrades your gear', '强化装备'),
    ('DETAIL_MAP', 'maps this floor', '显示本层地图'),
    ('DETAIL_WEAPON', '%d-%d dmg', '伤害 %d-%d'),
    ('DETAIL_ARMOR', '%d armor', '护甲 %d'),
    ('DETAIL_FOOD', 'food, heals a little', '食物，少量治疗'),
    # --- mobs -----------------------------------------------------------
    ('MOB_RAT', 'rat', '老鼠'),
    ('MOB_GNOLL', 'gnoll', '豺狼人'),
    ('MOB_CRAB', 'crab', '螃蟹'),
    ('MOB_SKELETON', 'skeleton', '骷髅'),
    ('MOB_BAT', 'bat', '蝙蝠'),
    ('MOB_SNAKE', 'snake', '蛇'),
    ('MOB_SPINNER', 'spinner', '巨蛛'),
    ('MOB_SLIME', 'slime', '史莱姆'),
    ('MOB_GOLEM', 'golem', '石魔'),
    ('MOB_YOG', 'Yog-Dzewa', '尤格-兹瓦'),
    # --- chrome ---------------------------------------------------------
    ('TITLE', 'PIXEL DUNGEON', '像素地牢'),
    ('SUBTITLE', 'shattered art, PXA engine', '破碎像素美术 PXA 引擎'),
    ('TAP_CLASS', 'tap a class to descend', '点击职业开始冒险'),
    ('BEST_DEPTH', 'best depth %d', '最深 %d 层'),
    ('CLASS_1', 'Warrior', '战士'),
    ('CLASS_2', 'Rogue', '盗贼'),
    ('CLASS_3', 'Mage', '法师'),
    ('CLASS_1_DESC', 'melee tank', '近战肉盾'),
    ('CLASS_2_DESC', 'dodgy scrapper', '灵巧斗士'),
    ('CLASS_3_DESC', 'glass cannon', '脆皮法师'),
    ('HUD_DEPTH', 'DEPTH %d', '深度 %d'),
    ('HUD_LEVEL', 'LEVEL %d', '等级 %d'),
    ('BTN_PACK', 'PACK', '背包'),
    ('BTN_WAIT', 'WAIT', '等待'),
    ('BTN_POTION', 'POTION', '药水'),
    ('BTN_STAIRS', 'STAIRS', '楼梯'),
    ('PACK_TITLE', 'PACK', '背包'),
    ('PACK_GOLD', 'Gold %d', '金币 %d'),
    ('PACK_WEAPON', 'Weapon: %s', '武器：%s'),
    ('PACK_FISTS', 'bare fists', '空手'),
    ('BAG_USE', 'USE', '使用'),
    ('BAG_WIELD', 'WIELD', '装备'),
    ('BAG_DROP', 'DROP', '丢弃'),
    ('BAG_CLOSE', 'CLOSE', '关闭'),
    ('BAG_EMPTY', 'Your pack is empty.', '背包是空的。'),
    ('DEAD_TITLE', 'YOU DIED', '你死了'),
    ('WIN_TITLE', 'VICTORY!', '胜利！'),
    ('STAT_DEPTH', 'Depth %d', '层数 %d'),
    ('STAT_LEVEL_KILLS', 'Level %d   Kills %d', '等级 %d   击杀 %d'),
    ('STAT_GOLD_TURNS', 'Gold %d   Turns %d', '金币 %d   回合 %d'),
    ('STAT_DEEPEST', 'Deepest %d', '最深 %d 层'),
    ('TAP_RETURN', 'tap to return', '点击返回'),
    ('ENTER_GAME', 'ENTER THE DUNGEON', '进入地牢'),
    ('NEW_GAME', 'NEW GAME', '新游戏'),
    ('CONTINUE_GAME', 'CONTINUE', '继续游戏'),
    ('SAVED_GAME', 'GAME IN PROGRESS', '进行中的游戏'),
    ('CHOOSE_HERO', 'CHOOSE A HERO', '选择英雄'),
    ('BACK', 'BACK', '返回'),
    ('PLAYER_INFO', 'HERO', '英雄信息'),
    ('PLAYER_STRENGTH', 'Strength', '力量'),
    ('PLAYER_HEALTH', 'Health', '生命'),
    ('PLAYER_EXPERIENCE', 'Experience', '经验'),
    ('PLAYER_SATIETY', 'Satiety', '饱腹'),
    ('PLAYER_DEPTH', 'Depth', '层数'),
    ('PLAYER_GOLD', 'Gold', '金币'),
    ('SETTINGS', 'SETTINGS', '设置'),
    ('MAP_ZOOM', 'Map zoom', '地图缩放'),
    ('PINCH_HINT', 'Pinch to zoom the map', '双指缩放地图'),
    ('MAIN_MENU', 'MAIN MENU', '主菜单'),
    ('RANKINGS', 'RANKINGS', '排行榜'),
    ('JOURNAL', 'JOURNAL', '日志'),
    ('SAVE_EMPTY', 'New game', '新游戏'),
    ('SAVE_SLOT', 'Slot %d', '存档 %d'),
    ('KEY_COUNT', 'Keys %d', '钥匙 %d'),
    ('SHOP_TITLE', 'SHOP', '商店'),
    ('SHOP_BUY_BUTTON', 'BUY', '购买'),
    ('RANK_EMPTY', 'No completed adventures yet.', '还没有冒险记录。'),
    ('JOURNAL_EMPTY', 'Nothing recorded yet.', '暂时没有日志。'),
]


def c_identifier(name):
    return 'PD_STR_' + name


def en_text(entry):
    return entry[1]


def zh_text(entry):
    return entry[2]


def chinese_characters():
    """Every distinct CJK character used by the Chinese strings, sorted by
    codepoint so the generated lookup table can be binary searched."""
    seen = set()
    for entry in STRINGS:
        for char in entry[2]:
            if ord(char) > 0x2000:
                seen.add(char)
    return sorted(seen)

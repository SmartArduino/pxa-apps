# 原始美术

场景、肖像和兵种 PNG 由内置 ImageGen 生成。下面记录每张图的提示词。打包脚本将原图转换成共享 RGB565 色板的 PXA 索引纹理。

- `cover.png` 与 `square-cover.png` 分别为宽屏和方屏标题原画。
- `logo.png` 为透明标题 Logo，`app-icon.png` 为独立的城门图标。
- `map.png` 是初版地图原画。新版使用 terrain.png 作为地形底图，应用绘制道路、城池和行军部队。
- `battle.png` 为平原会战背景，另外两张战场图用于山地和水乡。
- `portraits.png` 为六宫格肖像，依次是刘备、曹操、孙权、赵云、夏侯惇、周瑜。
- `soldiers.png` 为 6 列 × 3 行的透明 PNG。三行依次为枪兵、骑兵和弓兵，每行依次为待机、走路接地、走路过渡、蓄势、出手、收势。运行时每帧 42×42 像素，游戏为敌军使用镜像和配色变体。

## 生成提示词

### 封面

> Create an original wide 16:9 key art background for a late Han Three Kingdoms tactile strategy game, no text or symbols. Moody painterly ink-and-gouache mixed with cinematic realistic digital art, antique teal shadows, reddish bronze, fine ochre light. At the RIGHT third, a mounted late-Han commander from behind with flowing red cloak and spear, overlooking a rocky cliff and vast fortress valley with orange dawn, banners and distant armies in fog. Keep the left half dark atmospheric charcoal-blue with low detail for overlaid gold Chinese typography. Strong art direction, premium historical game, no UI or lettering or watermark.

### 地图

> Original 16:9 painted strategic terrain map plate of central and eastern ancient China for a Three Kingdoms video game. High oblique aerial almost top down handcrafted cartographic ink-wash painted miniature landscape, north at top, verdigris teal and dark olive valleys, russet-gold sculpted mountain ranges especially west and edges, banks of two distinct winding rivers, coast water on far east, mist and forests. Top quarter lighter desaturated tan. Central land suitable for overlaying game map city nodes and routes, detailed but muted. Fill full image with terrain, no people, no text, no borders, no lettering, no map symbols or UI.

### 会战

> Wide 16:9 original side-view historic late-Han video game battlefield environment graphic. Dark ink and gouache digitally painted historical illustration, reddish bronze and antique teal, orange dusk. Far background left wooden Chinese hill fort and silhouettes, pale misty jagged mountains across upper half, windswept shredded banners at far left and right. Bottom half is a broad open ochre-earth battlefield with level uncluttered ground for superimposed walking sprite armies, horizontal fighting plane. No characters in foreground. No UI, text, or labels.

### 兵种逐帧图

> Production sprite sheet on transparent background for an original historical late Han Chinese pixel art strategy game. EXACTLY 6 columns by 3 rows in rigid equally spaced invisible grid, each of the 18 tiles contains a single separated small FULL BODY soldier at its center with generous empty transparent gutters. Every character faces RIGHT in side view. No props or pixels crossing between cells. No text or frames or shadows outside the silhouette. Row 1 all of the same teal lamellar shield infantry, aged gold trim and short spear. Row 2 all the same teal lamellar mounted cavalry soldier, red tassel and spear, on a brown horse. Row 3 all the same teal lamellar foot archer holding a longbow. In EVERY row the six tiles left to right are idle, walk contact pose, walk passing pose with opposite leg, attack wind-up, attack release or thrust, recovery. The anatomy, equipment and palette of each soldier are consistent along its whole row. Readable crunchy high quality outlined 32-bit pixel art, strong deliberate single pixels and clean palette, no gradients. TRUE alpha transparent background.

### 武将肖像

> A 3 by 2 grid of six original character portrait illustrations for a Three Kingdoms historical video game, six perfectly equal and abutting cells with no gutters, portraits in order left to right top row Liu Bei (calm short beard, teal robes), Cao Cao (piercing gaze, indigo armor, dignified), Sun Quan (young russet armored ruler with auburn beard). Bottom row Zhao Yun (young clean shaven warrior in ivory armor), Xiahou Dun (weathered commander with cloth eyepatch), Zhou Yu (regal youthful red-robed strategist). Each full face and chest in the center of its individual tile with sufficient head margins. Different recognizable authentic late Han silhouettes. Cohesive fine painterly ink and gouache, dramatic warm rim lighting, dark charcoal plain backgrounds. No text, no labels, no frames. Aspect ratio 3:2 landscape.

### 扩展武将

> Precisely 4 columns by 3 rows of equally sized square tiles, an original character portrait atlas for a polished late-Han Chinese historical strategy video game. Twelve different bust portraits in an exact rigid grid with no gutters, no text, no decoration. Painterly ink-and-gouache historical realism, fine antique gold warm rim lighting and deep charcoal backgrounds, faces centered independently in each square with generous margins. Same painting style throughout. LEFT TO RIGHT ROW ONE: Guan Yu with long beard, deep green lamellar and red-toned complexion; Zhang Fei with wild black beard, stern eyes and black iron armor; Zhuge Liang with a scholarly gray feather fan, white and dark-teal Han robes and cloth headdress; Huang Zhong weathered white-bearded archer in rust and ivory lamellar. ROW TWO: Zhang Liao in oxblood black eastern Han officer armor, youthful focused face; Cao Ren stocky older and mustached with deep purple armor; Xu Huang mature with wrapped ochre head scarf and bronze scale armor; Jia Xu calculating older long-bearded scholar in dark layered robes. ROW THREE: Gan Ning young daring pirate general, red scarf, bells, bronze armor; Lü Meng composed younger commander in chestnut blue armor; Lu Su rounded kind face and clean historical civilian black gauze cap with russet robes; Lu Xun handsome young scholar general in white and green robes. Authentic late Han costume, no fantastical weapons, no typography. Landscape 4:3 or square.

### 江畔会战

> Wide 16:9 original cinematic side-view environment for a Three Kingdoms strategy game. A river landing on the Yangtze in late Han at dusk, an immense reflective blue-green river above, blurred ochre reeds and storm-lit green mountains hidden in mist. A few distant dark wood war junks with brown square sails on the far bank, atmospheric burnt umber and plum charcoal ink-and-gouache with subtle beautiful realistic detail. Broad open horizontal ground across entire lower half, compact muddy pale sand waterfront for foreground soldier sprites. Keep foreground clear and no people. No interface, lettering, borders or watermarks.

### 山地会战

> A wide 16:9 original side-view late Han Chinese video game battlefield environment. Hand painted ink-and-gouache with cinematic realistic detail, a narrow Sichuan mountain pass with monumental layered pine covered gray-green cliffs, a tiny wood-and-stone fortified gate far in the background, morning amber and cold turquoise mist. Lower half a broad empty stony ochre fighting plane with very low foreground detail, uninterrupted level stage ground. No foreground characters, no interface, no lettering. Tasteful antique bronze and verdigris colors.

### 应用图标

> Square premium app launch icon for an original late-Han Three Kingdoms strategy game. An unmistakable silhouette of a single ancient Chinese fortified city gate with three battlements and a small flowing crimson military pennant above, all as a compact etched antique gold-ochre medallion on a deep midnight verdigris and charcoal background. Rich silk and worn bronze detail but bold readable silhouette at 32px, subtle contrast and red accent, original mark. Late Han stone-and-wood gate aesthetic, no paper, no letters, no characters or glyphs, no text. Fill entire square; keep the crest comfortably within the central 72 percent. No clip mask, no round corner or margin, no mockup or watermark.

### 标题 Logo

> An ORIGINAL wordmark title logo on a genuinely transparent background for a Chinese historical strategy video game. Render EXACT four Simplified Chinese characters in order, large and isolated in one horizontal line: “三国逐城”. Three Kingdoms march and siege heritage, visual feeling of a late-Han bronze inscription. Strong controlled monoline angular Chinese lettering, medium-heavy engraved strokes with clean consistent stroke weight, sand-gold luminous raised bronze face, fine dark ochre bevel, no strong distress that obscures the strokes. Standalone wordmark, compact and extremely legible. No other text, no pinyin, no punctuation, no seal lettering, no glow, no scenery, no big background, no watermark. True RGBA transparent background. 3:1 wide horizontal composition, minimal padding. Respect every exact character, especially 国 in simplified form and the 逐 and 城 radicals.

### 方屏封面

> Square full-bleed splash environment key art for an original late Han Three Kingdoms historical strategy game, designed specifically to fill a rounded square handheld display. Cinematic painterly digital ink and gouache, fine antique muted bronze-gold, plum charcoal shadows, dark teal atmospheric mist. A single mounted late-Han commander from behind with red cloak at x=76% y=52%, horse and rider clear. He is overlooking a fortified river valley and distant carved mountain silhouettes. On the right warm copper sunrise through smoke behind layered pines and banners. Keep upper left and middle left calm and substantially dark with atmospheric texture so a gold title and two menu buttons can be laid on it. Fill all four edges with continuous scene and subtle light falloff, no frame, no border, no letters or UI, no logos or watermarks. Beautiful layered landscape, confident original composition. Square 1:1.

## 战役扩展原画

新增九位武将、四件军械和城墙战场。号令动作原画含三兵种各六帧，单挑动作原画含六帧。运行图集按透明轮廓提取动作，并为敌方制作镜像配色。图集中三兵种顺序与游戏阵型一致。

### veterans.png

> Original late Han Chinese historical strategy game portrait atlas. EXACTLY 3 columns by 3 rows of evenly sized square bust portraits, rigid flush grid, no borders, labels or text. Each face centered within its own tile, warm finely painted realistic ink-and-gouache, worn bronze and jade highlights, midnight charcoal individual backgrounds. Row 1 left to right: Fa Zheng (sharp scholarly strategist in cream layered Han robes, short black beard), Wei Yan (stern weathered black-bearded frontier warrior in dark green lamellar leather), Ma Chao (young powerful western horseman in pale silver lamellar and red scarf). Row 2: Zhang He (composed distinguished officer with neat mustache and dark navy lamellar), Xu Chu (large formidable square-faced guard in dark bronze and black armor), Man Chong (older hawk-eyed tactician in muted purple robes and war cap). Row 3: Cheng Pu (elder veteran commander with grey beard and copper helmet), Ling Tong (young proud commander with red band at hair and black-russet lamellar), Xu Sheng (confident mature naval commander with dark green shoulder plates and soot red cloak). Distinct recognizable faces, authentic late Han costume, no emblems, no fantastical elements, no border. Square artwork.

### tactic-frames.png

> Production pixel art sprites for an original late-Han Chinese historical strategy game, true RGBA alpha transparent canvas. Exactly 6 columns by 3 rows, rigid equal grid, every tile contains just ONE isolated complete side-view figure facing RIGHT with generous transparent gutters, no grid lines or text. Every row is a consistent model six poses across (same outfit, face, and proportions): 1 braced guarding, 2 hit recoil, 3 heavy attack anticipation, 4 heavy attack extension, 5 turning to retreat with body still in side view, 6 hurt kneeling recovery. Row 1 jade bronze late Han spearman with rectangular shield. Row 2 jade bronze mounted lancers on chestnut horse, all 6 frames include the same complete horse. Row 3 jade bronze Chinese foot archer in short robe with curved bow, release pose frame 4. Tightly focused superb hand-pixelled 32-bit game sprite aesthetic around 40 pixels high at use. Defined dark outline, ochre highlights, readable silhouettes, no shadows on background, no watermark.

### duel-frames.png

> Production true transparent sprite sheet for an original historical Chinese late-Han strategy game, pixel art with dark bronze outlines and teal highlights. Exactly 6 strictly evenly spaced independent cells in one long horizontal row, generous transparent gaps, no grid marks or text. Each tile shows the same distinct single full-body dignified general facing RIGHT, dark teal and bronze lamellar with gold pauldron, high tied hair, flowing short red sash, wielding long Chinese polearm. Chronological poses left to right: balanced idle with polearm forward, lunge and strike, protective block with shaft and bent knees, feint to side with polearm behind shoulder, stumble back from a hit, low kneel with hand on weapon. The same outfit, face, scale and ground baseline throughout. Clear bright pixels and chunky atlas-ready outlines, no extra weapons floating, no cropping, no shadows, no effects or text.

### siege.png

> Original wide 16:9 side view environment for a late Han Chinese strategy video game. Massive rammed-earth castle ramparts and late-Han watchtower on a raised ridge in the upper middle distance, with ochre torchlight and large storm cloud silhouettes. Foreground lower 55 percent is a flat horizontal open earthen road appropriate for two facing sprite armies and one-on-one hero duels; readable dark brown and gold ground with restrained texture. Evening deep teal haze and muted burnt copper, cinematic realistic hand-painting and Chinese ink-gouache. Atmospheric siege anticipation without people, siege machinery or graphic violence. No text, logos, watermarks or interface.

### gear.png

> Original video game inventory sprite art, true transparent background. Exactly 2x2 equal square tiles in an invisible grid, and one isolated item centered in each tile, graceful ample gutters and nothing crossing cells. Top left an ancient Chinese bronze straight jian sword pointing diagonally up-right with jade-wrapped hilt; top right a compact front-facing dark bronze late-Han lamellar breastplate with jade laces; bottom left a curled saddle and gold crimson cloth tack with simple bronze stirrup; bottom right an ancient Chinese bamboo military scroll wrapped in red silk lace and held by tiny bronze clasp. Strong readable cutout silhouettes at 32 pixels, elegant coherent hand-painted ink-and-gouache painted antiquities with selectively bright gold edges, no text, no floor or background, no icons within the icons.

## 青铜界面素材

胜负题字使用透明原图。会战军牌、司南边框与进度条使用 tools/ui_art.py 生成的位图。军牌按九宫格拼接，兵力、体力、军令和行军进度的底槽与内芯各用三段拼接。

### outcome-win.png

> A premium original title stamp for a Three Kingdoms video game battle result, true transparent RGB alpha PNG. Render ONLY the EXACT two Simplified Chinese characters '胜利' in one horizontal line. Huge centered angular Chinese lettering, forceful thick strokes cut like antique bronze engraving, bright warm luminous sand gold and warm copper edge bevel, a subtle red reflected light near trailing edges, restrained antique detail, extremely clear glyphs at tiny handheld size. No other lettering, no seal, no symbol, no background, no border, no outer glow, no people. Shape only, genuine alpha. 2:1 wide image, two letters with equal scale and sufficient gap. Check exact simplified 胜利.

### outcome-loss.png

> Original historically inspired battle result title stamp for a late Han strategy game on fully transparent alpha background. Only EXACT two simplified Chinese characters '失败' large in one horizontal line, precise Chinese strokes, bold antique cast-metal inscription, dark cinnabar with clear illuminated warm copper-pink faces and charcoal recessed shadows, readable at 100px, crisp shape edge. Refinement and scale similar to a professional historical strategy game's gold victory stamp, distinct distressed red defeat atmosphere. No glow, wreath, frame, seal, scenery, pinyin, extra letters or people. Wide 2:1 shape. Exact text 失败.

### ui-frame.png

> Orthographic pixel-art-friendly ornamental nine-slice frame for an original Chinese late Han game interface, a square hollow border against true transparent RGBA. Precisely centered, symmetrical on all sides, straight horizontal and vertical rails, 90 degree corners. The center is EMPTY fully transparent, around 80% of full width. Border only. Thin antique dark-bronze and aged gold edges, tiny engraved repeating marks on the rails, restrained quatrefoil cloud relief at the four outer corners, formed in sculpted metal, subtle bronze shadows. Each decorative corner stays within its own outer 12% quadrant, spine of each middle edge has uniform thickness so it can be stretched horizontally or vertically in a nine slice. Top and bottom similar. No text or labels, no blank background, no plates outside, no dramatic perspective, no inner gradient. Exact square UI fabrication asset.

### hud-bars.png

> Orthographic sprite atlas for a late Han strategy game HUD, true transparent RGBA, 3 very slim horizontal capsule-like straight bars in separate equally high rows, with transparent gaps, same centered width, each approx 8:1 aspect. Row 1 empty health bar with narrow straight antique gold bronze outer frame, beveled dark charcoal recessed channel, miniature rounded squared end caps that can be cut off for three-slice joining. Row 2 one continuous very bright jade green enameled fill strip, subtle horizontal specular highlight and dark shade at lower rim, clean pixels, no frame. Row 3 one continuous bright red-orange cinnabar enameled fill strip, same height and profile as row 2, clean pixels, no frame. The three rows are visually separate exact long rectangles in side view, no logos, text, numbers, cross markings or ornaments outside the bar. No shadow behind them, designed for small readable handheld health gauges. Landscape image.

## 作战台与城池界面

由 ImageGen 生成。军议厅和城郭用作满幅场景，令牌按九宫格切片，图标和舆图道具打包到同一纹理页。

### command-icons-v2.png

使用内置 ImageGen 生成透明四列四行的汉末章纹图标。各行依次为铜牌、方阵盾、锋矢、雁行翼，环首刀、将技星、军旗、暂停，军心、双刀、迂回、回撤，以及司南、帆船、城门、营帐。提示词要求图标保持独立轮廓、透明留边、青玉和古铜配色，在 12 至 24 像素下仍能辨认。打包时裁取原图的透明单元。

大屏背景从原画重新取样。横屏标题保留 768 × 432 像素，军议厅保留 448 × 252 像素，会战和单挑保留 512 × 272 像素。背景拆成不超过 256 像素的贴图，绘制时在相邻图块之间共用同一条边。小屏和资源不足的设备使用原有的紧凑纹理。

### `war-hall.png`

> Wide 16:9 environmental background art for an original late-Han Chinese historical strategy game. A lantern-lit military council chamber inside a timber citadel, lacquered columns, old bronze fittings, hanging wine red banners with NO markings, a broad campaign table and a rolled parchment, open doorway with dark mountains in mist. Layered hand-painted ink-and-gouache with physically convincing textures, deep charcoal and black jade shadows, rich copper firelight, restrained oxblood accents. Architectural framing left and right, softly lit along upper and outer edges, dark low-contrast middle and lower areas so readable game HUD overlays can live on top. Richly atmospheric premium game art, cinematic environmental illustration, board-game age of war theme. No people, no lettering, no scroll text, no UI, no logo, no border.

### `city-courtyard.png`

> Wide 16:9 original concept art background for a period strategy video game, an early morning view over a compact late-Han fortified city. Large ancient layered gatehouse, red banners without writing hanging from timber, market rooftops, pines, distant misty mountains and a few warm lanterns. Sumptuous painterly historical ink-and-gouache realistic detail, jade-green shadow, bronze light, sepia-brown stone and timber, softly dusky ochre sky. Frame the main gatehouse prominently within the top half, with patterned foreground dark and subdued. No foreground people, no UI, no text and no signs, no borders or logos. Render as cinematic game environment with strong depth.

### `strategy-icons.png`

> Generate an exact 4-column by 3-row texture atlas of twelve distinct original Chinese late Han strategy game prop icons on genuine transparent background. Every equal square cell contains one isolated small icon with 12% transparent gutters, no background, no frames, no text. Reading left to right: row 1 one gold ingot, one gathered wheat sheaf, one flat bamboo military tally with no writing, one late-Han bronze infantry helmet. Row 2 a terrace of planted jade-green seedlings, a tiny stone gate watchtower, a red cloth-covered wicker market basket with coins, two cupped clay hands holding a glowing amber flame. Row 3 an unfurled blank scroll with a bronze magnifier above it, a small spear crossed with a bronze shield, a single bent wooden bow and arrow, a chestnut warhorse head in profile with a scarlet bridle. Extremely recognizable at 18 pixels, chunky silhouettes and intentional highlights, ink-and-gouache rendered miniature objects with subtle pixel art clarity, bronze highlights, red silk and jade accents, no lettering, no numbers, no drop shadows outside props. Exactly 4 columns by 3 rows on true RGBA transparent alpha.

### `command-plaque.png`

> Original empty horizontal button skin for a premium late Han Chinese strategy video game. An isolated orthographic dark lacquered wood and hammered bronze command plaque with rounded cut corners, symmetrical outer aged bronze rim, fine gilded corner fitting accents, softly textured dark brown recessed center with enough contrast for pale text. Strictly straight continuous horizontal and vertical rails and uniform central material with no perspective. Shape must be suitable for nine-slice tiling: corner ornaments confined to the outermost 12% of width on each end, unbroken central 60% region with fine almost invisible wood texture. Very slim simple bevel profile. A single complete horizontal 4:1 shape, no other shapes, not a mockup. No text, symbols, emblems, or writing. True transparent RGBA around the plaque, no backdrop or projected shadow.

### `cartography-props.png`

> An exact 2-column by 2-row transparent sprite atlas of four isolated prop accents for a late Han Chinese strategy game map. Top left a small fortified timber gateway with three battlements, seen frontally from slightly above, with NO flag and clear dark silhouette; top right a small raised gold circular compass with a simple rose pointing upward, no letters or marks; bottom left a wooden crimson-edged square military camp tent with ochre canvas; bottom right a miniature dark teal and gold empire map scroll rolled partly from both sides, no writing. True RGBA transparent background, plenty of empty margin in every equal square cell, no extra components crossing cells, painted pixel-art-friendly bronze engraved objects with bold readable forms at 16 pixels, historically grounded material, no typography, logo or numbers.


## 商城图标与封面

`store-icon.png` 是原创 `app-icon.png` 的 512 × 512 商城版本。`store-cover.png` 是独立的 1280 × 960 插画，使用 ImageGen，以原始标题背景和金色题字为参考。

> Game store illustrated cover, exact landscape 4:3 composition, for the original historical Chinese strategy game 三国逐城. The first image is a visual reference for setting, late-Han gear and painterly art, the second is a branding reference for the exact gold relief title. Create a fresh cover illustration with a Han commander on horseback in dark lacquer lamellar and a red cape occupying the foreground right third, viewed from behind in profile, overlooking a fortified Chinese city ringed by mist, distant mountain ridges, river, and small columns of marching armies. Dark ink-green and smoky cyan shadows, warm copper and amber sunset rim light, detailed brushwork, strong thumbnail readability and silhouette. Put the exact four simplified Chinese characters 三国逐城 at the upper-left, in one uninterrupted horizontal line, unmistakable chunky hammered gold relief from the second reference. Leave breathing room around the edges and keep the title and commander within central 85 percent of the frame. No other text, graphics, ratings, UI, product mockup or border. Square corners, cover art fills frame.

## 会战军牌与木漆面板

原始美术稿由 ImageGen 绘制。`battle-bronze.png` 保留为会战军牌的配色参考。`amber-lacquer.png` 用于军令、战报、内政与武将面板。两种框都按九宫格拼接，保留外缘镂空和不透明木漆底。

### `battle-bronze.png`

> Create one isolated original game HUD plaque for a late-Han Three Kingdoms strategy game, broad horizontal silhouette roughly 4.2:1. Game-production bitmap, true square-on orthographic UI front view, exactly level rails. Confident craft: a carved dark walnut/amber lacquer center with a visible quiet warm wood-grain sheen (warm reddish brown, no black), gently inset, and a thick antique-brass bevel with fine etched thunder scroll geometric corners confined to the first and last 12% of width. Thin double copper rails with aged hammered gloss. Slightly angular silhouette, sturdy ends, central field uninterrupted, no ornaments, jewels, marks or letterforms in the center. IMPORTANT: Designed as a nine-slice scalable UI sprite. Top/bottom edges perfectly straight and uniform over the middle 76%, short side edges perfectly straight and uniform except corners, corners bright and legible when reduced to 8 pixels. OPAQUE center and rails, no translucent fill. Genuine transparent RGBA only around the outline, no outside cast shadow. No lettering, figures, gauges, mockup, frame within frame, second plaque, symbols or ornament outside the rim. Inspired by an archaeological Han bronze tally and lacquered wood, painted pixel-art friendly, richly colored amber and chestnut wood with high polish and deliberate silhouette.

### `amber-lacquer.png`

> Create one complete ornate but restrained original UI dialog frame for a historical late Han strategy video game. Single 4:3 landscape rectangular pane, square-on front elevation. Solid opaque warm chestnut lacquered hardwood with quiet visible grain and subtle darker edge vignette. Center restrained uniform medium-dark umber, even visual surface, no black. Bronze outer frame has a stepped bevel, fine double lines, warm copper highlights, and small angular Han Chinese thunder-pattern corner plates using roughly 9% of width and height. The thick material should read as a finished antique campaign table object at low resolution. Top/bottom and left/right rails must be completely straight, unbroken, and uniform between the four corners, with no center edge crests or fasteners; use for nine-slice UI. Center uninterrupted, no crest, text, figures, objects, numbers, buttons, inset panel, extra frame or watermark. Opaque pane rather than an empty hole. Genuine alpha only outside silhouette, no perspective or projected shadow. Walnut, burnt sienna and softly worn warm gold, game prop illustration with ink-painted detail and recognizable wide bevels.

### 在野人才

[六位在野人才头像图集](wild-talents.png) 由内置 ImageGen 生成，以 veterans.png 为画风参考。图集从左至右、从上至下依次为程砚、许棠、沈汐、霍毅、黎岚、梁稷。游戏将每格等比裁成方形头像。

> Create a production bitmap portrait sprite sheet for the original Three Kingdoms strategy game Han Banners. The attached existing portraits are ONLY a style reference for the muted detailed realism, old oil on canvas grain, charcoal backgrounds, and Late Eastern Han costume styling. Make six NEW, distinct fictional East Asian adults. Output an exactly 3:2 LANDSCAPE image with a precise 3-column by 2-row grid of six equal perfectly square cells. No outer margins, no dividers, no labels, no lettering or decorations. Each cell has a separate black-brown charcoal canvas background, its own coherent warm portrait lighting, a clearly visible color clothing silhouette and a complete large centered head from hair top through upper chest with some headroom. Portrait heads should stay centered horizontally so tight square crops work at 58px. Top left a modest Jianghan scholar man around 34, warm oval face, slim moustache and dark green headcloth, blue-gray woven robes. Top middle a serious young woman treasury clerk with an umber wrapped bun, ivory and aubergine clothing and simple wooden beads. Top right a sunburned Jiangdong boat logistics officer man forty with short beard, cyan rain cape over light leather armor and brown sash. Bottom left stout Jingzhou veteran male spear captain fifty, sideburns, coarse steel lamellar and deep vermilion knotted neck cloth. Bottom middle alert female Fujian hill scout early thirties, narrow eyes and bound high hair under dull tan headband, olive leather shoulderguard and reed-green cloak. Bottom right kind older Hepu grain and port magistrate male, more angular slim gray beard, bronze pin and tobacco-colored woven Han robe. Distinguish all faces and lighting, keep historical Chinese Late Han garment construction and no fantasy armor. Fine matte paint texture, no photoreal glossy modern lighting, no separated objects, all corners opaque.

#!/usr/bin/env python3
"""Rebuild the small, original teaching decks (classical poem texts are public domain)."""
from pathlib import Path
import json
ROOT=Path(__file__).resolve().parents[3]

def deck(name, groups):
    rows=[]
    for level,group in enumerate(groups):
        for q,a,e,*lookup in group:
            rows.append(dict(question=q,answer=a,explanation=e,lookup=lookup[0] if lookup else '',level=level))
    folder=ROOT/name
    folder.mkdir(exist_ok=True)
    (folder/'content.json').write_text(json.dumps({'version':1,'cards':rows},ensure_ascii=False,indent=2)+'\n')
    lines=['// SPDX-License-Identifier: MIT','// Generated from content.json by common/study/tools/create_content.py.','inline std::vector<study::Item> builtins() {','    std::vector<study::Item> items = {']
    for r in rows:
        vals=[json.dumps(r[k],ensure_ascii=False) for k in ('question','answer','explanation','lookup')]
        lines.append('        {'+', '.join(vals)+f', {r["level"]}, 0'+'},')
    lines+=['    };','    for (auto& item : items) item.id = study::identity(item);','    return items;','}']
    (folder/'content.inc').write_text('\n'.join(lines)+'\n')

vocab=[
'apple:苹果 cat:猫 dog:狗 sun:太阳 moon:月亮 water:水 book:书 school:学校 red:红色 blue:蓝色 green:绿色 yellow:黄色 one:一 two:二 three:三 mother:母亲 father:父亲 friend:朋友 happy:高兴的 big:大的 small:小的 day:白天 night:夜晚 tree:树',
'library:图书馆 breakfast:早餐 vegetable:蔬菜 animal:动物 weather:天气 season:季节 spring:春天 summer:夏天 autumn:秋天 winter:冬天 question:问题 answer:答案 beautiful:美丽的 healthy:健康的 exercise:锻炼 travel:旅行 country:国家 language:语言 learn:学习 remember:记得 forget:忘记 early:早的 careful:仔细的 together:一起',
'compare:比较 improve:改善 explain:解释 discover:发现 environment:环境 energy:能量 population:人口 culture:文化 communicate:交流 necessary:必要的 responsible:负责任的 independent:独立的 experience:经验 knowledge:知识 opinion:观点 advantage:优点 challenge:挑战 confidence:信心 opportunity:机会 solution:解决方案 evidence:证据 achieve:实现 purpose:目的 protect:保护',
'budget:预算 invoice:发票 appointment:预约 colleague:同事 negotiate:协商 deadline:截止日期 efficient:高效的 reliable:可靠的 sustainable:可持续的 priority:优先事项 investment:投资 interest:利息 insurance:保险 expense:支出 revenue:收入 strategy:策略 evaluate:评估 maintain:维护 perspective:视角 accurate:准确的 flexible:灵活的 ambiguous:有歧义的 consistent:一致的 resilient:有韧性的']
examples={'apple':'An apple is a fruit. 苹果是一种水果。','library':'I read books in the library. 我在图书馆读书。','evidence':'Explain your answer with evidence. 用证据解释答案。','budget':'Make a budget before shopping. 购物前制定预算。'}
groups=[]
for group in vocab:
    groups.append([(w,zh,examples.get(w,f'{w}：{zh}。翻卡后先回忆，再选择“记住”或“再学”。'),w) for w,zh in (x.split(':') for x in group.split())])
deck('learn-words',groups)

poems=[
[("静夜思","李白","床前明月光，疑是地上霜。","举头望明月，低头思故乡。","诗人望见月光，想起自己的故乡。"),
("春晓","孟浩然","春眠不觉晓，处处闻啼鸟。","夜来风雨声，花落知多少。","从鸟鸣和落花感受春天的早晨。"),
("登鹳雀楼","王之涣","白日依山尽，黄河入海流。","欲穷千里目，更上一层楼。","想看得更远，就再登高一层。"),
("悯农（其二）","李绅","锄禾日当午，汗滴禾下土。","谁知盘中餐，粒粒皆辛苦。","每一粒粮食都凝结着劳动。"),
("相思","王维","红豆生南国，春来发几枝。","愿君多采撷，此物最相思。","借红豆表达对朋友的思念。"),
("江雪","柳宗元","千山鸟飞绝，万径人踪灭。","孤舟蓑笠翁，独钓寒江雪。","用寂静的雪景衬托独自垂钓的人。")],
[("绝句","杜甫","两个黄鹂鸣翠柳，一行白鹭上青天。","窗含西岭千秋雪，门泊东吴万里船。","黄、翠、白、青四种颜色构成明丽的春景。"),
("望庐山瀑布","李白","日照香炉生紫烟，遥看瀑布挂前川。","飞流直下三千尺，疑是银河落九天。","用夸张与比喻写出瀑布的壮美。"),
("山行","杜牧","远上寒山石径斜，白云生处有人家。","停车坐爱枫林晚，霜叶红于二月花。","“坐”在这里是“因为”的意思。"),
("泊船瓜洲","王安石","京口瓜洲一水间，钟山只隔数重山。","春风又绿江南岸，明月何时照我还。","“绿”写出春风让江岸焕发生机。"),
("咏柳","贺知章","碧玉妆成一树高，万条垂下绿丝绦。","不知细叶谁裁出，二月春风似剪刀。","把春风比作剪刀，写柳叶的精巧。"),
("赠汪伦","李白","李白乘舟将欲行，忽闻岸上踏歌声。","桃花潭水深千尺，不及汪伦送我情。","用潭水之深衬托朋友情谊之深。")],
[("送杜少府之任蜀州","王勃","海内存知己，","天涯若比邻。","节选。真正的知己即使相隔天涯，也像近邻一样亲近。"),
("望岳","杜甫","会当凌绝顶，","一览众山小。","节选。表达勇于攀登、俯视一切的志向。"),
("行路难（其一）","李白","长风破浪会有时，","直挂云帆济沧海。","节选。相信终有一天能够克服困难，实现理想。"),
("酬乐天扬州初逢席上见赠","刘禹锡","沉舟侧畔千帆过，","病树前头万木春。","节选。借新旧更替的景象表现积极向前的精神。"),
("次北固山下","王湾","海日生残夜，","江春入旧年。","节选。残夜中升起旭日，旧年中已有春意。"),
("游山西村","陆游","山重水复疑无路，","柳暗花明又一村。","节选。看似无路时，转折之后又出现新的景象。")],
[("终南别业","王维","行到水穷处，","坐看云起时。","节选。走到流水尽头，静坐看云升起，心境自在。"),
("定风波","苏轼","竹杖芒鞋轻胜马，谁怕？","一蓑烟雨任平生。","节选。以雨中行路写面对人生风雨的从容。"),
("饮酒（其五）","陶渊明","采菊东篱下，","悠然见南山。","节选。写采菊时自然抬头见山的悠闲心境。"),
("浣溪沙","晏殊","无可奈何花落去，","似曾相识燕归来。","节选。在花落燕归之间感受流逝与重逢。"),
("题西林壁","苏轼","不识庐山真面目，","只缘身在此山中。","节选。身处局部时，往往难以看清整体。"),
("观书有感（其一）","朱熹","问渠那得清如许？","为有源头活水来。","节选。以源头活水比喻不断学习带来的新知。")]]
groups=[]
for group in poems:
    groups.append([(f'《{title}》·{author}\n{first}\n下一句是？',second,f'{first}\n{second}\n\n{note}',title) for title,author,first,second,note in group])
deck('learn-poetry',groups)

chars=[
'人:rén:人们 口:kǒu:人口 日:rì:日出 月:yuè:月亮 山:shān:高山 水:shuǐ:河水 火:huǒ:火花 木:mù:木头 大:dà:大小 小:xiǎo:小手 上:shàng:上下 下:xià:山下',
'学:xué:学习 校:xiào:学校 朋:péng:朋友 友:yǒu:友好 春:chūn:春天 夏:xià:夏天 秋:qiū:秋天 冬:dōng:冬天 读:dú:读书 写:xiě:写字 家:jiā:家人 国:guó:祖国',
'行:xíng:行走 行:háng:银行 长:cháng:长短 长:zhǎng:成长 重:zhòng:重量 重:chóng:重复 乐:lè:快乐 乐:yuè:音乐 藏:cáng:收藏 藏:zàng:西藏 着:zhuó:着陆 着:zháo:着急',
'酌:zhuó:斟酌 缜:zhěn:缜密 赡:shàn:赡养 契:qì:契约 隽:juàn:隽永 踌:chóu:踌躇 躇:chú:踌躇 谒:yè:拜谒 镌:juān:镌刻 觊:jì:觊觎 觎:yú:觊觎 翌:yì:翌日']
groups=[]
for group in chars:
    rows=[]
    for token in group.split():
        char,pinyin,word=token.split(':')
        rows.append((f'“{word}”中的“{char}”读什么？',pinyin,f'正确读音：{char}（{pinyin}）\n词语：{word}\n读音与词语有关，多音字要结合语境。',char))
    groups.append(rows)
deck('learn-hanzi',groups)

facts=[
[("一天有多少小时？","24 小时","一天通常按24小时计算。"),("一年有几个月？","12 个月","一年分为12个月。"),("一周有几天？","7 天","一周包括星期一到星期日。"),("红灯亮时，行人应该怎样？","停下等待","过路口遵守交通信号，确认安全后再通行。"),("鱼主要用什么器官呼吸？","鳃","多数鱼用鳃从水中获取氧气。"),("植物通常用什么部位吸收土壤中的水？","根","根能吸收水和无机盐。"),("太阳通常从哪个方向升起？","东方","我们用太阳升起的一侧辨认东方。"),("把垃圾放入对应类别的垃圾桶叫什么？","垃圾分类","按照当地要求分类投放垃圾。"),("三角形有几条边？","3 条","三条线段围成三角形。"),("人主要用什么器官看东西？","眼睛","眼睛接收光，视觉还需要大脑参与。"),("冰融化后通常变成什么？","水","这是从固态到液态的变化。"),("彩虹通常有几种代表颜色？","7 种","常说红橙黄绿青蓝紫，实际颜色连续变化。")],
[("地球的天然卫星叫什么？","月球","月球围绕地球运行。"),("植物光合作用通常释放什么气体？","氧气","在光照下，植物利用二氧化碳和水合成有机物，并释放氧气。"),("水在标准大气压下的沸点是多少？","100 摄氏度","沸点会随气压变化，高海拔地区通常低于100摄氏度。"),("指南针的N极通常指向哪里？","地理北方附近","磁北与地理北并不完全重合。"),("中国的首都是什么城市？","北京","北京是中华人民共和国的首都。"),("一千米等于多少米？","1000 米","长度单位：1千米=1000米。"),("一千克等于多少克？","1000 克","质量单位：1千克=1000克。"),("《西游记》传统上署名的作者是谁？","吴承恩","《西游记》是中国古典长篇小说。"),("地球自转带来的主要现象是什么？","昼夜交替","同一地点随地球自转轮流朝向或背向太阳。"),("人体骨骼的主要作用之一是什么？","支撑身体","骨骼还参与保护器官和运动。"),("蒸发通常发生在液体的哪里？","表面","蒸发可以在不同温度下进行。"),("平年二月有多少天？","28 天","闰年的二月有29天。")],
[("速度的基本计算公式是什么？","路程 ÷ 时间","平均速度等于总路程除以总时间。"),("电阻的国际单位是什么？","欧姆","电阻单位符号为Ω。"),("化学符号O代表什么元素？","氧","O是氧元素符号；氧气常写为O₂。"),("中性原子的质子数等于什么？","电子数","质子带正电，电子带负电，数量相等时电中性。"),("直角三角形两直角边为3和4，斜边为？","5","勾股定理：3²+4²=5²。"),("太阳系中体积最大的行星是？","木星","木星是太阳系最大的行星。"),("产生四季更替的重要原因是什么？","地轴倾斜与地球公转","不同季节同一纬度接收的太阳辐射不同。"),("DNA中文通常叫什么？","脱氧核糖核酸","DNA是多数生物储存遗传信息的物质。"),("声音能在真空中传播吗？","不能","声音传播需要介质。"),("文艺复兴最早兴起于欧洲哪个地区？","意大利","意大利城市的经济与文化为文艺复兴提供了条件。"),("方程2x+3=11的解是什么？","x = 4","两边减3得到2x=8，再除以2。"),("概率为0.25，用百分数表示为多少？","25%","0.25乘以100%等于25%。")],
[("复利与单利的关键区别是什么？","利息是否继续计息","复利会让前期产生的利息参与后续计息。"),("相关关系能直接证明因果关系吗？","不能","相关可能来自共同原因、偶然性或反向因果。"),("强密码在不同网站应该怎样使用？","每个网站使用不同密码","避免一个账户泄露后影响其他账户。"),("数据中最常出现的数值叫什么？","众数","一组数据可能有多个众数，也可能没有唯一众数。"),("中位数主要用于描述什么？","排序后居中的位置","中位数通常比平均数更不易受极端值影响。"),("HTTPS中用于保护传输的常用协议是？","TLS","TLS为传输提供加密和身份认证机制。"),("机会成本指什么？","放弃的最佳替代方案价值","做出选择时，机会成本也是需要考虑的成本。"),("名义金额扣除通胀影响后称为？","实际金额","比较不同时期的购买力要考虑物价变化。"),("一度电对应多少能量？","1 千瓦时","1千瓦的电器工作1小时消耗1千瓦时电能。"),("计算百分比变化时，应以什么为分母？","原来的数值","变化率=(新值-原值)/原值，原值为0时需另行处理。"),("抽样调查应优先避免什么问题？","样本选择偏差","样本应尽可能代表需要研究的总体。"),("把复杂任务拆成可检查的小步骤有什么作用？","便于执行和反馈","小步骤更容易明确完成条件并及时纠正方向。")]]
deck('learn-cards',facts)

# Each installable app contains only one age group's data. The five parent
# directories are source decks/artwork, not additional mixed-age packages.
import shutil
suffixes=['preschool','primary','teen','adult']
age_names=['亲子启蒙','小学','中学','成人']
names=['口算闯关','诗词小馆','单词记忆卡','汉字练习册','知识卡盒']
slugs=['learn-math','learn-poetry','learn-words','learn-hanzi','learn-cards']
for kind,base in enumerate(slugs):
    source=ROOT/base
    rows=json.loads((source/'content.json').read_text())['cards'] if kind else []
    for age,suffix in enumerate(suffixes):
        slug=base+'-'+suffix
        target=ROOT/slug;target.mkdir(exist_ok=True)
        selected=[r for r in rows if r['level']==age]
        lines=['// SPDX-License-Identifier: MIT','// Generated age-specific deck; rebuild with common/study/tools/create_content.py.',
               'inline std::vector<study::Item> builtins() {','    std::vector<study::Item> items = {']
        for r in selected:
            values=[json.dumps(r[k],ensure_ascii=False) for k in ('question','answer','explanation','lookup')]
            lines.append('        {'+', '.join(values)+f', {age}, 0'+'},')
        lines+=['    };','    for (auto& item : items) item.id = study::identity(item);','    return items;','}']
        (target/'content.inc').write_text('\n'.join(lines)+'\n')
        (target/'CMakeLists.txt').write_text(f'cmake_minimum_required(VERSION 3.20)\nproject(pxa_{slug.replace("-","_")} LANGUAGES C CXX)\nset(STUDY_KIND {kind})\nset(STUDY_AGE {age})\ninclude(../common/study/StudyApp.cmake)\n')
        origins={1:['https://v1.jinrishici.com'],2:['https://api.dictionaryapi.dev'],3:['https://cdn.jsdelivr.net'],4:['web']}.get(kind,[])
        services=['ui','clock','storage','log','window']+(['fs','permission',{'name':'net','min_version':[0,5]}] if kind else [])
        pkg={'format':'pxa-package-source-0.2','min_sdk':[2,1],'target_sdk':[2,1],'compile_sdk':[2,1],
             'id':'pxa-'+slug,'version':'0.1.0','release_sequence':1,'name':names[kind]+'·'+age_names[age],
             'description':f'{age_names[age]}独立版。全屏轻量学习，原生触摸与按键操作，短回合练习、学习记录和错题复习。'+('对应年龄题目本地生成，无需网络或SD卡。' if not kind else f'只内置本年龄段{len(selected)}张卡片，在线内容按需获取，不依赖SD卡。'),
             'publisher':{'handle':'lucinhu','display_name':'lucinhu'},'icon':'assets/icon.png',
             'build':{'system':'cmake','source_dir':'.','linear_memory':{'maximum_bytes':1048576,'pinned':False}},
             'components':[{'id':'main','kind':'ui','cmake_target':'pxa_main','artifact':'aot','services':services,
                            'wasi':{'version':'preview1','libc':'wasi-libc','features':['monotonic-clock','wall-clock']}}],
             'permissions':[{'name':'net.client','required':False,'scope':origin} for origin in origins]}
        (target/'package.json').write_text(json.dumps(pkg,ensure_ascii=False,indent=2)+'\n')
        (target/'assets').mkdir(exist_ok=True)
        for name in ('icon.png','LICENSE.txt','NOTICE.txt'):
            shutil.copyfile(source/'assets'/name,target/'assets'/name)
        if kind==3:shutil.copyfile(source/'assets/ARPHICPL.TXT',target/'assets/ARPHICPL.TXT')
        if kind==4:
            example={'version':1,'cards':[{k:r[k] for k in ('level','question','answer','explanation')} for r in selected[:4]]}
            (target/'example-course.json').write_text(json.dumps(example,ensure_ascii=False,indent=2)+'\n')
    for name in ('package.json','CMakeLists.txt','content.inc'):
        path=source/name
        if path.exists():path.unlink()

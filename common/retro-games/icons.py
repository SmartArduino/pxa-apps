#!/usr/bin/env python3
"""Rebuild the original SVG/PNG launcher artwork (requires cairosvg)."""
# SPDX-License-Identifier: MIT
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
INK = '#edf4ff'
DARK = '#17263f'
COLORS = ['#55dbcb', '#80b5ff', '#ffbb69', '#c49aff', '#ff859d', '#b0df78']


def rect(x, y, w, h, fill=INK, r=5, stroke='none', sw=3):
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>'


def circle(x, y, r, fill=INK, stroke='none', sw=3):
    return f'<circle cx="{x}" cy="{y}" r="{r}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>'


def line(x, y, X, Y, color=INK, w=5):
    return f'<path d="M{x} {y} L{X} {Y}" fill="none" stroke="{color}" stroke-width="{w}" stroke-linecap="round"/>'


def path(d, fill='none', stroke=INK, sw=5):
    return f'<path d="{d}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}" stroke-linecap="round" stroke-linejoin="round"/>'


def text(x, y, value, size=24, color=INK):
    return f'<text x="{x}" y="{y}" text-anchor="middle" dominant-baseline="central" font-family="DejaVu Sans, sans-serif" font-weight="bold" font-size="{size}" fill="{color}">{value}</text>'


def grid(n=4):
    return ''.join(line(48+i*160/n, 48, 48+i*160/n, 208, '#48607c', 2)+
                   line(48, 48+i*160/n, 208, 48+i*160/n, '#48607c', 2) for i in range(n+1))


def cell(x, y, c=INK, n=4, inset=4):
    s = 160/n
    return rect(48+x*s+inset, 48+y*s+inset, s-2*inset, s-2*inset, c, 4)


def pixels(rows, x, y, step, color=INK):
    return ''.join(rect(x+j*step,y+i*step,step,step,color,0) for i,row in enumerate(rows)
                   for j,p in enumerate(row) if p != '.')


def star(x, y, s=12, c=INK):
    return path(f'M{x} {y-s} L{x+s*.3} {y-s*.3} L{x+s} {y} L{x+s*.3} {y+s*.3} L{x} {y+s} L{x-s*.3} {y+s*.3} L{x-s} {y} L{x-s*.3} {y-s*.3} Z',c,'none')


def figure(x, y, c=INK, run=False):
    return circle(x,y,10,c)+path(f'M{x} {y+17} L{x-5} {y+43} M{x-3} {y+26} L{x-22} {y+35} M{x-3} {y+26} L{x+19} {y+19} M{x-5} {y+43} L{x-23} {y+65} M{x-5} {y+43} L{x+18} {y+(58 if run else 65)}','none',c,8)


def motif(name, a):
    s = ''
    if name in ('fifteen', 'sixteen', 'solo', 'keen', 'towers', 'unequal', 'filling', 'range'):
        n = 4 if name in ('fifteen','sixteen') else 3
        values = {'fifteen':[1,2,3,4,5,6,7,8,9,10,11,12,13,14,0,15],
                  'sixteen':[1,3,2,4,5,6,7,8,9,10,12,11,13,14,15,16],
                  'solo':[1,0,3,0,2,0,3,0,1], 'keen':[2,1,3,3,2,1,1,3,2],
                  'unequal':[1,0,3,0,2,0,2,3,1], 'filling':[3,3,2,3,1,2,2,2,1],
                  'range':[0,3,0,2,0,4,0,2,0]}.get(name,[0]*9)
        if name=='towers':
            s=grid(3)
            for y in range(3):
                for x in range(3):
                    h=12+((x+y*2)%3)*12
                    s+=rect(61+x*53,95+y*53-h,28,h,a,3)+path(f'M{61+x*53} {95+y*53-h} l10 -8 h28 l-10 8 Z',INK,'none')
            return s
        for i,v in enumerate(values):
            x=48+(i%n)*160/n;y=48+(i//n)*160/n
            s+=rect(x+3,y+3,160/n-6,160/n-6,a if v else '#253a55',5)
            if v:s+=text(x+80/n,y+80/n,str(v),23 if n==4 else 30,DARK)
        if name=='sixteen':s+=path('M60 226 H183 l-10 -10 M183 226 l-10 10','none',INK,4)
        if name=='keen':s+=path('M52 102 H148 V204 M102 52 V102 M153 152 H204','none',DARK,5)
        if name=='unequal':s+=text(102,78,'&lt;',20)+text(154,130,'&gt;',20)
        return s
    if name=='blackbox':
        return rect(55,55,146,146,'#253a55',12)+circle(100,107,16,a)+circle(153,151,16,a)+path('M28 80 H77 Q107 80 107 50 V28','none',INK,6)+path('M228 175 H190 Q175 175 175 205 V228','none',a,6)+star(107,28,7)+star(228,175,7,a)
    if name=='bridges':
        nodes=[(65,65,2),(191,65,3),(65,191,2),(191,191,3),(128,128,4)]
        s=line(65,61,191,61,a)+line(65,69,191,69,a)+line(65,65,65,191,a)+line(187,65,187,191,a)+line(195,65,195,191,a)+line(65,191,191,191,a)
        for x,y,v in nodes:s+=circle(x,y,24,INK)+text(x,y,v,23,DARK)
        return s
    if name=='cube':
        return path('M128 41 L201 83 V171 L128 214 L55 171 V83 Z',a,'none')+path('M55 83 L128 126 L201 83 M128 126 V214','none',DARK,5)+path('M92 62 L165 105 V192 M165 62 L92 105 V192 M55 127 L128 170 L201 127','none',DARK,3)+path('M165 105 l36 -22 v43 l-36 22 Z',INK,'none')
    if name=='dominosa':
        for x,y,w,h in [(48,48,77,37),(131,48,77,77),(48,91,37,117),(91,131,117,37),(91,174,117,34)]:
            s+=rect(x,y,w,h,a,8)
        for x,y,v in [(65,66,1),(105,66,2),(151,66,3),(191,66,1),(65,108,2),(65,149,3),(65,190,1),(110,149,3),(151,149,1),(191,149,2),(110,190,2),(151,190,1),(191,190,3)]:
            for j in range(v):s+=circle(x-5+(j%2)*9,y-5+(j//2)*9,3,DARK)
        return s
    if name in ('flip','mosaic','pattern','unruly','samegame','flood'):
        n=5
        matrix={'flip':['00100','01110','11111','01110','00100'], 'pattern':['00100','01110','11111','00100','00100'],
                'mosaic':['11100','10100','11111','00101','00111'], 'unruly':['01001','10110','11001','00110','01010'],
                'samegame':['00122','00122','11022','11220','01200'], 'flood':['00012','00112','00122','01122','11222']}[name]
        for y,row in enumerate(matrix):
            for x,v in enumerate(row):s+=cell(x,y,[a,INK,'#ff967b'][int(v)] if name in ('samegame','flood') else a if v=='1' else '#344b68',n)
        if name=='flip':s+=path('M62 32 H178 l-10 -9 M194 224 H78 l10 9','none',INK,4)
        return s
    if name=='galaxies':
        return grid()+path('M128 49 V88 H88 V168 H168 V128 H208 M48 128 H88 M128 168 V208','none',a,7)+circle(128,128,12,INK)+circle(68,188,7,a)+circle(188,68,7,a)
    if name in ('guess','guessdots'):
        for y,row in enumerate([[0,1,2,3],[2,1,0,3],[2,0,1,3]]):
            for x,v in enumerate(row):s+=circle(62+x*40,68+y*55,15,[a,'#ffba69','#c59cff','#ff869e'][v])
        if name=='guess':s+=text(211,68,'?',22)+circle(211,122,4,INK)+circle(211,133,4,INK)+circle(211,177,4,a)
        else:s+=path('M40 210 H208','none',a,4)+star(208,210,9)
        return s
    if name=='inertia':
        return grid()+cell(0,3,a)+cell(1,1,'#3b506a')+cell(2,2,'#3b506a')+star(188,68,15,a)+star(68,108,12,a)+circle(68,188,13,INK)+path('M68 161 V148 H188 V97','none',INK,5)
    if name=='lightup':
        return grid()+cell(0,1,a)+cell(1,1,a)+cell(2,1,a)+cell(3,1,a)+cell(2,0,a)+cell(2,2,a)+cell(2,3,a)+circle(148,108,14,INK)+line(141,126,155,126,DARK,5)+cell(1,3,DARK)+text(108,188,2,24)
    if name in ('loopy','pearl','slant','untangle','net','netslide'):
        if name in ('net','netslide'):
            for x,y in [(68,68),(128,68),(188,68),(68,128),(128,128),(188,128),(68,188),(128,188),(188,188)]:s+=circle(x,y,8,a)
            s+=path('M68 68 H128 V188 H188 V128 H68 V188 M128 68 H188','none',INK,9)
            s+=circle(128,128,14,a)+circle(128,128,6,DARK)
            if name=='netslide':s+=path('M42 224 H189 l-12 -10 M189 224 l-12 10','none',a,5)
        elif name=='slant':
            s=grid()
            for y,row in enumerate(['0101','1110','0011','1000']):
                for x,v in enumerate(row):s+=line(53+x*40,53+y*40+(30 if v=='1' else 0),83+x*40,53+y*40+(0 if v=='1' else 30),a,5)
            s+=circle(128,128,16,DARK)+text(128,128,3,23)
        elif name=='untangle':
            pts=[(128,45),(207,104),(176,198),(80,198),(49,104)]
            for i,j in [(0,2),(0,3),(1,3),(1,4),(2,4)]:s+=line(*pts[i],*pts[j],a,5)
            for x,y in pts:s+=circle(x,y,13,INK)+circle(x,y,5,a)
        else:
            s=path('M68 68 H188 V188 H128 V128 H68 Z','none',a,10)
            if name=='pearl':s+=circle(128,68,12,INK)+circle(188,128,12,DARK,INK)+circle(128,188,12,INK)
            else:
                for y in range(5):
                    for x in range(5):s+=circle(48+x*40,48+y*40,2,INK)
                s+=text(108,88,2,21)+text(168,168,3,21)+text(88,168,1,21)
        return s
    if name=='magnets':
        for x,y,rot in [(51,63,0),(141,63,0)]:
            body=rect(x,y,66,130,a,19)+rect(x+8,y+8,50,49,INK,12)+text(x+33,y+32,'+',32,DARK)+text(x+33,y+96,'−',32,DARK)
            s+=f'<g transform="translate(0 0) rotate({rot} {x+33} {y+65})">{body}</g>' if rot else body
        return s
    if name in ('map','palisade','rect'):
        s=grid()
        for x,y,w,h,c in [(48,48,80,80,a),(132,48,76,40,INK),(132,92,76,76,'#c49aff'),(48,132,80,76,'#ffbb69'),(132,172,76,36,a)]:s+=rect(x,y,w,h,c,5,DARK,3)
        if name=='rect':s+=text(88,88,4,32,DARK)+text(170,130,4,32,DARK)+text(88,168,4,32,DARK)
        if name=='palisade':s+=path('M48 128 H128 V208 M128 88 H208','none',INK,7)+text(88,88,5,28,DARK)
        return s
    if name=='pegs':
        for y in range(5):
            for x in range(5):
                if (x not in (0,4) or y not in (0,4)):s+=circle(64+x*32,64+y*32,12,a if (x,y)!=(2,2) else '#344b68')
        return s+path('M128 189 V146 l-9 10 M128 146 l9 10','none',INK,4)
    if name=='signpost':
        s=grid(3)
        for x,y,ang in [(75,75,0),(128,75,45),(181,75,90),(181,128,135),(181,181,180),(128,181,225),(75,181,270),(75,128,0)]:
            s+=f'<g transform="rotate({ang} {x} {y})">'+path(f'M{x-13} {y} H{x+12} l-8 -8 M{x+12} {y} l-8 8','none',a,5)+'</g>'
        return s+text(128,128,9,29)
    if name=='singles':
        s=grid(3)
        for i,v in enumerate([1,2,1,2,1,3,1,3,2]):
            x=75+(i%3)*53;y=75+(i//3)*53
            s+=circle(x,y,22,a) if i in (0,4,8) else ''
            s+=text(x,y,v,29,DARK if i in (0,4,8) else INK)
        return s
    if name=='tents':
        s=grid(3)
        for x,y in [(76,77),(181,181)]:s+=rect(x-4,y,8,17,'#b0df78',1)+path(f'M{x} {y-22} l-18 30 h36 Z','#b0df78','none')
        for x,y in [(128,78),(128,181)]:s+=path(f'M{x} {y-17} l-22 35 h44 Z',a,'none')+path(f'M{x} {y-2} l-8 20 h16 Z',DARK,'none')
        return s
    if name=='tracks':
        s=path('M48 82 H133 Q176 82 176 124 V208','none',a,24)+path('M48 82 H133 Q176 82 176 124 V208','none',DARK,13)
        for x in range(54,139,18):s+=line(x,65,x,99,INK,5)
        for y in range(139,207,18):s+=line(159,y,193,y,INK,5)
        return s+rect(49,134,64,41,a,8)+circle(65,180,9,INK)+circle(99,180,9,INK)+rect(60,142,22,17,DARK,3)
    if name=='twiddle':
        for y in range(3):
            for x in range(3):s+=cell(x,y,[a,INK,'#c49aff'][(x+y)%3],3,7)
        return s+path('M212 66 Q238 126 210 184 l16 -5 M210 184 l-4 -16','none',a,5)
    if name=='undead':
        return path('M67 188 V104 Q67 55 113 55 Q160 55 160 104 V188 l-24 -15 -22 15 -24 -15 Z',a,'none')+circle(97,111,10,DARK)+circle(132,111,10,DARK)+path('M178 66 l28 28 -28 28 M178 164 l28 28','none',INK,7)+circle(100,153,8,DARK)
    if name in ('blockout','bloq'):
        for x,y,c in [(58,137,a),(98,137,a),(98,97,a),(138,137,INK),(138,177,INK),(178,177,INK)]:
            s+=rect(x,y,34,34,c,4)+path(f'M{x} {y} l8 -8 h34 l-8 8 Z',c,'none')+path(f'M{x+34} {y} l8 -8 v34 l-8 8 Z','#758ca5','none')
        if name=='blockout':s=path('M43 73 L89 36 H215 V180 L177 220 H43 Z','none','#48607c',3)+s
        return s
    if name=='brogue':
        return path('M46 88 V50 H102 M154 50 H210 V109 M210 157 V207 H158 M102 207 H46 V158','none',a,13)+text(129,128,'@',105)+star(190,86,9,'#ffbb69')+rect(53,145,24,9,'#48607c',2)
    if name=='meritous':
        return path('M128 41 L201 128 L128 215 L55 128 Z',a,'none')+path('M128 41 V215 M55 128 H201 M128 41 L93 128 L128 215 L163 128 Z','none',DARK,4)+circle(128,128,24,DARK)+circle(128,128,12,INK)+circle(128,128,5,a)+star(53,55,10)+star(207,201,9)
    if name=='opengolf':
        return path('M49 150 Q101 99 209 139 L186 196 Q113 228 48 183 Z',a,'none')+circle(154,152,16,DARK)+line(156,151,156,49,INK,5)+path('M159 50 h45 l-17 26 h-28 Z','#ffbb69','none')+circle(82,175,12,INK)+path('M87 149 Q103 121 127 137','none',INK,3)
    if name=='whatajong':
        for x,y,rot in [(46,77,-13),(108,57,11)]:
            tile=rect(x,y,82,122,INK,10,'#c6d5dd',3)+rect(x+3,y+110,76,9,a,4)
            for yy in (y+30,y+62,y+92):tile+=line(x+27,yy-9,x+27,yy+9,'#2d947d',7)+line(x+55,yy-9,x+55,yy+9,'#2d947d',7)
            s+=f'<g transform="rotate({rot} {x+41} {y+61})">{tile}</g>'
        return s
    if name in ('sopwith','obstacleflight'):
        s=path('M45 128 L112 108 L129 54 L145 54 L145 108 L205 120 L214 137 L146 133 L142 174 L163 188 V198 L130 188 L102 198 V188 L120 174 L114 135 L45 143 Z',INK,'none')
        if name=='sopwith':s+=line(84,96,177,96,a,10)+line(87,151,173,151,a,10)+circle(127,212,5,a)+circle(141,212,5,a)
        else:s=path('M48 55 H88 V90 M208 55 H168 V90 M48 202 H88 V167 M208 202 H168 V167','none',a,7)+s
        return s
    if name in ('ardubullets','space-invaders','nightraid','evasion'):
        inv=['..x...x..','...x.x...','..xxxxx..','.xx.x.xx.','xxxxxxxxx','x.xxxxx.x','x.x...x.x','...x.x...']
        if name=='space-invaders':return pixels(inv,65,60,14,a)+path('M60 207 H94 V192 H112 V177 H144 V192 H162 V207 H196','none',INK,8)
        if name=='ardubullets':
            s=path('M128 123 l-31 66 31 -15 31 15 Z',INK,'none')
            for x,y in [(74,72),(128,49),(182,72),(56,112),(200,112)]:s+=rect(x-4,y,8,23,a,4)
            return s+path('M121 201 V217 M135 201 V217','none',a,5)
        if name=='nightraid':return circle(189,66,26,a)+pixels(inv,68,95,12,INK)+path('M44 210 H212','none',a,7)+rect(145,177,34,28,a,3)+rect(57,188,33,17,a,3)
        return path('M128 66 L156 120 L128 176 L100 120 Z',INK,'none')+circle(68,79,11,a)+circle(188,160,11,a)+circle(72,194,11,a)+path('M176 52 V101 M51 130 H82 M163 209 H208','none',a,8)
    if name in ('ardudash','runr','hopper','hollowseeker','lode-runner'):
        s=figure(111,71,INK,True)+path('M41 198 H99 M133 198 H215','none',a,12)
        if name=='ardudash':s+=path('M165 186 l19 -35 20 35 Z',a,'none')+line(45,114,76,114,a,4)+line(55,140,78,140,a,4)
        elif name=='runr':s+=rect(161,155,28,38,a,4)+path('M151 105 l17 -14 -17 -14 M166 105 l17 -14 -17 -14','none',a,5)
        elif name=='hopper':s+=path('M148 153 Q183 72 200 95 l-13 -8 M200 95 l-3 -15','none',a,5)+rect(183,179,32,14,a,3)
        elif name=='hollowseeker':s+=path('M49 49 V174 H64 V157 H82 V174 H98 M157 49 H207 V146 H185 V174 H158','none',a,12)
        else:s+=line(176,50,176,192,a,7)+line(202,50,202,192,a,7)+''.join(line(176,y,202,y,a,5) for y in range(61,191,23))+star(59,161,12,'#ffbb69')
        return s
    if name=='snek':
        return path('M58 64 H126 V111 H82 V164 H167 V113 H199','none',a,26)+circle(200,113,18,a)+circle(202,106,4,DARK)+path('M219 115 h12 l8 -6 M231 115 l8 6','none',INK,4)+circle(178,58,13,'#ff859d')+line(180,44,186,35,INK,4)
    if name=='microtd':
        return path('M38 174 H103 V86 H216','none','#49617d',24)+rect(129,130,57,66,INK,5)+rect(129,118,13,24,INK,2)+rect(151,118,13,24,INK,2)+rect(173,118,13,24,INK,2)+circle(157,160,14,a)+path('M157 155 l29 -34','none',a,9)+circle(72,174,9,a)+circle(104,122,9,a)+circle(153,86,9,'#ff859d')
    if name=='chribocchi':
        return path('M63 159 V73 L94 91 Q128 71 162 91 L193 73 V159 Q193 198 128 198 Q63 198 63 159 Z',a,'none')+circle(100,132,8,DARK)+circle(156,132,8,DARK)+path('M121 151 h14 l-7 8 Z',DARK,'none')+path('M128 160 q-10 15 -21 2 M128 160 q10 15 21 2','none',DARK,4)+path('M73 151 L45 145 M73 163 L45 171 M183 151 L211 145 M183 163 L211 171','none',INK,4)
    if name=='knightmove':
        return path('M71 204 H187 L171 177 H95 Z',INK,'none')+path('M95 172 L98 137 L134 108 L107 111 L90 127 L71 106 L108 58 L150 45 L155 66 Q189 96 173 135 L163 173 Z',a,'none')+circle(131,82,5,DARK)+path('M73 216 H185','none',a,5)
    if name=='lasers':
        return path('M47 67 H128 V168 H206','none',a,11)+path('M102 94 l53 -53 M102 194 l53 -53','none',INK,8)+circle(49,67,16,INK)+circle(207,168,17,INK)+circle(207,168,8,a)+star(128,67,9)+star(128,168,9)
    if name=='psicolo':
        return rect(49,55,98,98,a,17)+circle(76,82,8,DARK)+circle(120,82,8,DARK)+circle(98,104,8,DARK)+circle(76,126,8,DARK)+circle(120,126,8,DARK)+rect(111,121,98,98,INK,17,DARK,5)+circle(138,148,8,DARK)+circle(182,148,8,DARK)+circle(138,192,8,DARK)+circle(182,192,8,DARK)+star(192,70,12,a)
    if name=='randocity':
        for x,y,w,h,c in [(47,120,42,81,a),(94,75,47,126,INK),(146,111,36,90,a),(187,140,24,61,INK)]:
            s+=rect(x,y,w,h,c,3)
            for yy in range(y+12,y+h-9,18):
                for xx in range(x+9,x+w-7,16):s+=rect(xx,yy,7,8,DARK,1)
        return s+line(39,210,218,210,a,5)
    if name=='shooter':
        return rect(63,155,131,35,a,16)+circle(89,176,9,DARK)+circle(126,176,9,DARK)+circle(167,176,9,DARK)+rect(103,126,53,32,INK,5)+path('M130 125 L177 92','none',INK,12)+circle(194,71,14,a)+path('M47 74 H102 L117 92 H33 Z',a,'none')+rect(61,54,27,20,INK,7)
    if name=='toyokumono':
        return circle(91,90,29,INK)+circle(124,72,38,INK)+circle(161,90,30,INK)+rect(86,89,80,29,INK,8)+path('M94 139 l-7 14 M130 135 l-7 14 M164 139 l-7 14','none',a,7)+path('M71 212 H192 M126 211 V180 M126 189 Q94 190 93 168 Q118 165 126 189 M126 179 Q132 153 155 160 Q156 180 126 179',a,a,5)+circle(108,88,4,DARK)+circle(143,88,4,DARK)+path('M117 101 q9 8 18 0','none',DARK,3)
    if name=='pacman-arduman':
        return path('M152 86 A68 68 0 1 0 152 171 L106 128 Z',a,'none')+circle(111,86,6,DARK)+circle(170,128,7,INK)+circle(202,128,7,INK)+path('M175 181 V168 Q175 152 191 152 Q207 152 207 168 V181 l-8 -6 -8 6 -8 -6 Z','#ff859d','none')
    if name=='pong':
        return rect(50,72,12,107,INK,6)+rect(194,113,12,100,a,6)+circle(151,97,11,INK)+path('M81 160 L149 98 L188 125','none',a,3)+''.join(line(128,y,128,y+8,'#48607c',3) for y in range(48,209,23))
    raise ValueError('Missing icon motif: '+name)


def main():
    import cairosvg
    games = json.loads((ROOT/'common/retro-games/games.json').read_text())['games']
    for game in games:
        name = game['directory']
        accent = COLORS[int(hashlib.sha256(name.encode()).hexdigest()[:4],16)%len(COLORS)]
        svg = ('<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 256 256">'
               '<defs><linearGradient id="bg" x2="0.8" y2="1"><stop stop-color="#243b59"/><stop offset="1" stop-color="#101c30"/></linearGradient></defs>'
               +rect(0,0,256,256,'url(#bg)',52)+rect(1,1,254,254,'none',51,'#425570',2)
               +motif(name,accent)+'</svg>\n')
        assets=ROOT/name/'assets'
        assets.mkdir(exist_ok=True)
        (assets/'icon.svg').write_text(svg)
        cairosvg.svg2png(bytestring=svg.encode(),write_to=str(assets/'icon.png'),output_width=512,output_height=512)
        (assets/'icon-license.txt').write_text('Original launcher artwork. Copyright (c) 2026 lucinhu.\n'
            'SPDX-License-Identifier: MIT\n\nPermission is hereby granted, free of charge, to any person obtaining a copy\n'
            'of this artwork and associated source, to deal in the artwork without\n'
            'restriction, including without limitation the rights to use, copy, modify,\n'
            'merge, publish, distribute, sublicense, and/or sell copies, and to permit\n'
            'persons to whom it is furnished to do so, subject to the following conditions:\n\n'
            'The above copyright notice and this permission notice shall be included in\n'
            'all copies or substantial portions of the artwork.\n\n'
            'THE ARTWORK IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n'
            'IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n'
            'FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n'
            'AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n'
            'LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n'
            'OUT OF OR IN CONNECTION WITH THE ARTWORK OR THE USE OR OTHER DEALINGS IN IT.\n')
        metadata=ROOT/name/'package.json'
        data=json.loads(metadata.read_text())
        data['icon']='assets/icon.png'
        metadata.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')
        print(name)


if __name__ == '__main__':
    main()

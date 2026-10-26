/* SPDX-License-Identifier: MIT. PXA text/touch frontend for unchanged Whatajong rules.
 * Stage transitions mirror routes/run/{runGameOver,runReward,runShop}.tsx.
 * No solver, score override, alternate board, or alternate matching rules. */
import { Database } from '@/lib/in-memoriam'
import { deckTileIndexes, getAvailablePairs, getCard,
  getPenalty, isFree, isWind, selectTile, type Deck, type Tile, type TileDb } from '@/lib/game'
import { initializeDeckState } from '@/state/deckState'
import { initialGameState, type Game } from '@/state/gameState'
import { initializeTileState } from '@/state/tileState'
import { initialRunState, generateRound, getLevels, generateItems, calculateIncome,
  buyTile, upgradeTile, getNextMaterial, REROLL_COST, type Path, type TileItem } from '@/state/runState'
import { shuffle } from '@/lib/rand'
import Rand from 'rand-seed'
import { uniqueBy } from 'remeda'

declare function pxaRect(x:number,y:number,w:number,h:number,color:number):void
declare function pxaText(x:number,y:number,text:string,color:number,scale:number):void
declare function pxaTrace(text:string):void
declare function pxaRandom():number
declare function __pxaAdvanceTimers(ms:number):number
const WIDTH=800, HEIGHT=600
let run:any, game:Game, deck:Deck, tiles:TileDb, levels:any[], round:any
let screen='menu', modal:TileItem|null=null, elapsed=0, lastTick=0
let rewards:TileItem[]=[], message='', dirty=true
type Button={x:number,y:number,w:number,h:number,fn:()=>void}
let buttons:Button[]=[]
const xy=(t:Tile)=>({x:32+t.x*38+t.z*5,y:94+t.y*26-t.z*4})
function button(x:number,y:number,w:number,label:string,fn:()=>void,enabled=true) {
  pxaRect(x,y,w,36,enabled?6:4); pxaText(x+8,y+11,label,enabled?1:3,1)
  if(enabled) buttons.push({x,y,w,h:36,fn})
}
function title(text:string) { pxaText(28,23,text,7,2) }
function mainMenu() { screen='menu';elapsed=0 }
function newRun(difficulty:string) {
  run=initialRunState(difficulty+'-pxa-'+Math.floor(pxaRandom()*1e9))
  deck=new Database(deckTileIndexes); initializeDeckState(deck)
  levels=getLevels(run.runId); startRound(); screen='run'; message=''
}
function startRound() {
  round=generateRound(run.round,run)
  game=initialGameState(run.runId+'-'+run.round)
  tiles=new Database((requireTileIndexes as any))
  initializeTileState(run.runId+'-'+run.round,deck.all,tiles)
  elapsed=0; run.stage='game'; modal=null
}
// The original index callbacks are imported, rather than rebuilding its database.
import { tileIndexes as requireTileIndexes } from '@/lib/game'
function win() { return game.endCondition==='empty-board' && game.points-getPenalty(game.time,round)>=round.pointObjective }
function nextStage() {
  if(!win()) return
  const total=game.points-getPenalty(game.time,round)
  const extra=Math.min(Math.floor(Math.max(0,total/round.pointObjective-1)*2),12)
  run.money+=calculateIncome(run)+game.coins+extra; run.totalPoints+=total
  const level=levels.find(l=>l.level===run.round)
  run.stage=!level?'end':level.rewards>0?'reward':'shop'
  if(run.stage==='reward') {
    rewards=shuffle(uniqueBy(level.tileItems,(i:any)=>i.cardId),new Rand(run.runId+'-'+run.round)).slice(0,level.rewards)
  }
  enterShop()
}
function enterShop() {
  if(run.stage==='shop' && run.freeze?.active) {
    const capturedFreeze=run.freeze
    setTimeout(()=>{capturedFreeze.active=false;dirty=true},2000)
  }
}
function receiveRewards() {
  for(const item of rewards) buyTile({run,item,deck,reward:true})
  rewards=[]; run.stage='shop'; enterShop()
}
function continueRound() { run.round++; startRound() }
function purchase(item:TileItem) {
  if(run.freeze?.active || item.cost>run.money || deck.size>=163) return
  const same=deck.filterBy({cardId:item.cardId})
  if(getCard(item.cardId).colors.some(color=>getNextMaterial(same,color)!=='bone')) modal=item
  else buyTile({run,item,deck})
}
function upgrade(path:Path) { if(modal) upgradeTile({run,item:modal,deck,path}); modal=null }
function reroll() {
  if(run.money<REROLL_COST)return
  if(run.freeze && !run.freeze.active)run.freeze=undefined
  run.money-=REROLL_COST; run.reroll++
}
function freeze() {
  if(run.freeze)run.freeze.active=!run.freeze.active
  else run.freeze={round:run.round,reroll:run.reroll,active:true}
}
function drawTile(t:Tile) {
  const p=xy(t),free=isFree(tiles,t,game)
  pxaRect(p.x+3,p.y+4,70,50,4)
  pxaRect(p.x,p.y,70,50,t.selected?7:free?8:9)
  const card=getCard(t.cardId), suit=card.suit==='bam'?10:card.suit==='crack'?11:card.suit==='dot'?12:1
  pxaText(p.x+4,p.y+5,t.cardId.slice(0,8),suit,1)
  pxaText(p.x+4,p.y+27,t.material==='bone'?'PAIR':t.material.slice(0,8),free?1:3,1)
}
function state() {
  const pairs=screen==='run'&&run.stage==='game'?getAvailablePairs(tiles,game).sort((a,b)=> {
    const aw=isWind(a[0].cardId),bw=isWind(b[0].cardId)
    return aw&&!bw?1:!aw&&bw?-1:b[0].z+b[1].z-a[0].z-a[1].z
  }):[]
  const pair=pairs[0]?.map(t=>{const p=xy(t);return {id:t.id,card:t.cardId,x:p.x+35,y:p.y+20}})||[]
  return {screen,stage:run?.stage||'menu',seed:run?.runId,round:run?.round||0,
    points:game?.points||0,time:game?.time||0,penalty:game?getPenalty(game.time,round):0,
    objective:round?.pointObjective||0,money:run?.money||0,deck:deck?.size||0,
    freeze:run?.freeze?{...run.freeze}:null,reroll:run?.reroll||0,retries:run?.retries||0,
    remaining:tiles?.filterBy({deleted:false}).length||0,pairs:pairs.length,pair,
    condition:game?.endCondition||null,win:game?win():false,
    rewards:rewards.map(i=>i.cardId),items:run?.stage==='shop'?generateItems(run,levels).map(i=>({card:i.cardId,cost:i.cost,id:i.id})):[],
    upgraded:deck?.all.filter(t=>t.material!=='bone').map(t=>({card:t.cardId,material:t.material}))||[]}
}
function trace(){pxaTrace('WHATAJONG STATE '+JSON.stringify(state()))}
function render() {
  buttons=[]; pxaRect(0,0,WIDTH,HEIGHT,0)
  if(screen==='menu') {
    title('WHATAJONG'); pxaText(30,84,'Original 24-round mahjong roguelike',8,2)
    pxaText(30,124,'Match free pairs. Beat each score objective.',8,1)
    pxaText(30,148,'Earn coins, collect special tiles, upgrade materials.',8,1)
    button(42,214,210,'NEW EASY RUN',()=>newRun('E'))
    button(42,266,210,'NEW MEDIUM RUN',()=>newRun('M'))
    button(42,318,210,'NEW HARD RUN',()=>newRun('H'))
    if(run)button(42,370,210,'RESUME',()=>{screen='run';enterShop()})
    button(42,438,210,'HOW TO PLAY',()=>screen='help')
    pxaText(30,534,'MIT rules / maps by Pao Ramon. PXA text frontend.',3,1)
    pxaText(30,555,'Touch controls. Silent. Save state lasts this session.',3,1)
  } else if(screen==='help') {
    title('HOW TO PLAY')
    const lines=['Tap a free tile, then a matching free tile.',
      'A tile is free if uncovered and open on its left or right.',
      'Topaz / sapphire can ignore the side constraint.',
      'Clear every tile AND reach the round point objective.',
      'Time subtracts points; dark materials pause the timer.',
      'Coins and round rewards buy pairs or material upgrades.',
      'Special cards preserve original wind / dragon / gem rules.',
      'BAM = bamboo, CRACK = characters, DOT = circles.',
      'Materials / special cards use their original identifiers.',
      'The shop has reroll ($1), freeze, and all color upgrades.',
      'MENU keeps the run; NEW RUN resets it. HOME exits.' ]
    lines.forEach((line,i)=>pxaText(30,90+i*30,line,8,1))
    button(30,500,170,'BACK',()=>screen='menu')
  } else {
    title('ROUND '+run.round+' / 24  '+run.stage.toUpperCase())
    button(672,18,100,'MENU',mainMenu)
    pxaText(30,66,'Points '+game.points+'  Penalty '+getPenalty(game.time,round)+'  Goal '+round.pointObjective+'  $'+run.money,8,1)
    if(run.stage==='game') {
      for(const t of tiles.filterBy({deleted:false}).sort((a,b)=>a.z-b.z||a.y-b.y))drawTile(t)
      if(game.endCondition) {
        pxaRect(135,188,540,183,2)
        pxaText(162,217,win()?'ROUND WON':'ROUND LOST',7,2)
        pxaText(162,254,'Final score '+(game.points-getPenalty(game.time,round))+' / '+round.pointObjective,8,1)
        pxaText(162,278,'Result: '+game.endCondition,8,1)
        button(162,312,160,win()?'CONTINUE':'RETRY',()=> {
          if(win())nextStage()
          else {run.retries++;startRound()}
        })
      } else {
        pxaText(30,537,'Available pairs '+getAvailablePairs(tiles,game).length+'  Time '+game.time+'s',8,1)
        pxaText(530,541,game.pause?'TIMER PAUSED BY MATERIAL':'TIMER RUNNING',8,1)
      }
    } else if(run.stage==='reward') {
      title('ROUND '+run.round+' REWARD')
      pxaText(40,118,'Free original special tile pairs:',8,1)
      rewards.forEach((i,n)=>pxaText(45,166+n*42,i.cardId,7,2))
      button(40,420,250,'COLLECT / GO TO SHOP',receiveRewards)
    } else if(run.stage==='shop') {
      pxaText(30,111,'Deck pairs '+deck.size+' / 163. Tap item to buy or upgrade.',8,1)
      generateItems(run,levels).forEach((item:TileItem,i:number)=> {
        pxaText(30,163+i*55,item.cardId+'  $'+item.cost,8,1)
        button(260,151+i*55,230,'BUY / UPGRADE',()=>purchase(item),!run.freeze?.active&&item.cost<=run.money&&deck.size<163)
      })
      button(530,151,230,'REROLL $1',reroll,run.money>=REROLL_COST)
      button(530,207,230,run.freeze?.active?'UNFREEZE':'FREEZE SHOP',freeze)
      button(530,431,230,'NEXT ROUND',continueRound)
      pxaText(30,482,'Upgrades merge 3 same materials into the next tier.',3,1)
      if(modal) {
        buttons=[]
        pxaRect(120,172,560,258,2); pxaText(145,192,'CHOOSE MATERIAL PATH',7,2)
        const card=getCard(modal.cardId)
        card.colors.forEach((color,i)=>button(145,231+i*40,420,color+' -> '+getNextMaterial(deck.filterBy({cardId:modal!.cardId}),color),()=>upgrade(color)))
        button(145,386,160,'CANCEL',()=>modal=null)
      }
    } else {
      pxaText(80,170,'RUN COMPLETE! Total points '+run.totalPoints,7,2)
      button(80,280,200,'MAIN MENU',mainMenu)
    }
    if(message)pxaText(30,580,message.slice(0,90),11,1)
  }
  dirty=false
}
function touch(x:number,y:number) {
  const hit=[...buttons].reverse().find(b=>x>=b.x&&x<b.x+b.w&&y>=b.y&&y<b.y+b.h)
  try {
    if(hit)hit.fn()
    else if(screen==='run'&&run.stage==='game'&&!game.endCondition) {
      const tile=tiles.filterBy({deleted:false}).sort((a,b)=>b.z-a.z||b.y-a.y).find(t=> {
        const p=xy(t);return x>=p.x&&x<p.x+70&&y>=p.y&&y<p.y+50
      })
      if(tile&&isFree(tiles,tile,game))selectTile({tileDb:tiles,game,tileId:tile.id})
    }
    message=''
  }catch(error){message=String(error);pxaTrace('WHATAJONG ERROR '+message)}
  dirty=true;render();trace()
}
function tick(ms:number) {
  if(__pxaAdvanceTimers(ms))dirty=true
  const delta=lastTick?Math.max(0,ms-lastTick):0;lastTick=ms
  if(screen==='run'&&run.stage==='game'&&!game.pause&&!game.endCondition) {
    elapsed+=delta
    if(elapsed>=1000){game.time+=Math.floor(elapsed/1000);elapsed%=1000;dirty=true}
  }
  if(dirty)render()
}
Object.assign(globalThis,{whatajongTouch:touch,whatajongTick:tick,whatajongRender:render})
render();trace()

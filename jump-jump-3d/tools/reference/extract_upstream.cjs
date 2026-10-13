// Run the original method itself without WeChat/WebGL. Usage:
// node extract_upstream.cjs /path/to/weapp-jump/game.js > upstream-motion.json
const fs = require('fs');
const vm = require('vm');
const source = fs.readFileSync(process.argv[2], 'utf8');
const match = source.match(/key: "_jump", value: (function \(e\) \{[\s\S]*?)\n                }/);
if (!match) throw Error('upstream _jump not found');
const jump = vm.runInNewContext('(' + match[1] + '\n})', {
    r: {Vector3: function(x,y,z) {this.x=x;this.y=y;this.z=z;}},
    o: {GAME: {gravity: 720}}
});
const rows = [];
for (const charge of [0, .05, .4, .7, 1, 2.2, 3]) {
    const velocity = {vz: Math.min(charge*70,150), vy: Math.min(135+charge*15,180)};
    const start = 5.5*Math.max(.5, 1-.32*charge)-2.75;
    let y=start, x=0;
    const bottle = {velocity, flyingTime:0, axis:{}, obj:{
        translateY(delta) {y+=delta;}, translateOnAxis(axis,delta) {x+=delta;}
    }};
    for (const t of [.05,.14,.25,.32]) {
        jump.call(bottle, t-bottle.flyingTime);
        rows.push({charge,t,x:x/10,y:y/10+.275,vy:velocity.vy/10});
    }
}
process.stdout.write(JSON.stringify({commit:'8102017c5357ae18a0df2eece2e09771e8648b66',
    source:'game.js:_jump (lines 12350–12353)', rows},null,2)+'\n');

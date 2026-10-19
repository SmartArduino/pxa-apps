var index=0;function assert(v,name){++index;if(!v)throw Error('numeric case '+index+' '+(name||''));}
assert(+("0x10")===16&&parseFloat('1.25e2tail')===125&&parseInt('-0xff',0)===-255);
assert(Number('Infinity')===Infinity&&Number('-Infinity')===-Infinity&&Number('')===0);
assert(Object.is(-0,-0)&&!Object.is(0,-0));
assert(((v=1.25e2)=>v)()===125);
assert(((v=.5_0)=>v)()===.5);
assert(((v=1_000_000)=>v)()===1000000);
assert(((v=0xFF_FFn)=>v)()===65535n);
assert(((v=123_456_789_012_345_678_901_234_567_890_123_456_789_012_345_678_901_234_567_890n)=>String(v))()==='123456789012345678901234567890123456789012345678901234567890');
assert(((v=1e9999)=>v)()===Infinity);
assert(((v=1e-9999)=>v)()===0);
assert(((v=0b1010_1010)=>v)()===170);
assert(((v=0o7_777)=>v)()===4095);
assert(((v=[1,2,{n:3.14}])=>v[2].n)()===3.14);
assert((function(){return 010;})()===8);
for(const text of ['(v=1.5n)=>v','(v=1e2n)=>v','(v=00n)=>v','(v=01n)=>v','(v=0x)=>v','(v=0b2)=>v','(v=1__2)=>v','(v=1_)=>v','(v=0_1)=>v','(v=1e+) =>v','(v=1e_2)=>v','(v=1._0)=>v']) {
 let caught=false;try{eval(text);}catch(e){caught=e instanceof SyntaxError;}assert(caught,text);
}
assert(((v=42)=>v)()===42);
assert((()=>7)()===7);
assert(((...x)=>x.length)(1,2)===2);
assert((({a=3}={})=>a)()===3);
assert((([a=4]=[])=>a)()===4);
assert(((π)=>π)(2)===2);
assert(((a,)=>a)(2)===2);
assert(typeof (async (a=3)=>a)==='function');
assert((function(){return 13;})()===13);
assert((1+2)*3===9&&(((1)))===1&&(!false)===true);
for(const text of ['(1)=>1','((a))=>a','(function(){})=>1','(...a,)=>a','(a)\n=>a']){
 let caught=false;try{eval(text);}catch(e){caught=e instanceof SyntaxError;}assert(caught,text);
}

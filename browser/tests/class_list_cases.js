// Shared behavioral cases for the signed runtime and an independent browser.
// Covers DOMTokenList ordering, live views, conversion, errors and iteration.
// Expected data follows https://dom.spec.whatwg.org/#interface-domtokenlist and
// Web IDL indexed-property rules, including missing empty attributes and
// rejecting writes to read-only indices (some engines differ on these cases).
function runClassListCases() {
    const rows=[];
    function test(name,fn) {try{rows.push({name,result:fn()});}catch(e){rows.push({name,error:e.name});}}
    function node(raw) {const n=document.createElement('div');if(raw!==undefined)n.setAttribute('class',raw);return n;}
    function caught(fn) {try{const value=fn();return ['ok',value===undefined?null:value];}catch(e){return [e.name,e.code||0,e instanceof DOMException,e instanceof Error];}}
    test('empty and identity',()=>{const n=node(),l=n.classList;return [l===n.classList,l.value,l.length,n.getAttribute('class'),l.item(0),l[0]??null,Object.keys(l),l instanceof DOMTokenList,Object.prototype.toString.call(l)];});
    test('ASCII whitespace and duplicate order',()=>{const n=node(' \ta a\nb\rc\fd\u00a0z\v '),l=n.classList;return [l.value,Array.from(l),l.length,String(l),l.contains('a'),l.contains('A'),l.contains('d\u00a0z\v')];});
    test('item unsigned conversion',()=>{const l=node('a b c').classList;return [l.item(1.9),l.item(-1),l.item(NaN),l.item(Infinity),l.item(4294967296),l.item('2'),l.item(null),l[0],l['01']??null];});
    test('add all and normalize',()=>{const n=node(' a a\tb '),l=n.classList;const r=l.add('c','a','d','c');return [r??null,n.className,Array.from(l)];});
    test('remove all and normalize',()=>{const n=node(' a a\tb c '),l=n.classList;const r=l.remove('a','missing','a');return [r??null,n.className,Array.from(l)];});
    test('empty calls and missing attribute',()=>{const n=node(),l=n.classList;l.add();l.remove();l.remove('x');const a=n.getAttribute('class');l.value='';l.add();return [a,n.getAttribute('class'),l.length];});
    test('empty calls normalize existing attribute',()=>{const n=node(' a a\tb ');n.classList.add();const a=n.className;n.className=' a a\tb ';n.classList.remove();return [a,n.className];});
    test('toggle force including undefined',()=>{const n=node(),l=n.classList,r=[];for(const args of [['a'],['a'],['a',true],['a',true],['a',false],['a',false],['a',undefined],['a',undefined],['a',null],['a',1],['a',0]])r.push([l.toggle(...args),n.className]);return r;});
    test('toggle no-op retains raw spelling',()=>{const n=node(' a a\tb '),l=n.classList;const a=l.toggle('a',true),b=n.className,c=l.toggle('missing',false);return [a,b,c,n.className,Array.from(l)];});
    test('replace earlier and later duplicates',()=>{const r=[];for(const pair of [['a','c'],['c','a'],['b','b'],['z','a'],['a','z']]){const n=node(' a b c d ');r.push([n.classList.replace(...pair),n.className,Array.from(n.classList)]);}return r;});
    test('contains does not validate token',()=>{const l=node('a B x\u00a0y').classList;return ['', 'a b','a\n','B','b','x\u00a0y'].map(t=>caught(()=>l.contains(t)));});
    test('invalid add is atomic',()=>{const n=node('original'),l=n.classList;const r=[caught(()=>l.add('valid','')),caught(()=>l.add('valid','bad token')),caught(()=>l.add('bad token',''))];return [r,n.className,Array.from(l)];});
    test('invalid remove is atomic',()=>{const n=node('original extra'),l=n.classList;const r=[caught(()=>l.remove('original','')),caught(()=>l.remove('original','bad\ttoken'))];return [r,n.className];});
    test('replace validates both empty tokens first',()=>{const n=node('a'),l=n.classList;const r=[caught(()=>l.replace('bad token','')),caught(()=>l.replace('','bad token')),caught(()=>l.replace('a','x\ny')),caught(()=>l.replace('missing',''))];return [r,n.className];});
    test('toggle invalid',()=>{const l=node('a').classList;return [caught(()=>l.toggle('')),caught(()=>l.toggle('a\fb',true)),Array.from(l)];});
    test('reflection and same object',()=>{const n=node('a'),l=n.classList;n.className='b b c';const a=Array.from(l);n.setAttribute('class','d\te');const b=[l[0],l[1],l.length];n.removeAttribute('class');return [a,b,l.value,l.length,l===n.classList,n.hasAttribute('class')];});
    test('live iterator and terminal done',()=>{const n=node('a b'),l=n.classList,i=l.values(),r=[i.next()];l.add('c');r.push(i.next());l.remove('c');l.add('d');r.push(i.next(),i.next());l.add('e');r.push(i.next());return r;});
    test('keys entries and iterator alias',()=>{const l=node('a b').classList;return [Array.from(l.keys()),Array.from(l.entries()),Array.from(l),l[Symbol.iterator]===l.values];});
    test('forEach sees mutations and receiver',()=>{const n=node('a b c'),l=n.classList,receiver={},r=[];const v=l.forEach(function(value,index,list){r.push([value,index,this===receiver,list===l]);if(value==='a')l.remove('b');if(value==='c')l.add('d');},receiver);return [r,v??null];});
    test('own indices and expandos',()=>{const l=node('a b').classList;l.note='ok';const a=Object.keys(l),d=Object.getOwnPropertyDescriptor(l,'0');return [a,d,l.note,'0' in l,'2' in l,Object.hasOwn(l,'1')];});
    test('index writes and deletes',()=>{const l=node('a b').classList;return [Reflect.set(l,'0','z'),Reflect.set(l,'8','z'),Reflect.defineProperty(l,'1',{value:'z'}),Reflect.defineProperty(l,'9',{value:'z'}),Reflect.deleteProperty(l,'0'),Reflect.deleteProperty(l,'9'),Array.from(l),Object.keys(l)];});
    test('extensions cannot be prevented',()=>{const l=node('a').classList;return [Reflect.preventExtensions(l),Object.isExtensible(l),caught(()=>Object.freeze(l))];});
    test('required arguments and supports',()=>{const l=node('a').classList;return [caught(()=>l.item()),caught(()=>l.contains()),caught(()=>l.toggle()),caught(()=>l.replace('a')),caught(()=>l.supports()),caught(()=>l.supports('a')),caught(()=>l.supports(''))];});
    test('value and forwarded assignment',()=>{const n=node(),l=n.classList;l.value=' a a ';const a=[l.value,Array.from(l)];n.classList=' b b\tc ';const b=[l.value,Array.from(l),l===n.classList];l.value=null;const c=l.value;l.value=undefined;return [a,b,c,l.value];});
    test('DOMString scalar conversions',()=>{const n=node(),l=n.classList;l.add(12,null,undefined,1n,true);return [Array.from(l),caught(()=>l.add(Symbol('x'))),caught(()=>l.contains(Symbol('x'))),caught(()=>l.value=Symbol('x')),caught(()=>l.item(1n)),caught(()=>l.item(Symbol('x')))];});
    test('conversion side effects precede token reads',()=>{const n=node('a'),l=n.classList;const token={toString(){n.className='b';return 'c';}};l.add(token);const a=n.className;const index={valueOf(){n.className='x y';return 1;}};return [a,l.item(index),n.className];});
    test('all argument conversions precede validation',()=>{const n=node('a'),l=n.classList;const mark={toString(){n.className='external';return 'x';}};const a=caught(()=>l.add('',mark));const b=n.className;const bad={toString(){throw new TypeError('conversion');}};const c=caught(()=>l.add('',bad));return [a,b,c,n.className];});
    test('removed final token leaves attribute',()=>{const n=node('a'),l=n.classList;l.remove('a');return [n.hasAttribute('class'),n.className,l.value,l.length,Object.keys(l)];});
    test('constructor and method brands',()=>{const l=node('a').classList;return [caught(()=>new DOMTokenList()),caught(()=>DOMTokenList()),caught(()=>l.add.call({})),caught(()=>l.contains.call(null,'a')),caught(()=>Object.getOwnPropertyDescriptor(DOMTokenList.prototype,'length').get.call({}))];});
    test('DOMException identity and legacy codes',()=>{const r=[];for(const name of ['SyntaxError','InvalidCharacterError','SecurityError','TypeMismatchError','DOMStringSizeError','ValidationError','Unknown','']){const e=new DOMException('message',name);r.push([e.name,e.message,e.code,e instanceof DOMException,e instanceof Error,Object.prototype.toString.call(e),String(e)]);}return [r,DOMException.SYNTAX_ERR,DOMException.INVALID_CHARACTER_ERR,DOMException.prototype.TIMEOUT_ERR];});
    test('selector synchronization',()=>{const h=node(),n=node('a');h.appendChild(n);n.id='leaf';const l=n.classList;l.add('b');const a=[n.matches('.a.b'),h.querySelector('.b')===n];l.remove('a');const b=[n.matches('.a'),n.matches('.b')];n.className='c';return [a,b,l.contains('c'),l.contains('b'),h.querySelector('.c')===n];});
    test('ASCII validation and non-ASCII tokens',()=>{const l=node().classList,r=[];for(const token of ['a b','a\tb','a\nb','a\rb','a\fb','a\vb','a\u00a0b','中文','Case','case'])r.push(caught(()=>l.add(token)));return [r,Array.from(l)];});
    test('iteration after raw attribute replacement',()=>{const n=node('a b'),l=n.classList,i=l.entries(),r=[i.next()];n.className='x y z';r.push(i.next(),i.next());n.removeAttribute('class');r.push(i.next());return r;});
    test('numeric inherited property outside range',()=>{const n=node('a'),l=n.classList,p=Object.create(DOMTokenList.prototype);p[4]='inherited';Object.setPrototypeOf(l,p);return [l[4]??null,'4' in l,Reflect.set(l,'7','seven'),l[7]??null,Object.keys(l)];});
    test('selector syntax uses DOMException',()=>{const n=node(),r=[];for(const method of ['querySelector','querySelectorAll','matches','closest']){try{n[method]('[')}catch(e){r.push([e.name,e.code,e instanceof DOMException,e instanceof SyntaxError]);}}return r;});
    return rows;
}

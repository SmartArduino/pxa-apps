// Shared, independently executed browser/runtime fixture. ASCII data-* names
// follow https://html.spec.whatwg.org/multipage/dom.html#dom-dataset and Web IDL.
// Expected accessor rejection and string-before-symbol key order follow Web
// IDL rather than the differing results of the independent reference engine.
function runDatasetCases() {
    const rows=[];
    function test(name,fn) {try{rows.push({name,result:fn()});}catch(e){rows.push({name,error:e.name});}}
    function node() {return document.createElement('div');}
    function caught(fn) {try{const r=fn();return ['ok',r===undefined?null:r];}catch(e){return [e.name,e.code||0,e instanceof DOMException];}}
    test('empty and same object',()=>{const n=node(),d=n.dataset;return [d===n.dataset,Object.keys(d),d.missing??null,d instanceof DOMStringMap,Object.prototype.toString.call(d),Object.getOwnPropertySymbols(d).length];});
    test('attribute to camel case',()=>{const n=node();for(const [key,value] of [['data-foo-bar','a'],['data--foo','b'],['data-foo--bar','c'],['data-foo-9','d'],['data-a.b:c_d','e'],['data-','f'],['data--','g']])n.setAttribute(key,value);return Object.entries(n.dataset);});
    test('camel case to attribute',()=>{const n=node(),d=n.dataset;for(const key of ['fooBar','Foo','foo-Bar','foo-9','a.b:c_d','','-','ABC'])d[key]=key;return ['foo-bar','-foo','foo--bar','foo-9','a.b:c_d','','-','-a-b-c'].map(key=>n.getAttribute('data-'+key));});
    test('read and external updates',()=>{const n=node(),d=n.dataset;n.setAttribute('data-src','/first.png');const a=d.src;n.setAttribute('DATA-SRC','/next.png');const b=d.src;n.removeAttribute('data-src');return [a,b,d.src??null,Object.keys(d),d===n.dataset];});
    test('set and delete reflection',()=>{const n=node(),d=n.dataset;d.src='/MiXeD.png';const a=n.getAttribute('data-src'),b=delete d.src;return [a,b,n.hasAttribute('data-src'),'src' in d,Object.hasOwn(d,'src'),delete d.missing];});
    test('DOMString conversion',()=>{const n=node(),d=n.dataset;d.a=null;d.b=undefined;d.c=12;d.d=1n;d.e=true;return Object.entries(d);});
    test('symbol values reject atomically',()=>{const n=node(),d=n.dataset;d.x='old';return [caught(()=>d.x=Symbol('v')),d.x,n.getAttribute('data-x')];});
    test('syntax error preserves attrs',()=>{const n=node(),d=n.dataset;d.fooBar='old';const r=['foo-bar','-foo','foo-a-b'].map(key=>caught(()=>d[key]='new'));return [r,Object.entries(d),d['foo-bar']??null,'foo-bar' in d];});
    test('conversion precedes invalid key validation',()=>{const n=node(),d=n.dataset;let calls=0;const value={toString(){++calls;d.side='effect';return 'x';}};let a,b;try{d['bad-key']=value}catch(e){a=[e.name,e.code||0,e instanceof DOMException]}try{d['bad-key']={toString(){throw new TypeError('convert');}}}catch(e){b=[e.name,e.code||0,e instanceof DOMException]}return [a,b,calls,Object.entries(d)];});
    test('invalid characters',()=>{const d=node().dataset;return ['x y','x/y','x>y','x=y'].map(key=>caught(()=>d[key]='x'));});
    test('prototype names remain data',()=>{const n=node(),d=n.dataset,p=Object.getPrototypeOf(d);d.__proto__='safe';d.constructor='ctor';d.toString='text';d.hasOwnProperty='own';return [Object.entries(d),Object.getPrototypeOf(d)===p,n.getAttribute('data-__proto__'),d instanceof DOMStringMap];});
    test('prototype lookup restored on delete',()=>{const d=node().dataset;const a=[typeof d.toString,typeof d.constructor,'toString' in d,Object.hasOwn(d,'toString')];d.toString='x';delete d.toString;return [a,typeof d.toString,Object.hasOwn(d,'toString')];});
    test('named descriptors',()=>{const d=node().dataset;d.x='one';const a=Object.getOwnPropertyDescriptor(d,'x');d.x='two';return [a,Object.getOwnPropertyDescriptor(d,'x'),Object.getOwnPropertyDescriptor(d,'missing')??null];});
    test('define data properties',()=>{const n=node(),d=n.dataset;const a=Reflect.defineProperty(d,'one',{value:12,configurable:true});const b=Reflect.defineProperty(d,'two',{value:'v'});const c=Reflect.defineProperty(d,'three',{writable:true,configurable:true});return [a,b,c,Object.entries(d),Object.getOwnPropertyDescriptor(d,'one')];});
    test('accessor definitions reject',()=>{const d=node().dataset;d.x='old';return [Reflect.defineProperty(d,'x',{get(){return 'bad';},configurable:true}),Reflect.defineProperty(d,'y',{get(){return 'bad';},configurable:true}),Object.entries(d)];});
    test('enumeration attribute order including numbers',()=>{const n=node(),d=n.dataset;for(const key of ['z','12','2','a'])d[key]=key;d.z='new';const a=Object.keys(d);delete d['12'];d['12']='back';return [a,Object.keys(d),JSON.stringify(d)];});
    test('Object.assign and spread',()=>{const n=node(),d=n.dataset;Object.assign(d,{src:'/image.png',pageNumber:3});return [{...d},Object.assign({},d),n.getAttribute('data-page-number')];});
    test('symbols are ordinary properties',()=>{const n=node(),d=n.dataset,s=Symbol('note');d.x='v';d[s]={ok:true};const a=[d[s].ok,s in d,Object.hasOwn(d,s),Object.keys(d),Reflect.ownKeys(d).map(k=>typeof k==='symbol'?'symbol':k),n.hasAttribute('data-symbol')];delete d[s];return [a,d[s]??null,s in d];});
    test('symbol descriptors',()=>{const d=node().dataset,s=Symbol('fixed');Object.defineProperty(d,s,{value:7});return [d[s],Reflect.set(d,s,8),Reflect.deleteProperty(d,s),Object.getOwnPropertyDescriptor(d,s)];});
    test('cannot prevent extensions',()=>{const d=node().dataset;return [Reflect.preventExtensions(d),Object.isExtensible(d),caught(()=>Object.freeze(d))];});
    test('distinct element views',()=>{const a=node(),b=node(),d=a.dataset;d.x='a';b.dataset.x='b';return [d!==b.dataset,d.x,b.dataset.x,a.getAttribute('data-x')];});
    test('dataset is read only',()=>{const n=node(),d=n.dataset;const a=Reflect.set(n,'dataset',{x:'other'});return [a,n.dataset===d,Object.keys(d)];});
    test('constructor is illegal',()=>[caught(()=>new DOMStringMap()),caught(()=>DOMStringMap())]);
    test('inherited receiver writes do not change attributes',()=>{const n=node(),d=n.dataset;d.x='one';const receiver=Object.create(d);receiver.x='two';receiver.y='extra';return [receiver.x,receiver.y,d.x,d.y??null,n.getAttribute('data-x'),Object.keys(receiver)];});
    test('Reflect.set alternate receiver',()=>{const n=node(),d=n.dataset,receiver={};d.x='one';const a=Reflect.set(d,'x','two',receiver),b=Reflect.set(d,'y','new',receiver);return [a,b,receiver,d.x,Object.keys(d)];});
    test('selector synchronization',()=>{const n=node();n.dataset.pageNumber='2';const a=n.matches('[data-page-number="2"]');n.setAttribute('data-page-number','3');const b=n.dataset.pageNumber;delete n.dataset.pageNumber;return [a,b,n.matches('[data-page-number]')];});
    test('primitive conversion hint and receiver',()=>{const d=node().dataset,trace=[];const value={[Symbol.toPrimitive](hint){trace.push([hint,this===value]);return 23},toString(){throw Error('unexpected')}};d.x=value;return [d.x,trace];});
    test('ordinary conversion order and fallback',()=>{const d=node().dataset,trace=[];const value={toString(){trace.push('string');return {}},valueOf(){trace.push('value');return 42}};d.x=value;return [d.x,trace];});
    test('noncallable string method falls back',()=>{const d=node().dataset;d.x={toString:0,valueOf(){return null}};return d.x;});
    test('invalid primitive conversion preserves value',()=>{const d=node().dataset;d.x='old';return [caught(()=>d.x={[Symbol.toPrimitive]:1}),caught(()=>d.x={[Symbol.toPrimitive](){return {}}}),caught(()=>d.x={toString:0,valueOf:0}),d.x];});
    test('symbol primitive conversion rejects',()=>{const d=node().dataset;d.x='old';return [caught(()=>d.x={[Symbol.toPrimitive](){return Symbol('value')}}),caught(()=>d.x=Object(Symbol('value'))),d.x];});
    test('conversion ignores overridden call member',()=>{const d=node().dataset;function text(){return this===value?'converted':'wrong'}text.call=()=>{throw Error('wrong call')};const value={toString:text};d.x=value;return d.x;});
    test('reentrant conversion writes same key first',()=>{const d=node().dataset;d.x='old';d.x={toString(){d.x='inner';return 'outer'}};return [d.x,Object.keys(d)];});
    test('null primitive hook uses ordinary conversion',()=>{const d=node().dataset;d.x={[Symbol.toPrimitive]:null,toString(){return 'fallback'}};return d.x;});
    test('named setters override inherited descriptors',()=>{const d=node().dataset,p=Object.create(DOMStringMap.prototype);Object.defineProperty(p,'fixed',{value:'inherited'});Object.defineProperty(p,'accessor',{set(){throw Error('inherited setter')},get(){throw Error('inherited getter')}});Object.setPrototypeOf(d,p);d.fixed='own';d.accessor='safe';return [d.fixed,d.accessor,Object.keys(d),Object.getPrototypeOf(d)===p];});
    return rows;
}

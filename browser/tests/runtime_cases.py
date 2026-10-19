"""Small, synthetic JS programs for interpreter recursion and VM semantics.

Native sanitizers do not model WAMR's operand stack. The smoke driver runs each
program in a fresh real Wasm instance and requires the next script to recover.
"""


from pathlib import Path


def recursive_cases():
    return {
        'recursive-constructor': 'function T(){new T()}new T()',
        'recursive-class-constructor': 'class T{constructor(){new T()}}new T()',
        'recursive-call-thunk': 'function f(){return f.call(null)}f()',
        'recursive-apply-thunk': 'function f(){return f.apply(null,[])}f()',
        'recursive-apply-argument': 'var a={length:1,get 0(){return f.apply(null,a)}};function f(){return f.apply(null,a)}f()',
        'recursive-proxy-accessor': 'var p=new Proxy({get x(){return p.x}},{});p.x',
        'recursive-constructor-accessor':'var o={get x(){new T()}};function T(){return o.x}new T()',
        'recursive-getter-call':'var o={get x(){return f.call(null)}};function f(){return o.x}o.x',
        'recursive-getter-eval':'var o={get x(){return eval("o.x")}};o.x',
        'recursive-direct-eval':'function f(){return eval("f()")}f()',
        'recursive-indirect-eval':'var e=eval;function f(){return e("f()")}f()',
        'recursive-function': 'function f(){return f()}f()',
        'recursive-method': 'var o={f(){return this.f()}};o.f()',
        'recursive-getter': 'var o={get x(){return o.x}};o.x',
        'recursive-setter': 'var o={set x(v){o.x=v}};o.x=1',
        'recursive-proxy-get': 'var p=new Proxy({},{get(t,k){return p[k]}});p.x',
        'recursive-proxy-apply': 'var p=new Proxy(function(){},{apply(){return p()}});p()',
        'recursive-bind': 'function f(){return g()}var g=f.bind(null);g()',
        'recursive-map': 'function f(){return [1].map(f)}f()',
        'recursive-sort': 'function f(){return [2,1].sort(f)}f()',
        'recursive-tojson': 'var x={toJSON(){return JSON.stringify(x)}};JSON.stringify(x)',
        'recursive-valueof': 'var o={valueOf(){return +o}};+o',
        'recursive-primitive': 'var o={[Symbol.toPrimitive](){return +o}};+o',
        'recursive-tostring': 'var o={toString(){return String(o)}};String(o)',
        'recursive-proxy-keys': 'var p=new Proxy({},{ownKeys(){return Object.keys(p)}});Object.keys(p)',
        'recursive-iterator': 'var o={[Symbol.iterator](){return Array.from(o)[Symbol.iterator]()}};Array.from(o)',
        'recursive-proxy-set': 'var p=new Proxy({},{set(t,k,v){return p[k]=v}});p.x=1',
        'recursive-proxy-has': 'var p=new Proxy({},{has(t,k){return k in p}});"x" in p',
        'recursive-proxy-delete': 'var p=new Proxy({},{deleteProperty(t,k){return delete p[k]}});delete p.x',
        'recursive-proxy-descriptor': 'var p=new Proxy({},{getOwnPropertyDescriptor(t,k){return Object.getOwnPropertyDescriptor(p,k)}});Object.getOwnPropertyDescriptor(p,"x")',
        'recursive-proxy-define': 'var p=new Proxy({},{defineProperty(t,k,d){return Reflect.defineProperty(p,k,d)}});Object.defineProperty(p,"x",{value:1})',
        'recursive-proxy-getprototype': 'var p=new Proxy({},{getPrototypeOf(t){return Object.getPrototypeOf(p)}});Object.getPrototypeOf(p)',
        'recursive-proxy-setprototype': 'var p=new Proxy({},{setPrototypeOf(t,v){return Reflect.setPrototypeOf(p,v)}});Object.setPrototypeOf(p,null)',
        'recursive-proxy-extensible': 'var p=new Proxy({},{isExtensible(t){return Object.isExtensible(p)}});Object.isExtensible(p)',
        'recursive-proxy-prevent': 'var p=new Proxy({},{preventExtensions(t){return Reflect.preventExtensions(p)}});Object.preventExtensions(p)',
        'recursive-array-copy-getter': 'var a=new Array(1024);Object.defineProperty(a,500,{get(){return a.copyWithin(0,0)[0]}});a.copyWithin(0,0)',
        'recursive-array-splice-getter': 'var a=new Array(1024);Object.defineProperty(a,8,{get(){return a.splice(0,1)[0]}});a.splice(0,1)',
        'recursive-transparent-get': 'var t={},p=new Proxy(t,{});Object.setPrototypeOf(t,p);p.x',
        'recursive-transparent-has': 'var t={},p=new Proxy(t,{});Object.setPrototypeOf(t,p);"x" in p',
    }


def semantic_cases():
    from sparse_array_cases import cases as sparse_cases
    return {
        **sparse_cases(),
        'proxy-semantics': """
            var target={x:3},p=new Proxy(target,{get(t,k){return Reflect.get(t,k)+1}});
            if(p.x!==4)throw Error('proxy get');
            var r=Proxy.revocable(target,{});if(r.proxy.x!==3)throw Error('revocable');
            r.revoke();var revoked=false;try{r.proxy.x}catch(e){revoked=e instanceof TypeError}
            Object.defineProperty(target,'fixed',{value:7,writable:false,configurable:false});
            var invariant=false;try{p.fixed}catch(e){invariant=e instanceof TypeError}
            if(!revoked||!invariant||Object.getOwnPropertyDescriptor(target,'fixed').value!==7)
                throw Error('proxy invariant');
        """,
        'numeric-lookahead': Path(__file__).with_name('language_cases.js').read_text(encoding='utf-8'),
        'vm-arguments': """
            function f(a){arguments[0]=4;return a}
            function g(a){'use strict';arguments[0]=4;return a}
            function r(a,...rest){return rest.join(',')}
            if(f(1)!==4||g(1)!==1||r(0,2,3)!=='2,3')throw Error('arguments');
        """,
        'vm-constructor': """
            function T(){this.t=new.target}
            class B{constructor(v){this.v=v}m(){return this.v}}
            class D extends B{#a=2;m(){return super.m()+this.#a}}
            var d=new D(3);
            if(new T().t!==T||d.m()!==5||!(d instanceof B))throw Error('ctor');
        """,
        'vm-iterators': """
            var v=0;
            for(var x of [1,2,3]){try{if(x===3)break;v+=x}finally{v+=10}}
            function* g(){try{yield 2}finally{v+=100}}
            var it=g();if(it.next().value!==2)throw Error('yield');it.return();
            if(v!==133)throw Error('iterator');
        """,
        'vm-eval': """
            function f(){var x=3;eval('x+=4');return x}
            var o={x:2};with(o){x+=3}
            if(f()!==7||o.x!==5||Function('return 5')()!==5)throw Error('eval');
        """,
        'vm-arithmetic': """
            var x={valueOf(){return 7}},y=2n;
            if(x+3!==10||x*3!==21||y**3n!==8n||(x>>1)!==3||'3'+2!=='32'||
               !(NaN!==NaN)||!('x' in {x:1}))throw Error('arith');
        """,
        'coercion-recovery': """
            var a,b,o={valueOf(){return +o}};
            try{+o}catch(e){a=e}try{+o}catch(e){b=e}
            if(!(a instanceof InternalError)||!(b instanceof InternalError)||a===b)
                throw Error('coercion error');
            o.valueOf=function(){return 7};
            if(+o!==7)throw Error('coercion counter');
        """,
    }


def checked_recursion(source, *, proxy=False):
    """Require actual stack protection, not a missing API or another JS error.

    Keep declarations at global scope: wrapping in a function changes indirect
    eval semantics. Proxy probes additionally prove the trap was entered.
    """
    assertion = ''
    if proxy:
        source = ('var __pxa_proxy_calls=0;' +
                  source.replace('return ', '__pxa_proxy_calls++;return '))
        assertion = "if(__pxa_proxy_calls<2)throw Error('proxy trap was not entered');"
    return ("var __pxa_resource_error;try{" + source +
            "}catch(e){__pxa_resource_error=e}" +
            "if(!(__pxa_resource_error instanceof InternalError)||" +
            "String(__pxa_resource_error).indexOf('stack overflow')<0)" +
            "throw Error('wrong resource failure');" + assertion)

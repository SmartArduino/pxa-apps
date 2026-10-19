// PXA's deliberately small Web environment. Loaded only for a script page.
// Native code owns deadlines, allocation limits, origin checks and networking.
(function (native) {
    'use strict';
    const root = globalThis;
    const Function = root.Function; // Inline attributes use the original compiler.
    const own = Object.hasOwn;
    let dirty = false, building = true, nextHit = 1, hits = Object.create(null);
    let nextTimer = 1, timers = [], pending = Object.create(null);
    const escapePattern = /[&<>"\u0000]/g;
    const escapeCharacters = {'&':'&amp;', '<':'&lt;', '>':'&gt;', '"':'&quot;', '\0':'&#xfffd;'};
    const escapeCharacter = c => escapeCharacters[c];
    const escape = s => String(s).replace(escapePattern, escapeCharacter);
    function changed() { if (!building) { dirty = true; native.changed(); } }
    function event(type) {
        return {type, target:null, currentTarget:null, defaultPrevented:false,
            preventDefault() { this.defaultPrevented = true; },
            stopPropagation() { this.stopped = true; }};
    }
    function dispatch(node, e, bubble = false) {
        if (!e.target) e.target = node;
        for (let at = node, depth = 0; at && depth++ < 192; at = bubble ? at.parentNode : null) {
            e.currentTarget = at;
            let handler = at['on' + e.type];
            if (handler === undefined && at._attrs && at._attrs['on' + e.type]) {
                handler = new Function('event', at._attrs['on' + e.type]);
                at['on' + e.type] = handler;
            }
            if (typeof handler === 'function' && handler.call(at, e) === false) e.preventDefault();
            const listeners = at._listeners && at._listeners[e.type];
            if (listeners) for (const listener of listeners.slice()) listener.call(at, e);
            if (e.stopped) break;
        }
        return !e.defaultPrevented;
    }
    function listen(type, callback) {
        if (typeof callback !== 'function') return;
        if (!this._listeners) this._listeners = Object.create(null);
        const list = this._listeners[type] || (this._listeners[type] = []);
        if (list.length >= 16) throw new RangeError('event listener limit');
        if (list.indexOf(callback) < 0) list.push(callback);
    }
    function unlisten(type, callback) {
        const list = this._listeners && this._listeners[type];
        if (list) { const at = list.indexOf(callback); if (at >= 0) list.splice(at, 1); }
    }
    function Node(tag, text) {
        this.nodeType = tag ? 1 : 3;
        this.nodeName = tag ? tag.toUpperCase() : '#text';
        this.tagName = tag ? this.nodeName : undefined;
        this.parentNode = null; this.childNodes = [];
        this._text = text || ''; this._attrs = null;
        if (tag) this._style = null;
    }
    Node.prototype.appendChild = function (child) {
        if (!(child instanceof Node) || child === this) throw new TypeError('invalid child');
        for (let at = this; at; at = at.parentNode) if (at === child) throw new TypeError('cyclic DOM');
        if (this.childNodes.length >= 192) throw new RangeError('child limit');
        if (child.parentNode) child.parentNode.removeChild(child);
        child.parentNode = this; this.childNodes.push(child); changed();
        if(child.nodeName==='SCRIPT' || child._hasScripts) {
            for(let at=this;at;at=at.parentNode)at._hasScripts=true;
            activateScripts(child);
        }
        return child;
    };
    Node.prototype.removeChild = function (child) {
        const at = this.childNodes.indexOf(child);
        if (at < 0) throw new TypeError('child not found');
        this.childNodes.splice(at, 1); child.parentNode = null; changed(); return child;
    };
    Node.prototype.remove = function () { if (this.parentNode) this.parentNode.removeChild(this); };
    Node.prototype.addEventListener = listen;
    Node.prototype.removeEventListener = unlisten;
    Node.prototype.dispatchEvent = function (e) { return dispatch(this, e, true); };
    Node.prototype.getAttribute = function (name) { name = String(name).toLowerCase(); return this._attrs && own(this._attrs, name) ? this._attrs[name] : null; };
    Node.prototype.hasAttribute = function (name) { return !!this._attrs && own(this._attrs, String(name).toLowerCase()); };
    Node.prototype.setAttribute = function (name, value) {
        name = String(name).toLowerCase(); value = String(value);
        if (!/^[a-z_][a-z0-9_:.-]*$/i.test(name) || name.length > 64 || value.length > 4096)
            throw new RangeError('attribute limit');
        if (!this._attrs) this._attrs = Object.create(null);
        if (!own(this._attrs, name) && Object.keys(this._attrs).length >= 24) throw new RangeError('attribute count');
        if (this._attrs[name] === value) return;
        this._attrs[name] = value;
        if (name.startsWith('on')) this[name] = undefined;
        if (name === 'style') for (const rule of value.split(';')) {
            const colon = rule.indexOf(':'); if (colon < 0) continue;
            const key = rule.slice(0, colon).trim().replace(/-([a-z])/g, (_, c) => c.toUpperCase());
            if (this.style) this.style[key] = rule.slice(colon + 1).trim();
        }
        changed();
        if (name==='src' && this.nodeName==='SCRIPT') activateScripts(this);
    };
    function activateScripts(node) {
        let root=node;while(root.parentNode)root=root.parentNode;
        if(root!==document)return;
        const todo=[node];let visited=0;
        while(todo.length) {
            const n=todo.pop();if(++visited>512)throw new RangeError('script traversal limit');
            if(n.nodeName==='SCRIPT' && !n._scriptRequested) {
                const source=n.getAttribute('src'),type=(n.getAttribute('type')||'').toLowerCase();
                if(source && (!type || type==='text/javascript' || type==='application/javascript')) {
                    const id=native.request('SCRIPT',source,'','',0);
                    if(!id)throw new RangeError('script request limit');
                    n._scriptRequested=true;pending[id]=n;
                }
            }
            for(let i=n.childNodes.length-1;i>=0;--i)todo.push(n.childNodes[i]);
        }
    }
    Node.prototype.removeAttribute = function (name) {
        name = String(name).toLowerCase(); if (this._attrs && own(this._attrs, name)) {
            delete this._attrs[name]; if (name.startsWith('on')) this[name] = undefined; changed();
        }
    };
    for (const key of ['id','name','value','src','href','type','className','alt','title']) {
        const attribute = key === 'className' ? 'class' : key;
        Object.defineProperty(Node.prototype, key, {
            get() { const attrs=this._attrs; return attrs && own(attrs,attribute) ? attrs[attribute] || '' : ''; },
            set(v) { this.setAttribute(attribute, v); }});
    }
    for (const key of ['checked','disabled','hidden']) Object.defineProperty(Node.prototype, key, {
        get() { return this.hasAttribute(key); },
        set(v) { if (v) this.setAttribute(key, ''); else this.removeAttribute(key); }});
    const textContent = {get() {
                const todo=[this], parts=[]; let visited=0;
                while (todo.length) {
                    if (++visited>512) throw new RangeError('DOM traversal limit');
                    const at=todo.pop();
                    if (at.nodeType===3) parts.push(at._text);
                    else for(let i=at.childNodes.length-1;i>=0;--i) todo.push(at.childNodes[i]);
                }
                return parts.join('');
            },
            set(v) {
                v = String(v);
                if (v.length > 16384) throw new RangeError('text limit');
                if (this.nodeType === 3) { if (this._text !== v) { this._text = v; changed(); } return; }
                if (this.childNodes.length === 1 && this.childNodes[0].nodeType === 3) { const n=this.childNodes[0];if(n._text!==v){n._text=v;changed();}return; }
                for (const n of this.childNodes) n.parentNode = null;
                this.childNodes = [];
                if (v) this.appendChild(new Node('', v)); else changed();
            }};
    Object.defineProperties(Node.prototype, {
        style: {get() { return this._style || (this._style = Object.create(null)); }, set(v) { this._style = v; }},
        textContent, innerText: textContent,
        innerHTML: {get() { const parts=[]; for(const n of this.childNodes) parts.push(serialize(n,false)); return parts.join(''); },
            set(v) { v = String(v); if (v.indexOf('<') < 0 && v.indexOf('&') < 0) this.textContent = v;
                else { for (const n of this.childNodes) n.parentNode = null; this.childNodes = []; native.fragment(this, v); changed(); } }},
        outerHTML: {get() { return serialize(this, false); }},
        children: {get() { return this.childNodes.filter(n => n.nodeType === 1); }},
        firstChild: {get() { return this.childNodes[0] || null; }},
        lastChild: {get() { return this.childNodes[this.childNodes.length - 1] || null; }},
        clientWidth: {get() { return parseFloat(this._style && this._style.width) || native.width; }},
        clientHeight: {get() { return parseFloat(this._style && this._style.height) || 20; }},
        offsetWidth: {get() { return this.clientWidth; }}, offsetHeight: {get() { return this.clientHeight; }}
    });
    Node.prototype.getBoundingClientRect = function () {
        return {x:0,y:0,left:0,top:0,width:this.clientWidth,height:this.clientHeight,
            right:this.clientWidth,bottom:this.clientHeight};
    };
    function find(node, predicate, all) {
        const found = []; const queue = [node];
        for (let i = 0; i < queue.length && i < 512; ++i) {
            const at = queue[i];
            if (predicate(at)) { if (!all) return at; found.push(at); }
            for (const child of at.childNodes) queue.push(child);
        }
        return all ? found : null;
    }
    Node.prototype.getElementsByTagName = function (name) { name = String(name).toUpperCase(); return find(this, n => n.nodeType === 1 && (name === '*' || n.nodeName === name), true); };
    Node.prototype.querySelector = function (selector) {
        selector = String(selector);
        return find(this, n => n.nodeType === 1 && (selector[0] === '#' ? n._attrs && n._attrs.id === selector.slice(1) :
            selector[0] === '.' ? (' ' + (n._attrs && n._attrs['class'] || '') + ' ').indexOf(' ' + selector.slice(1) + ' ') >= 0 : n.nodeName === selector.toUpperCase()), false);
    };
    const document = root.document = new Node('#document'); document.nodeType = 9;
    document.readyState = 'loading'; document.documentElement = document; document.body = document; document.head = null;
    document.createElement = name => { name = String(name); if (!/^[a-z][a-z0-9-]*$/i.test(name)) throw new TypeError('invalid tag'); return new Node(name); };
    document.createTextNode = text => new Node('', String(text));
    root.Image = function (width, height) { const image = document.createElement('img'); if (width) image.setAttribute('width', width); if (height) image.setAttribute('height', height); return image; };
    document.getElementById = id => {
        id=String(id);const todo=[document];let visited=0;
        while(todo.length && visited++<512) {
            const n=todo.pop();if(n.nodeType===1 && n._attrs && n._attrs.id===id)return n;
            for(let i=n.childNodes.length-1;i>=0;--i)todo.push(n.childNodes[i]);
        }
        return null;
    };
    Object.defineProperty(document, 'title', {get() { const t = document.getElementsByTagName('title')[0]; return t ? t.textContent : ''; },
        set(v) { let t = document.getElementsByTagName('title')[0]; if (!t) { t = document.createElement('title'); (document.head || document).appendChild(t); } t.textContent = v; }});
    Object.defineProperty(document, 'cookie', {get:() => native.cookie(), set:value => native.cookie(String(value))});
    root.window = root; root.self = root; root.top = root; root.parent = root;
    root.addEventListener = listen; root.removeEventListener = unlisten;
    root.dispatchEvent = e => dispatch(root, e);
    root.navigator = {appName:'Netscape',appCodeName:'Mozilla',appVersion:'5.0',cookieEnabled:true,
        deviceMemory:0.008,doNotTrack:'1',hardwareConcurrency:1,languages:['zh-CN','en'],language:'zh-CN',
        maxTouchPoints:1,platform:'PXA',product:'Gecko',productSub:'20030107',userAgent:native.userAgent || 'PXA-Browser',vendor:''};
    root.screen = {width:native.width,height:native.height,availWidth:native.width,availHeight:native.height,colorDepth:16,pixelDepth:16};
    root.innerWidth = native.width; root.innerHeight = native.height; root.devicePixelRatio = native.density;
    root.console = {log() {},warn() {},error() {},debug() {}};
    const location = {assign:url => native.navigate(String(url)),replace:url => native.navigate(String(url)),reload:() => native.navigate(native.url),toString:() => native.url};
    Object.defineProperty(location, 'href', {get:() => native.url,set:url => native.navigate(String(url))});
    Object.defineProperty(root, 'location', {get:() => location,set:url => native.navigate(String(url))});
    document.location = location;
    function storage(session) { return {getItem:key => native.storage(0, String(key), '', session),
        setItem:(key, value) => { if (!native.storage(1, String(key), String(value), session)) throw new RangeError('storage limit'); },
        removeItem:key => native.storage(2, String(key), '', session),clear:() => native.storage(3, '', '', session)}; }
    root.localStorage = storage(false); root.sessionStorage = storage(true);
    function schedule() { native.wake(timers.length ? Math.min(...timers.map(t => t.at)) : -1); }
    root.setTimeout = function (fn, delay, ...args) {
        if (typeof fn !== 'function') throw new TypeError('function timer required');
        if (timers.length >= 8) throw new RangeError('timer limit');
        const id = nextTimer++; timers.push({id,fn,args,at:native.now() + Math.max(0, Number(delay) || 0),interval:0}); schedule(); return id;
    };
    root.clearTimeout = id => { timers = timers.filter(t => t.id !== id); schedule(); };
    root.setInterval = function (fn, delay, ...args) {
        const id = root.setTimeout(fn, delay, ...args); timers[timers.length - 1].interval = Math.max(20, Number(delay) || 20); return id;
    };
    root.clearInterval = root.clearTimeout;
    root.requestAnimationFrame = fn => root.setTimeout(() => fn(native.now()), 20);
    root.cancelAnimationFrame = root.clearTimeout;
    function XMLHttpRequest() { this.readyState = 0; this.status = 0; this.responseText = ''; this._headers = {}; }
    XMLHttpRequest.prototype.open = function (method, url, async = true) {
        if (async === false) throw new TypeError('synchronous XHR unsupported');
        this._method = String(method).toUpperCase(); this._url = String(url); this.readyState = 1;
    };
    XMLHttpRequest.prototype.setRequestHeader = function (name, value) {
        if (String(name).toLowerCase() !== 'content-type') throw new TypeError('request header unsupported');
        this._contentType = String(value);
    };
    XMLHttpRequest.prototype.addEventListener = listen;
    XMLHttpRequest.prototype.removeEventListener = unlisten;
    XMLHttpRequest.prototype.send = function (body = '') {
        const id = native.request(this._method, this._url, String(body), this._contentType || '', this.timeout || 0);
        if (!id) throw new TypeError('request blocked or limited');
        this._id = id; pending[id] = this;
    };
    XMLHttpRequest.prototype.abort = function () { if (this._id) { delete pending[this._id]; native.cancel(this._id); this._id = 0; this.readyState = 0; } };
    XMLHttpRequest.prototype.getResponseHeader = function () { return null; };
    root.XMLHttpRequest = XMLHttpRequest;
    root.fetch = function (url, options = {}) {
        return new Promise((resolve, reject) => {
            const xhr = new XMLHttpRequest(); xhr.open(options.method || 'GET', url);
            if (options.headers) for (const k of Object.keys(options.headers)) xhr.setRequestHeader(k, options.headers[k]);
            xhr.onload = () => resolve({status:xhr.status,ok:xhr.status >= 200 && xhr.status < 300,
                text:() => Promise.resolve(xhr.responseText),json:() => Promise.resolve(JSON.parse(xhr.responseText))});
            xhr.onerror = () => reject(new TypeError('network error')); xhr.send(options.body || '');
        });
    };
    function submit(form) {
        if (!dispatch(form, event('submit'), true)) return;
        const values = find(form, n => n.nodeName === 'INPUT' && n.name && !n.disabled, true)
            .filter(n => n.type !== 'submit' && (n.type !== 'checkbox' || n.checked))
            .map(n => encodeURIComponent(n.name) + '=' + encodeURIComponent(n.value)).join('&');
        native.navigate(form.getAttribute('action') || native.url, (form.getAttribute('method') || 'get').toLowerCase(), values);
    }
    Node.prototype.submit = function () { submit(this); };
    function snapshot(rootNode, render) {
        return {todo:[rootNode],endings:[''],visited:0,render,hits:render?Object.create(null):null};
    }
    function advance(state, budget) {
        // Fixed work per idle frame; no traversal consumes native recursion.
        const {todo,endings,render}=state, parts=[]; let count=0;
        while (todo.length && count++<budget) {
            const n=todo.pop(), ending=endings.pop();
            if (ending) { parts.push(ending); continue; }
            if (++state.visited>512) throw new RangeError('DOM traversal limit');
            if (n.nodeType===3) { parts.push(escape(n._text)); continue; }
            if (n.nodeType===9) {
                for(let i=n.childNodes.length-1;i>=0;--i) { todo.push(n.childNodes[i]);endings.push(''); }
                continue;
            }
            if (n.nodeName==='SCRIPT' || n.nodeName==='STYLE') continue;
            const attributes=n._attrs, styleValues=n._style;
            const type=attributes && attributes.type ? attributes.type.toLowerCase() : '';
            let tag=n.nodeName.toLowerCase(), attrs='', style='';
            if (render && (tag==='a' || tag==='button' || type==='submit' || typeof n.onclick==='function' ||
                    attributes && own(attributes,'onclick') || n._listeners && n._listeners.click)) {
                if (!n._hit) n._hit=nextHit++;
                if (Object.keys(state.hits).length>=96) throw new RangeError('interactive node limit');
                state.hits[n._hit]=n;attrs+=' data-pxa-click="'+n._hit+'"';
                if (tag!=='img' && tag!=='input' || type==='submit') tag='a';
            }
            if (render && tag==='input') {
                if (!n._hit) n._hit=nextHit++;
                state.hits[n._hit]=n;attrs+=' data-pxa-field="'+n._hit+'"';
            }
            if (attributes) for(const k of Object.keys(attributes))
                if (k!=='style' && !k.startsWith('on') && !k.startsWith('data-pxa-')) attrs+=' '+k+'="'+escape(attributes[k])+'"';
            if (styleValues) for(const key of Object.keys(styleValues)) if(styleValues[key])
                style+=key.replace(/[A-Z]/g,c=>'-'+c.toLowerCase())+':'+styleValues[key]+';';
            if(style) attrs+=' style="'+escape(style)+'"';
            parts.push('<'+tag+attrs+'>');
            if(tag==='img' || tag==='input' || tag==='br' || tag==='hr' || tag==='meta' || tag==='link') continue;
            if(tag==='a' && n.nodeName==='INPUT') { parts.push(escape(attributes.value || '提交'),'</a>');continue; }
            todo.push(null);endings.push('</'+tag+'>');
            for(let i=n.childNodes.length-1;i>=0;--i) { todo.push(n.childNodes[i]);endings.push(''); }
        }
        return {text:parts.join(''),complete:todo.length===0};
    }
    function serialize(rootNode, render) { return advance(snapshot(rootNode,render),1024).text; }
    let painting=null;
    let stack = [document], fragments = [];
    document.write = (...parts) => { native.fragment(building ? stack[stack.length-1] : document.body, parts.join('')); changed(); };
    document.writeln = (...parts) => document.write(parts.join('') + '\n');
    return {
        handlerConstructor: Function,
        handler(id, type) {
            const node=hits[id];
            return node && node['on'+type]===undefined && node._attrs && node._attrs['on'+type] ? node : null;
        },
        open(tag, attributes, empty) {
            // Parsed nodes are fresh; avoid reentering the public DOM methods.
            const node = new Node(tag), parent=stack[stack.length-1];
            for (let i = 0; i < attributes.length; i += 2) node.setAttribute(attributes[i], attributes[i + 1]);
            if(parent.childNodes.length>=192)throw new RangeError('child limit');
            node.parentNode=parent;parent.childNodes.push(node);
            if (tag === 'html') document.documentElement = node;
            if (tag === 'body') document.body = node;
            if (tag === 'head') document.head = node;
            if (!empty) stack.push(node);
        },
        text(text) { if (text) {
            const node=new Node('',text), parent=stack[stack.length-1];
            if(parent.childNodes.length>=192)throw new RangeError('child limit');
            node.parentNode=parent;parent.childNodes.push(node);
        } },
        close(tag) { for (let i = stack.length - 1; i > 0; --i) if (stack[i].nodeName.toLowerCase() === tag) { stack.length = i; break; } },
        beginFragment(node) { if (fragments.length >= 4) throw new RangeError('fragment nesting'); fragments.push(stack); stack = [node]; },
        endFragment() { stack = fragments.pop(); },
        finish() {
            building = false; document.readyState = 'interactive';
            dispatch(document, event('DOMContentLoaded')); document.readyState = 'complete';
            dispatch(root, event('load')); if (document.body) dispatch(document.body, event('load'));
            changed();
        },
        response(id, status, body, failed) {
            const xhr = pending[id]; delete pending[id]; if (!xhr) return;
            xhr._id = 0; xhr.readyState = 4; xhr.status = status; xhr.responseText = body;
            if (xhr.responseType === 'json') { try { xhr.response = JSON.parse(body); } catch (_) { xhr.response = null; } }
            else xhr.response = body;
            dispatch(xhr, event('readystatechange')); dispatch(xhr, event(failed ? 'error' : 'load')); dispatch(xhr, event('loadend'));
        },
        scriptResponse(id, failed) {
            const node=pending[id];delete pending[id];
            if(node)dispatch(node,event(failed?'error':'load'));
        },
        pump() {
            const now = native.now(), at = timers.findIndex(t => t.at <= now);
            if (at >= 0) {
                const timer = timers[at]; timers.splice(at, 1);
                if (timer.interval) { timer.at = now + timer.interval; timers.push(timer); }
                schedule(); timer.fn(...timer.args);
            }
        },
        markup() {
            if (building) return null;
            if (!painting) painting=snapshot(document,true);
            const batch=advance(painting,native.batchNodes || 8);
            if (batch.complete) { hits=painting.hits;painting=null;dirty=false; }
            return batch;
        },
        cancelMarkup() { painting=null; },
        click(id) {
            const node = hits[id]; if (!node) return;
            if (!dispatch(node, event('click'), true)) return;
            if (node.nodeName === 'A' && node.href) native.navigate(node.href);
            else if (node.nodeName === 'BUTTON' || node.type === 'submit') {
                for (let at = node.parentNode; at; at = at.parentNode) if (at.nodeName === 'FORM') { submit(at); break; }
            }
        },
        field(id, value, checked) { const node = hits[id]; if (node) { node.value = value; node.checked = checked; dispatch(node, event('input'), true); dispatch(node, event('change'), true); } }
    };
})

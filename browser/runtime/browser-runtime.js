// PXA's deliberately small Web environment. Loaded only for a script page.
// Native code owns deadlines, allocation limits, origin checks and networking.
(function (native) {
    'use strict';
    const root = globalThis;
    const Function = root.Function; // Inline attributes use the original compiler.
    const own = Object.hasOwn, attributeNamePattern=/^[a-z_][a-z0-9_:.-]*$/i;
    const fragmentStage=Symbol('fragmentStage');
    let dirty = false, building = true, nextHit = 1, hits = Object.create(null);
    let nextTimer = 1, timers = [], pending = Object.create(null);
    const escapePattern = /[&<>"\u0000]/g;
    const escapeCharacters = {'&':'&amp;', '<':'&lt;', '>':'&gt;', '"':'&quot;', '\0':'&#xfffd;'};
    const escapeCharacter = c => escapeCharacters[c];
    const escape = s => String(s).replace(escapePattern, escapeCharacter);
    function changed() { if (!building) { dirty = true; native.changed(); } }
    // Lazy, bounded inline declarations. Cache their text so a snapshot need
    // not enumerate and re-serialize every styled node on each repaint.
    const styleData = Symbol('inlineStyle'), stylePrototype = Object.create(null);
    const importantPattern = /!\s*important\s*$/i;
    function styleName(name, camel = false) {
        name = String(name);
        if (camel && !name.startsWith('--')) {
            if (name === 'cssFloat') name = 'float';
            else {
                let dashed = '';
                // Avoid replace callbacks inside retained native/Proxy frames.
                for (let i = 0; i < name.length; ++i) {
                    const c = name[i]; dashed += c >= 'A' && c <= 'Z' ? '-' + c.toLowerCase() : c;
                }
                name = dashed;
            }
        }
        return name.length <= 64 && /^(?:--[a-zA-Z0-9_-]+|-?[a-zA-Z][a-zA-Z0-9-]*)$/.test(name) ?
            (name.startsWith('--') ? name : name.toLowerCase()) : '';
    }
    function styleFragments(text, visit) {
        let prefix = '', segment = 0, quote = '', brackets = '', invalid = false;
        // Scan punctuation natively; long plain values avoid per-character VM work.
        const tokens = /[\\;(){}\[\]"'\/]/g;
        let token;
        while ((token = tokens.exec(text)) !== null) {
            const i = token.index, c = token[0];
            if (c === '\\') { ++tokens.lastIndex; continue; }
            if (quote) { if (c === quote) quote = ''; continue; }
            if (c === '/' && text[i + 1] === '*') {
                const end = text.indexOf('*/', i + 2); prefix += text.slice(segment, i) + ' ';
                if (end < 0) { segment = text.length; break; }
                segment = end + 2; tokens.lastIndex = segment; continue;
            }
            if (c === '"' || c === "'") quote = c;
            else if (c === '(' || c === '[' || c === '{') {
                if (brackets.length >= 16) throw new RangeError('style nesting limit');
                brackets += c;
            } else if (c === ')' || c === ']' || c === '}') {
                if (!brackets || '([{'.indexOf(brackets[brackets.length - 1]) !== ')]}'.indexOf(c)) invalid = true;
                else brackets = brackets.slice(0, -1);
            } else if (c === ';' && !brackets) {
                if (!invalid) visit(prefix + text.slice(segment, i), true);
                prefix = ''; segment = i + 1; invalid = false;
            }
        }
        if (!quote && !brackets && !invalid) visit(prefix + text.slice(segment), false);
    }
    function styleText(declarations) {
        const {values, priorities} = declarations;
        const keys = Object.keys(values);
        if (keys.length > 32) throw new RangeError('style property limit');
        let text = '';
        for (const name of keys) {
            text += name + ':' + values[name] + (priorities && priorities[name] ? ' !important' : '') + ';';
            if (text.length > 4096) throw new RangeError('style text limit');
        }
        return text;
    }
    function parseStyle(text) {
        if (text.length > 4096) throw new RangeError('style text limit');
        const result = {values:Object.create(null), priorities:null}, values = result.values;
        styleFragments(text, part => {
            const colon = part.indexOf(':'); if (colon < 0) return;
            const name = styleName(part.slice(0, colon).trim()); if (!name) return;
            let value = part.slice(colon + 1).trim(), priority = '';
            if (importantPattern.test(value)) { priority = 'important'; value = value.replace(importantPattern, '').trim(); }
            if (!value || result.priorities && result.priorities[name] && !priority) return;
            if (!own(values, name) && Object.keys(values).length >= 32) throw new RangeError('style property limit');
            values[name] = value;
            if (priority) (result.priorities || (result.priorities=Object.create(null)))[name] = true;
        });
        return result;
    }
    function commitStyle(data, declarations, text, raw = text) {
        const node = data.node, attrs = node._attrs;
        if (attrs && !own(attrs, 'style') && Object.keys(attrs).length >= 24) throw new RangeError('attribute count');
        if (data.text === text && attrs && attrs.style === raw) return;
        if (!attrs) node._attrs = Object.create(null);
        data.values = declarations.values; data.priorities = declarations.priorities;
        data.text = text; node._attrs.style = raw; changed();
    }
    function replaceStyle(node, value, raw = false) {
        const text = value == null ? '' : String(value), values = parseStyle(text), serialized = styleText(values);
        commitStyle(styleState(node), values, serialized, raw ? text : serialized);
    }
    function removeStyle(data, name) {
        const old = data.values[name]; if (!old) return '';
        const next = {values:Object.assign(Object.create(null), data.values), priorities:data.priorities};
        delete next.values[name];
        if (next.priorities && next.priorities[name]) {
            next.priorities = Object.assign(Object.create(null), next.priorities); delete next.priorities[name];
        }
        commitStyle(data, next, styleText(next)); return old;
    }
    function setStyle(data, name, value, priority = '') {
        if (!name) return;
        value = value == null ? '' : String(value);
        if (!value) { removeStyle(data, name); return; }
        priority = priority == null ? '' : String(priority).toLowerCase();
        if (priority && priority !== 'important') return;
        if (value.length > 4096) throw new RangeError('style text limit');
        let parsed = '', invalid = false;
        styleFragments(value, (part, separator) => { if (separator) invalid = true; else parsed = part.trim(); });
        if (invalid || !parsed || importantPattern.test(parsed)) return;
        const wasImportant = !!(data.priorities && data.priorities[name]);
        if (data.values[name] === parsed && wasImportant === !!priority) return;
        const next = {values:Object.assign(Object.create(null), data.values), priorities:data.priorities};
        next.values[name] = parsed;
        if (wasImportant !== !!priority) {
            next.priorities = Object.assign(Object.create(null), data.priorities);
            if (priority) next.priorities[name] = true; else delete next.priorities[name];
        }
        commitStyle(data, next, styleText(next));
    }
    Object.defineProperties(stylePrototype, {
        cssText: {get() { return this[styleData].text; }, set(v) { replaceStyle(this[styleData].node, v); }},
        length: {get() { return Object.keys(this[styleData].values).length; }},
        item: {value(index) { return Object.keys(this[styleData].values)[Number(index) >>> 0] || ''; }},
        getPropertyValue: {value(name) { return this[styleData].values[styleName(name)] || ''; }},
        getPropertyPriority: {value(name) { const p = this[styleData].priorities; return p && p[styleName(name)] ? 'important' : ''; }},
        setProperty: {value(name, value, priority) { setStyle(this[styleData], styleName(name), value, priority); }},
        removeProperty: {value(name) { return removeStyle(this[styleData], styleName(name)); }},
        toString: {value() { return this[styleData].text; }}
    });
    const styleHandler = {
        get(target, key, receiver) {
            if (key === 'cssText') return target[styleData].text;
            if (key === 'length') return Object.keys(target[styleData].values).length;
            if (typeof key !== 'string' || key in target) return Reflect.get(target, key, receiver);
            const values = target[styleData].values;
            if (/^(0|[1-9][0-9]*)$/.test(key)) return Object.keys(values)[key];
            return values[styleName(key, true)] || '';
        },
        set(target, key, value, receiver) {
            if (key === 'cssText') { replaceStyle(target[styleData].node, value); return true; }
            if (typeof key !== 'string' || key in target) return Reflect.set(target, key, value, receiver);
            setStyle(target[styleData], styleName(key, true), value); return true;
        },
        deleteProperty(target, key) {
            if (typeof key === 'string') removeStyle(target[styleData], styleName(key, true));
            return true;
        },
        ownKeys(target) { return Object.keys(target[styleData].values); },
        getOwnPropertyDescriptor(target, key) {
            const entry = target[styleData].values[styleName(key)];
            return entry ? {value:entry, writable:true, enumerable:true, configurable:true} : undefined;
        }
    };
    function styleState(node) {
        return node._style || (node._style = {node, values:Object.create(null), priorities:null, text:'', proxy:null});
    }
    function inlineStyle(node) {
        const data = styleState(node);
        if (!data.proxy) {
            const target = Object.create(stylePrototype); target[styleData] = data;
            data.proxy = new Proxy(target, styleHandler);
        }
        return data.proxy;
    }
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
    function clickTarget(node) {
        if (disabledControl(node)) return false;
        const attrs = node._attrs, listeners = node._listeners && node._listeners.click;
        return !!(node.nodeName === 'A' || node.nodeName === 'BUTTON' ||
            attrs && attrs.type && attrs.type.toLowerCase() === 'submit' ||
            typeof node._onclick === 'function' ||
            attrs && own(attrs, 'onclick') && node._onclick === undefined || listeners && listeners.length);
    }
    function disabledControl(node) {
        return !!node && (node.nodeName === 'INPUT' || node.nodeName === 'BUTTON') && node.disabled;
    }
    function submitControl(node) {
        const type = node.type.toLowerCase();
        return node.nodeName === 'INPUT' ? type === 'submit' :
            node.nodeName === 'BUTTON' && type !== 'button' && type !== 'reset';
    }
    function listen(type, callback) {
        if (typeof callback !== 'function') return;
        type = String(type);
        const node = type === 'click' && this instanceof Node, before = node && clickTarget(this);
        if (!this._listeners) this._listeners = Object.create(null);
        const list = this._listeners[type] || (this._listeners[type] = []);
        if (list.indexOf(callback) >= 0) return;
        if (list.length >= 16) throw new RangeError('event listener limit');
        list.push(callback);
        if (node && before !== clickTarget(this)) changed();
    }
    function unlisten(type, callback) {
        type = String(type);
        const list = this._listeners && this._listeners[type];
        if (!list) return;
        const at = list.indexOf(callback); if (at < 0) return;
        const node = type === 'click' && this instanceof Node, before = node && clickTarget(this);
        list.splice(at, 1);
        if (!list.length) delete this._listeners[type];
        if (node && before !== clickTarget(this)) changed();
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
        if (child.nodeName==='INPUT' && child.checked) uncheckRadioPeers(child);
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
    Object.defineProperty(Node.prototype, 'onclick', {
        get() { return this._onclick; },
        set(value) {
            const before = clickTarget(this); this._onclick = value == null ? null : value;
            if (before !== clickTarget(this)) changed();
        }
    });
    Node.prototype.getAttribute = function (name) { name = String(name).toLowerCase(); return this._attrs && own(this._attrs, name) ? this._attrs[name] : null; };
    Node.prototype.hasAttribute = function (name) { return !!this._attrs && own(this._attrs, String(name).toLowerCase()); };
    Node.prototype.setAttribute = function (name, value) {
        name = String(name).toLowerCase(); value = String(value);
        if (!attributeNamePattern.test(name) || name.length > 64 || value.length > 4096)
            throw new RangeError('attribute limit');
        if (name === 'style') { replaceStyle(this, value, true); return; }
        if (!this._attrs) this._attrs = Object.create(null);
        if (!own(this._attrs, name) && Object.keys(this._attrs).length >= 24) throw new RangeError('attribute count');
        if (this._attrs[name] === value) {
            // Setting an inline handler again reactivates it even after a
            // property assignment disabled/replaced the original handler.
            if (name === 'onclick') {
                const before = clickTarget(this); this._onclick = undefined;
                if (before !== clickTarget(this)) changed();
            } else if (name.startsWith('on')) this[name] = undefined;
            return;
        }
        this._attrs[name] = value;
        if(name==='class' && this[tokenData])classTokens(this[tokenData][tokenData],value);
        if (name==='checked') uncheckRadioPeers(this);
        if (name === 'onclick') this._onclick = undefined;
        else if (name.startsWith('on')) this[name] = undefined;
        changed();
        if (name==='src' && this.nodeName==='SCRIPT') activateScripts(this);
    };
    function formOwner(n) {
        for(let p=n.parentNode;p;p=p.parentNode) if(p.nodeName==='FORM') return p;
        return null;
    }
    function uncheckRadioPeers(node) {
        if (node.nodeName!=='INPUT' || node.type.toLowerCase()!=='radio' || !node.name) return;
        const form=formOwner(node);
        let top=node;while(top.parentNode)top=top.parentNode;
        const todo=[top];let visited=0;
        while(todo.length) {
            if(++visited>512)throw new RangeError('radio traversal limit');
            const other=todo.pop();
            if(other!==node && other.nodeName==='INPUT' && other.type.toLowerCase()==='radio' &&
                other.name===node.name && formOwner(other)===form && other.checked)
                other.removeAttribute('checked');
            for(let i=other.childNodes.length-1;i>=0;--i)todo.push(other.childNodes[i]);
        }
    }
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
            if (name === 'style' && this._style) {
                const data = this._style; data.values = Object.create(null); data.priorities = null; data.text = '';
            }
            delete this._attrs[name];
            if(name==='class' && this[tokenData])classTokens(this[tokenData][tokenData],'');
            if (name === 'onclick') this._onclick = undefined;
            else if (name.startsWith('on')) this[name] = undefined;
            changed();
        }
    };
    for (const key of ['id','name','value','src','srcset','sizes','media','loading','href','type','className','alt','title','action']) {
        const attribute = key === 'className' ? 'class' : key;
        Object.defineProperty(Node.prototype, key, {
            get() { const attrs=this._attrs;
                if(attrs && own(attrs,attribute))return attrs[attribute] || '';
                return key==='value' && this.nodeName==='INPUT' &&
                    (this.type.toLowerCase()==='radio' || this.type.toLowerCase()==='checkbox') ? 'on' : ''; },
            set(v) {
                if(key==='className') {
                    // Reflection writes directly, without a public DOM call
                    // retained around conversion hooks and token refresh.
                    const text=domString(v),attrs=this._attrs;if(text.length>4096)throw new RangeError('attribute limit');
                    if(attrs && attrs.class===text)return;
                    if(attrs && !own(attrs,'class') && Object.keys(attrs).length>=24)throw new RangeError('attribute count');
                    if(!attrs)this._attrs=Object.create(null);this._attrs.class=text;
                    if(this[tokenData])classTokens(this[tokenData][tokenData],text);changed();
                }else this.setAttribute(attribute,v);
            }});
    }
    for (const key of ['method','enctype']) Object.defineProperty(Node.prototype, key, {
        get() {
            const value = (this.getAttribute(key) || '').toLowerCase();
            return key === 'method' ? (value === 'post' || value === 'dialog' ? value : 'get') :
                (value === 'multipart/form-data' || value === 'text/plain' ? value : 'application/x-www-form-urlencoded');
        },
        set(value) { this.setAttribute(key, value); }
    });
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
                if (!v && this.childNodes.length === 0) return;
                if (this.childNodes.length === 1 && this.childNodes[0].nodeType === 3) { const n=this.childNodes[0];if(n._text!==v){n._text=v;changed();}return; }
                for (const n of this.childNodes) n.parentNode = null;
                this.childNodes = [];
                if (v) this.appendChild(new Node('', v)); else changed();
            }};
    Object.defineProperties(Node.prototype, {
        style: {get() { return inlineStyle(this); }, set(v) { replaceStyle(this, v); }},
        textContent, innerText: textContent,
        innerHTML: {get() { const parts=[]; for(const n of this.childNodes) parts.push(serialize(n,false)); return parts.join(''); },
            set(v) { v = String(v); if (v.indexOf('<') < 0 && v.indexOf('&') < 0 && v.indexOf('\r') < 0 && v.indexOf('\0') < 0) this.textContent = v;
                else {
                    // Keep old nodes, handlers and parent links until every
                    // fragment check succeeds. Only the text context is needed.
                    const staged={nodeName:this.nodeName,childNodes:[]},stage={target:this,radios:null};
                    staged[fragmentStage]=stage;native.fragment(staged,v,true);
                    for(const n of this.childNodes)n.parentNode=null;
                    this.childNodes=staged.childNodes;
                    for(const n of this.childNodes)n.parentNode=this;
                    changed();
                    const radios=stage.radios;
                    for(let i=radios?radios.length-1:-1;i>=0;--i)if(radios[i].checked)uncheckRadioPeers(radios[i]);
                } }},
        outerHTML: {get() { return serialize(this, false); }},
        children: {get() { return this.childNodes.filter(n => n.nodeType === 1); }},
        firstChild: {get() { return this.childNodes[0] || null; }},
        lastChild: {get() { return this.childNodes[this.childNodes.length - 1] || null; }},
        clientWidth: {get() { return parseFloat(this._style && this._style.values.width) || native.width; }},
        clientHeight: {get() { return parseFloat(this._style && this._style.values.height) || 20; }},
        offsetWidth: {get() { return this.clientWidth; }}, offsetHeight: {get() { return this.clientHeight; }}
    });
    Node.prototype.getBoundingClientRect = function () {
        return {x:0,y:0,left:0,top:0,width:this.clientWidth,height:this.clientHeight,
            right:this.clientWidth,bottom:this.clientHeight};
    };
    function collectionItem(index) { return this[index >>> 0] || null; }
    function find(node, predicate, all) {
        const found = all ? [] : null, todo = []; let visited = 0;
        for (let i=node.childNodes.length-1;i>=0;--i) todo.push(node.childNodes[i]);
        while (todo.length) {
            if (++visited > 512) throw new RangeError('DOM query traversal limit');
            const at = todo.pop();
            if (predicate(at)) { if (!all) return at; found.push(at); }
            for (let i=at.childNodes.length-1;i>=0;--i) todo.push(at.childNodes[i]);
        }
        if(all)Object.defineProperty(found,'item',{value:collectionItem});
        return all ? found : null;
    }
    Node.prototype.getElementsByTagName = function (name) { name = String(name).toUpperCase(); return find(this, n => n.nodeType === 1 && (name === '*' || n.nodeName === name), true); };
    const cssSpace = /[\t\n\f\r ]/, cssTrim = /^[\t\n\f\r ]+|[\t\n\f\r ]+$/g;
    const cssIdent = /^(?:--|-[A-Za-z_\u0080-\uffff]|[A-Za-z_\u0080-\uffff])[A-Za-z0-9_\u0080-\uffff-]*/;
    function classToken(value, token) {
        if(!value || !token || cssSpace.test(token))return false;
        let at=-1;
        while((at=value.indexOf(token,at+1))>=0) {
            const end=at+token.length;
            if((at===0 || cssSpace.test(value[at-1])) && (end===value.length || cssSpace.test(value[end])))return true;
        }
        return false;
    }
    Node.prototype.getElementsByClassName = function (names) {
        names=String(names);if(names.length>512)throw new RangeError('class query length limit');
        const tokens = names.replace(cssTrim,'').split(/[\t\n\f\r ]+/);
        if(tokens.length>64)throw new RangeError('class query token limit');
        if(!tokens[0])return Object.defineProperty([],'item',{value:collectionItem});
        return find(this, n => n.nodeType === 1 && tokens.every(t => classToken(n.className,t)), true);
    };
    let lastSelector = null;
    // Lazily retain token lists only for nodes which actually use classList.
    // Cached tokens follow raw attribute writes without touching every setter.
    const tokenData=Symbol('classListData');
    const exceptionNames=['IndexSizeError','','HierarchyRequestError','WrongDocumentError','InvalidCharacterError','',
        'NoModificationAllowedError','NotFoundError','NotSupportedError','InUseAttributeError','InvalidStateError','SyntaxError',
        'InvalidModificationError','NamespaceError','InvalidAccessError','','TypeMismatchError','SecurityError','NetworkError',
        'AbortError','URLMismatchError','QuotaExceededError','TimeoutError','InvalidNodeTypeError','DataCloneError'];
    function domString(value) {if(typeof value==='symbol')throw new TypeError('DOMString expected');return String(value);}
    class DOMException extends Error {
        #message;#name;
        constructor(message='',name='Error') {super();this.#message=domString(message);this.#name=domString(name);}
        get message() {return this.#message;}
        get name() {return this.#name;}
        get code() {return this.#name?exceptionNames.indexOf(this.#name)+1:0;}
    }
    const exceptionConstants='INDEX_SIZE DOMSTRING_SIZE HIERARCHY_REQUEST WRONG_DOCUMENT INVALID_CHARACTER NO_DATA_ALLOWED NO_MODIFICATION_ALLOWED NOT_FOUND NOT_SUPPORTED INUSE_ATTRIBUTE INVALID_STATE SYNTAX INVALID_MODIFICATION NAMESPACE INVALID_ACCESS VALIDATION TYPE_MISMATCH SECURITY NETWORK ABORT URL_MISMATCH QUOTA_EXCEEDED TIMEOUT INVALID_NODE_TYPE DATA_CLONE'.split(' ');
    for(let i=0;i<exceptionConstants.length;++i) {
        const descriptor={value:i+1,enumerable:true};
        Object.defineProperty(DOMException,exceptionConstants[i]+'_ERR',descriptor);
        Object.defineProperty(DOMException.prototype,exceptionConstants[i]+'_ERR',descriptor);
    }
    for(const name of ['name','message','code'])Object.defineProperty(DOMException.prototype,name,{enumerable:true});
    Object.defineProperty(DOMException.prototype,Symbol.toStringTag,{value:'DOMException',configurable:true});
    root.DOMException=DOMException;
    class DOMTokenList {constructor() {throw new TypeError('Illegal constructor');}}
    function tokenState(list) {const state=list && list[tokenData];if(!state || state.list!==list)throw new TypeError('DOMTokenList receiver');return state;}
    function classValue(state) {const attrs=state.node._attrs;return attrs && own(attrs,'class')?attrs.class:'';}
    function classTokens(state,raw=classValue(state)) {
        if(raw!==state.raw) {
            const seen=Object.create(null),tokens=[];
            const pattern=/[^\t\n\f\r ]+/g;let match;
            while((match=pattern.exec(raw))!==null) {const token=match[0];if(!own(seen,token)){seen[token]=true;tokens.push(token);}}
            const target=state.target,previous=state.tokens?state.tokens.length:0;
            for(let i=0;i<tokens.length;++i)Object.defineProperty(target,String(i),{value:tokens[i],writable:false,enumerable:true,configurable:true});
            for(let i=tokens.length;i<previous;++i)delete target[i];
            state.raw=raw;state.tokens=tokens;
        }
        return state.tokens;
    }
    function writeClass(state,text) {
        if(text.length>4096)throw new RangeError('attribute limit');
        const node=state.node,attrs=node._attrs;
        if(attrs && attrs.class===text)return;
        if(attrs && !own(attrs,'class') && Object.keys(attrs).length>=24)throw new RangeError('attribute count');
        if(!attrs)node._attrs=Object.create(null);
        node._attrs.class=text;classTokens(state,text);changed();
    }
    function commitTokens(state,tokens) {
        const attrs=state.node._attrs;
        if(!tokens.length && (!attrs || !own(attrs,'class')))return;
        const raw=tokens.join(' ');writeClass(state,raw);
    }
    function validateToken(token) {
        if(!token)throw new DOMException('Empty class token','SyntaxError');
        if(cssSpace.test(token))throw new DOMException('ASCII whitespace in class token','InvalidCharacterError');
    }
    function tokenIterator(list,kind) {
        const state=tokenState(list);let index=0,done=false;
        return {next() {
            const tokens=classTokens(state);if(done || index>=tokens.length){done=true;return {value:undefined,done:true};}
            const at=index++;return {value:kind===0?at:kind===1?tokens[at]:[at,tokens[at]],done:false};
        },[Symbol.iterator]() {return this;}};
    }
    Object.assign(DOMTokenList.prototype,{
        item(index) {const state=tokenState(this);if(!arguments.length)throw new TypeError('index required');index=index>>>0;return classTokens(state)[index]??null;},
        contains(token) {const state=tokenState(this);if(!arguments.length)throw new TypeError('token required');token=domString(token);return classTokens(state).includes(token);},
        add(...tokens) {
            const state=tokenState(this);for(let i=0;i<tokens.length;++i)tokens[i]=domString(tokens[i]);
            for(const token of tokens)validateToken(token);
            const next=classTokens(state).slice();for(const token of tokens)if(!next.includes(token))next.push(token);
            commitTokens(state,next);
        },
        remove(...tokens) {
            const state=tokenState(this);for(let i=0;i<tokens.length;++i)tokens[i]=domString(tokens[i]);
            for(const token of tokens)validateToken(token);
            const next=classTokens(state).filter(token=>!tokens.includes(token));commitTokens(state,next);
        },
        toggle(token,force) {
            const state=tokenState(this);if(!arguments.length)throw new TypeError('token required');token=domString(token);validateToken(token);
            const tokens=classTokens(state),at=tokens.indexOf(token),present=at>=0;
            const wanted=force!==undefined?!!force:!present;if(wanted===present)return present;
            const next=tokens.slice();if(wanted)next.push(token);else next.splice(at,1);commitTokens(state,next);return wanted;
        },
        replace(token,newToken) {
            const state=tokenState(this);if(arguments.length<2)throw new TypeError('two tokens required');
            token=domString(token);newToken=domString(newToken);
            if(!token || !newToken)throw new DOMException('Empty class token','SyntaxError');
            validateToken(token);validateToken(newToken);
            const tokens=classTokens(state),at=tokens.indexOf(token);if(at<0)return false;
            const next=tokens.slice(),other=next.indexOf(newToken);
            if(other<0 || other===at)next[at]=newToken;else {next[Math.min(at,other)]=newToken;next.splice(Math.max(at,other),1);}
            commitTokens(state,next);return true;
        },
        supports(token) {tokenState(this);if(!arguments.length)throw new TypeError('token required');domString(token);throw new TypeError('class has no supported-token vocabulary');},
        forEach(callback,thisArg) {
            const state=tokenState(this);if(typeof callback!=='function')throw new TypeError('callback required');
            for(let i=0;i<classTokens(state).length;++i)callback.call(thisArg,classTokens(state)[i],i,this);
        },
        keys() {return tokenIterator(this,0);},values() {return tokenIterator(this,1);},entries() {return tokenIterator(this,2);},
        toString() {return classValue(tokenState(this));}
    });
    Object.defineProperties(DOMTokenList.prototype,{
        length:{get() {return classTokens(tokenState(this)).length;},enumerable:true,configurable:true},
        value:{get() {return classValue(tokenState(this));},set(value) {const state=tokenState(this);writeClass(state,domString(value));},enumerable:true,configurable:true},
        [Symbol.iterator]:{value:DOMTokenList.prototype.values,writable:true,configurable:true},
        [Symbol.toStringTag]:{value:'DOMTokenList',configurable:true}
    });
    const tokenIndex=key=>typeof key==='string' && /^(0|[1-9][0-9]*)$/.test(key) && Number(key)<4294967295;
    // Ordinary reads avoid Proxy callbacks and their retained Wasm frames.
    // Only mutations need interception; indices are refreshed on class writes.
    const tokenHandler={
        set(target,key,value,receiver) {return !tokenIndex(key) && Reflect.set(target,key,value,receiver);},
        defineProperty(target,key,descriptor) {return !tokenIndex(key) && Reflect.defineProperty(target,key,descriptor);},
        deleteProperty(target,key) {return !(tokenIndex(key) && own(target,key)) && Reflect.deleteProperty(target,key);},
        preventExtensions() {return false;}
    };
    Object.defineProperty(Node.prototype,'classList',{
        get() {
            if(this.nodeType!==1)return undefined;
            let list=this[tokenData];if(!list) {
                const target=Object.create(DOMTokenList.prototype),state={node:this,target,raw:null,tokens:null};list=new Proxy(target,tokenHandler);
                state.list=list;Object.defineProperty(target,tokenData,{value:state});Object.defineProperty(this,tokenData,{value:list});classTokens(state);
            }
            return list;
        },
        set(value) {const list=this.classList;if(list)list.value=value;}
    });
    root.DOMTokenList=DOMTokenList;

    function selectorPlan(source) {
        source = String(source);
        if (source.length > 512) throw new RangeError('selector length limit');
        if (lastSelector && lastSelector.source === source) return lastSelector;
        let i=0, terms=0, steps=0; const groups=[];
        function invalid() { throw new DOMException('Unsupported or invalid selector','SyntaxError'); }
        function space() { const start=i;while(i<source.length && cssSpace.test(source[i]))++i;return i!==start; }
        function ident() { const m=cssIdent.exec(source.slice(i));if(!m)invalid();i+=m[0].length;return m[0]; }
        space();
        while (i<source.length) {
            const group=[];let relation='';
            while (true) {
                if (++steps>32) throw new RangeError('selector step limit');
                const part={tag:'',tests:[],relation};
                if (source[i]==='*')++i;
                else if (cssIdent.test(source.slice(i)))part.tag=ident().toUpperCase();
                while(i<source.length) {
                    const c=source[i];let test;
                    if(c==='#' || c==='.') {++i;test=[c,ident()];}
                    else if(c===':') {++i;if(ident()!=='scope')invalid();test=['scope'];}
                    else if(c==='[') {
                        ++i;space();const name=ident().toLowerCase();space();
                        let op='',value='',flag='';
                        if(source[i]!==']') {
                            if(source[i]==='=')op=source[i++];
                            else if('~|^$*'.includes(source[i]) && source[i+1]==='=') {op=source.slice(i,i+2);i+=2;}
                            else invalid();
                            space();const quote=source[i];
                            if(quote==='"' || quote==="'") {
                                ++i;const start=i;
                                while(i<source.length && source[i]!==quote) {
                                    if(source[i]==='\\' || source[i]==='\n' || source[i]==='\r' || source[i]==='\f')invalid();++i;
                                }
                                if(i===source.length)invalid();value=source.slice(start,i++);
                            } else value=ident();
                            space();if(source[i]!==']') {flag=ident().toLowerCase();if(flag!=='i' && flag!=='s')invalid();space();}
                        }
                        if(source[i++]!==']')invalid();test=['attr',name,op,value,flag];
                    } else break;
                    if(++terms>64)throw new RangeError('selector term limit');part.tests.push(test);
                }
                // A universal selector has neither a tag nor tests.
                if(!part.tag && !part.tests.length && source[i-1]!=='*')invalid();
                group.push(part);const separated=space();
                if(i===source.length || source[i]===',')break;
                if(source[i]==='>') {++i;space();relation='>';}
                else if(separated)relation=' ';
                else invalid();
            }
            if(groups.length===8)throw new RangeError('selector group limit');groups.push(group);
            if(i===source.length)break;
            ++i;space();if(i===source.length)invalid();
        }
        if(!groups.length)invalid();
        return lastSelector={source,groups};
    }
    const asciiUpper=/[A-Z]/,asciiOnly=/^[\x00-\x7f]*$/;
    function foldAsciiMixed(value) {
        let result='',start=0;
        for(let i=0;i<value.length;++i) {
            const c=value.charCodeAt(i);
            if(c>=65 && c<=90) {result+=value.slice(start,i)+String.fromCharCode(c+32);start=i+1;}
        }
        return result+value.slice(start);
    }
    function selectorAttribute(node, test) {
        const attrs=node._attrs,name=test[1],op=test[2];
        if(!attrs || !own(attrs,name))return false;
        if(!op)return true;
        let value=attrs[name],needle=test[3];
        const flag=test[4],htmlEnum=(name==='type' && (node.nodeName==='INPUT' || node.nodeName==='BUTTON')) ||
            (node.nodeName==='FORM' && (name==='method' || name==='enctype')) || name==='dir';
        if(flag==='i' || (!flag && htmlEnum)) {
            // No JS replacement callback inside nested event/query frames.
            if(asciiUpper.test(value))value=asciiOnly.test(value)?value.toLowerCase():foldAsciiMixed(value);
            if(asciiUpper.test(needle))needle=asciiOnly.test(needle)?needle.toLowerCase():foldAsciiMixed(needle);
        }
        if(op==='=')return value===needle;
        if(op==='|=')return value===needle || value.startsWith(needle+'-');
        if(!needle)return false;
        return op==='~=' ? classToken(value,needle) : op==='^=' ? value.startsWith(needle) :
            op==='$=' ? value.endsWith(needle) : value.includes(needle);
    }
    function selectorPart(node, part, scope, work) {
        if(--work.left<0)throw new RangeError('selector work limit');
        if(node.nodeType!==1 || part.tag && node.nodeName!==part.tag)return false;
        for(const test of part.tests) {
            if(test[0]==='#' ? node.id!==test[1] : test[0]==='.' ? !classToken(node.className,test[1]) :
               test[0]==='scope' ? node!==scope : !selectorAttribute(node,test))return false;
        }
        return true;
    }
    function selectorMatch(node, plan, scope, work) {
        for(const group of plan.groups) {
            const last=group.length-1;
            if(last===0) {if(selectorPart(node,group[0],scope,work))return true;continue;}
            const selected=[];let part=last,at=node;
            // Iterative backtracking keeps JS call depth independent of the
            // selector/ancestor depth. A later ancestor can satisfy a child
            // relation after a nearer descendant candidate fails it.
            while(true) {
                while(at && !selectorPart(at,group[part],scope,work)) {
                    if(part===last || group[part+1].relation==='>') {at=null;break;}
                    at=at.parentNode;
                }
                if(at) {if(part===0)return true;selected[part]=at;--part;at=at.parentNode;continue;}
                let retry=part+1;
                while(retry<last && group[retry+1].relation==='>')++retry;
                if(retry>=last)break;
                part=retry;at=selected[retry].parentNode;
            }
        }
        return false;
    }
    function query(node, selector, all) {
        const plan=selectorPlan(selector),scope=node.nodeType===9 ? node.documentElement : node,work={left:8192};
        return find(node,n=>n.nodeType===1 && selectorMatch(n,plan,scope,work),all);
    }
    Node.prototype.querySelector = function (selector) { return query(this,selector,false); };
    Node.prototype.querySelectorAll = function (selector) { return query(this,selector,true); };
    Node.prototype.matches = function (selector) { return selectorMatch(this,selectorPlan(selector),this,{left:8192}); };
    Node.prototype.closest = function (selector) {
        const plan=selectorPlan(selector),work={left:8192};let visited=0;
        for(let n=this;n;n=n.parentNode) {
            if(++visited>512)throw new RangeError('closest traversal limit');
            if(selectorMatch(n,plan,this,work))return n;
        }
        return null;
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
    Object.defineProperty(document, 'title', {get() { const t = document.getElementsByTagName('title')[0]; return t ? t.textContent.replace(/[\t\n\f\r ]+/g,' ').replace(cssTrim,'') : ''; },
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
    const simpleFormText = /^[A-Za-z0-9*._-]*$/;
    function formEncode(text) {
        if (simpleFormText.test(text)) return text;
        // Native string replacement avoids a retained JS callback frame for
        // every punctuation mark, including during an onsubmit -> submit().
        return encodeURIComponent(text).replaceAll('%20','+').replaceAll('!','%21')
            .replaceAll("'",'%27').replaceAll('(','%28').replaceAll(')','%29').replaceAll('~','%7E');
    }
    function submit(form, submitter = null, fireEvent = true) {
        if (form.nodeName !== 'FORM') throw new TypeError('form required');
        if (fireEvent && form._submitting) return;
        if (fireEvent) form._submitting = true;
        try {
            if (fireEvent) {
                const e = event('submit'); e.submitter = submitter;
                if (!dispatch(form, e, true)) return;
            }
            const method = (form.method || 'get').toLowerCase(), encoding = form.enctype.toLowerCase();
            if (method === 'dialog') return;
            if (method === 'post' && (encoding === 'multipart/form-data' || encoding === 'text/plain'))
                throw new TypeError('form encoding unsupported');
            // One bounded traversal, in tree order. Nested forms keep their
            // own controls; no intermediate find/filter/map node arrays.
            const todo = [form], parts = []; let visited = 0, bytes = 0;
            while (todo.length) {
                const n = todo.pop();
                if (++visited > 512) throw new RangeError('form traversal limit');
                if (n !== form && n.nodeName === 'FORM') continue;
                if ((n.nodeName === 'INPUT' || n.nodeName === 'BUTTON') && n.name && !n.disabled) {
                    const type = n.type.toLowerCase();
                    const button = n.nodeName === 'BUTTON' || type === 'submit' || type === 'button' || type === 'reset' || type === 'image';
                    const include = button ? n === submitter : ((type !== 'checkbox' && type !== 'radio') || n.checked);
                    if (include) {
                        const pair = formEncode(n.name) + '=' + formEncode(n.value);
                        bytes += pair.length + (parts.length ? 1 : 0);
                        if (bytes > 8192) throw new RangeError('form body limit');
                        parts.push(pair);
                    }
                }
                for (let i = n.childNodes.length - 1; i >= 0; --i) todo.push(n.childNodes[i]);
            }
            native.navigate(form.action || native.url, method === 'post' ? 'post' : 'get', parts.join('&'));
        } finally { if (fireEvent) form._submitting = false; }
    }
    Node.prototype.submit = function () { submit(this, null, false); };
    Node.prototype.requestSubmit = function (submitter = null) {
        if (submitter !== null && (!(submitter instanceof Node) || !submitControl(submitter)))
            throw new TypeError('submit button required');
        if (submitter && formOwner(submitter) !== this) throw new TypeError('submit button belongs to another form');
        submit(this, submitter);
    };
    function snapshot(rootNode, render) {
        return {todo:[rootNode],endings:[''],visited:0,render,hits:render?Object.create(null):null};
    }
    function rawTextNode(node) {
        return node && (node.nodeName==='SCRIPT' || node.nodeName==='STYLE' || node.nodeName==='XMP' ||
            node.nodeName==='IFRAME' || node.nodeName==='NOEMBED' || node.nodeName==='NOFRAMES' || node.nodeName==='PLAINTEXT');
    }
    function advance(state, budget) {
        // Fixed work per idle frame; no traversal consumes native recursion.
        const {todo,endings,render}=state, parts=[]; let count=0;
        while (todo.length && count++<budget) {
            const n=todo.pop(), ending=endings.pop();
            if (ending) { parts.push(ending); continue; }
            if (++state.visited>512) throw new RangeError('DOM traversal limit');
            if (n.nodeType===3) { parts.push(!render && rawTextNode(n.parentNode)?n._text:escape(n._text)); continue; }
            if (n.nodeType===9) {
                for(let i=n.childNodes.length-1;i>=0;--i) { todo.push(n.childNodes[i]);endings.push(''); }
                continue;
            }
            if (render && n.nodeName==='SCRIPT') continue;
            // Keep preceding page styles across an incremental DOM repaint.
            // CSS raw text must not be HTML-escaped; the native parser applies
            // its own 8 KiB / 64-rule budget without fetching external sheets.
            if (n.nodeName==='STYLE') {
                const media=n.getAttribute('media'), type=n.getAttribute('type');
                const css=n.textContent.slice(0,8192);
                parts.push('<style'+(media?' media="'+escape(media)+'"':'')+
                    (type?' type="'+escape(type)+'"':'')+'>'+(render?css.replace(/<\/style/gi,'\\3c /style'):css)+'</style>');
                continue;
            }
            const attributes=n._attrs;
            const type=attributes && attributes.type ? attributes.type.toLowerCase() : '';
            let tag=n.nodeName.toLowerCase(), attrs='', style='';
            // A DOM text string may itself contain a literal closing tag.
            // Project visible raw blocks as escaped preformatted text so an
            // incremental repaint cannot turn that string into active markup.
            if(render && (tag==='xmp' || tag==='plaintext'))tag='pre';
            const disabled=(tag==='input' || tag==='button') && attributes && own(attributes,'disabled');
            const inert=tag==='title' || tag==='iframe' || tag==='noembed' || tag==='noframes';
            if (render && !inert && !disabled && (tag==='a' || tag==='button' || type==='submit' || typeof n._onclick==='function' ||
                    attributes && own(attributes,'onclick') && n._onclick===undefined ||
                    n._listeners && n._listeners.click && n._listeners.click.length)) {
                if (!n._hit) n._hit=nextHit++;
                if (Object.keys(state.hits).length>=96) throw new RangeError('interactive node limit');
                state.hits[n._hit]=n;attrs+=' data-pxa-click="'+n._hit+'"';
                if (tag!=='img' && tag!=='input' || type==='submit') tag='a';
            }
            if (render && tag==='input' && !disabled) {
                if (!n._hit) n._hit=nextHit++;
                state.hits[n._hit]=n;attrs+=' data-pxa-field="'+n._hit+'"';
            }
            if (attributes) for(const k of Object.keys(attributes))
                if (k!=='style' && !k.startsWith('on') && !k.startsWith('data-pxa-')) attrs+=' '+k+'="'+escape(attributes[k])+'"';
            if (n._style) style=n._style.text;
            if(style) attrs+=' style="'+escape(style)+'"';
            parts.push('<'+tag+attrs+'>');
            if(render && tag==='textarea')parts.push('\n'); // compensate the parser's initial-LF rule
            if(tag==='img' || tag==='source' || tag==='input' || tag==='br' || tag==='hr' || tag==='meta' || tag==='link') continue;
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
            if (type === 'click' && disabledControl(node)) return null;
            return node && node['on'+type]===undefined && node._attrs && node._attrs['on'+type] ? node : null;
        },
        open(tag, attributes, empty) {
            // Parsed nodes are fresh; avoid reentering the public DOM methods.
            const node = new Node(tag), parent=stack[stack.length-1];
            if(attributes.length)node._attrs=Object.create(null);
            let count=0;
            for (let i = 0; i < attributes.length; i += 2) {
                const key=attributes[i].toLowerCase(),value=attributes[i+1],attrs=node._attrs;
                if(own(attrs,key))continue; // HTML keeps the first duplicate attribute.
                if(!attributeNamePattern.test(key) || key.length>64 || value.length>4096 || ++count>24)
                    throw new RangeError('parsed attribute limit');
                if(key==='style') {
                    const declarations=parseStyle(value),text=styleText(declarations),data=styleState(node);
                    data.values=declarations.values;data.priorities=declarations.priorities;data.text=text;
                }
                attrs[key]=value;
            }
            if(parent.childNodes.length>=192)throw new RangeError('child limit');
            node.parentNode=parent;parent.childNodes.push(node);
            if(node.nodeName==='INPUT' && node.type.toLowerCase()==='radio' && node.checked) {
                const staged=stack[0][fragmentStage];if(staged)(staged.radios || (staged.radios=[])).push(node);else uncheckRadioPeers(node);
            }
            if(!fragments.length) {
                if (tag === 'html') document.documentElement = node;
                if (tag === 'body') document.body = node;
                if (tag === 'head') document.head = node;
            }
            if (!empty) stack.push(node);
        },
        text(text) { if (text) {
            const node=new Node('',text), parent=stack[stack.length-1];
            if(parent.childNodes.length>=192)throw new RangeError('child limit');
            node.parentNode=parent;parent.childNodes.push(node);
        } },
        close(tag) { for (let i = stack.length - 1; i > 0; --i) if (stack[i].nodeName.toLowerCase() === tag) { stack.length = i; break; } },
        beginFragment(node) { if (fragments.length >= 4) throw new RangeError('fragment nesting'); fragments.push(stack); stack = [node]; },
        endFragment() {
            const staged=stack[0],stage=staged[fragmentStage];stack=fragments.pop();
            if(stage && stage.radios && stage.radios.some(n=>n.name)) {
                // Validate the prospective tree before committing children,
                // changing outside peers or consuming the style allowance.
                const target=stage.target;let top=target;
                while(top.parentNode)top=top.parentNode;
                const todo=[top];let visited=0;
                while(todo.length) {
                    if(++visited>512)throw new RangeError('radio traversal limit');
                    const n=todo.pop(),children=n===target?staged.childNodes:n.childNodes;
                    for(let i=children.length-1;i>=0;--i)todo.push(children[i]);
                }
            }
        },
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
            const node = hits[id]; if (!node || disabledControl(node)) return;
            if (!dispatch(node, event('click'), true)) return;
            if (disabledControl(node)) return;
            if (node.nodeName === 'A' && node.href) native.navigate(node.href);
            else if (submitControl(node)) { const form = formOwner(node); if (form) submit(form, node); }
        },
        field(id, value, checked) { const node = hits[id]; if (node) { node.value = value; node.checked = checked; dispatch(node, event('input'), true); dispatch(node, event('change'), true); } }
    };
})

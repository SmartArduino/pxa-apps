// Signed event compatibility and native scheduler; instantiated only when used.
(function(core){
    'use strict';
    const {Node,changed,DOMException,Function,clickTarget,disabledControl,submitControl,formOwner,submit,native,ensureSelect,document,root}=core;
    // Module loading has a trusted bootstrap allowance. Do not invoke page-
    // replaced globals, Array iterators or inherited descriptor accessors here.
    const {Symbol,defineProperties}=core.intrinsics;
    const eventDispatch=Symbol('eventDispatch'), eventPassive=Symbol('eventPassive');
    class Event {
        constructor(type, options={}) {
            if(!arguments.length)throw new TypeError('Event type required');
            options=options==null?{}:options;
            this.type=String(type);this.bubbles=!!options.bubbles;this.cancelable=!!options.cancelable;
            this.composed=!!options.composed;this.target=null;this.currentTarget=null;
            this.defaultPrevented=false;this.eventPhase=0;this.isTrusted=false;this.timeStamp=native.now();
        }
        preventDefault(){if(this.cancelable && !this[eventPassive])this.defaultPrevented=true;}
        stopPropagation(){this.stopped=true;}
        stopImmediatePropagation(){this.stopped=true;this.immediate=true;}
        get cancelBubble(){return !!this.stopped;}
        set cancelBubble(value){if(value)this.stopped=true;}
        get returnValue(){return !this.defaultPrevented;}
        set returnValue(value){if(!value)this.preventDefault();}
    }
    class CustomEvent extends Event {
        constructor(type,options={}) {
            if(!arguments.length)throw new TypeError('Event type required');
            super(type,options);const detail=options==null?null:options.detail;this.detail=detail===undefined?null:detail;
        }
    };
    const constants={__proto__:null,NONE:{__proto__:null,value:0},CAPTURING_PHASE:{__proto__:null,value:1},
        AT_TARGET:{__proto__:null,value:2},BUBBLING_PHASE:{__proto__:null,value:3}};
    defineProperties(Event,constants);defineProperties(Event.prototype,constants);
    function event(type,bubbles=false,cancelable=false) { return new Event(type,{bubbles,cancelable}); }
    const eventCall=Symbol('eventCall');
    function dispatchAt(at,e,capture=false,phase=2) {
        e.currentTarget=at;e.eventPhase=phase;
        // Clone before invoking website code. Removal marks shared records,
        // while additions wait for a later invocation of this target/phase.
        const list=at._listeners && at._listeners[e.type],listeners=list?list.slice():null;
        let handler=capture?null:at['on'+e.type];
        if(!capture && handler===undefined && at._attrs && at._attrs['on'+e.type]) {
            handler=new Function('event',at._attrs['on'+e.type]);at['on'+e.type]=handler;
        }
        if(!capture && typeof handler==='function') {
            let result;at[eventCall]=handler;
            try {result=at[eventCall](e);}finally {at[eventCall]=null;}
            if(result===false)e.preventDefault();
        }
        if(listeners)for(const listener of listeners) {
            if(e.immediate)break;
            if(listener.removed || listener.capture!==capture)continue;
            const callback=listener.callback;
            if(listener.once)removeListener(at,e.type,listener);
            const passive=e[eventPassive];e[eventPassive]=listener.passive;
            try {
                if(typeof callback==='function') {
                    at[eventCall]=callback;
                    try {at[eventCall](e);}finally {at[eventCall]=null;}
                } else callback.handleEvent(e);
            } finally {e[eventPassive]=passive;}
        }
    }
    function beginEvent(node,e,bubble) {
        if(!e || typeof e!=='object' || !e.type)throw new TypeError('Event required');
        if(e[eventDispatch])throw new DOMException('Event already being dispatched','InvalidStateError');
        const path=[];
        for(let at=node;at;at=at instanceof Node?at.parentNode:null) {
            if(path.length>=192)throw new RangeError('event path limit');
            path.push(at);
        }
        if(path[path.length-1]===document && e.type!=='load') {
            if(path.length>=192)throw new RangeError('event path limit');
            path.push(root);
        }
        const state={node,event:e,path,cursor:path.length,capture:true,at:null,
            bubble:e.bubbles===undefined?bubble:!!e.bubbles,phase:0};
        e[eventDispatch]=true;e.target=node;
        try {nextEventTarget(state);}catch(error){endEvent(e);throw error;}
        return state;
    }
    function nextEventTarget(state) {
        state.at=null;
        if(state.event.stopped)return;
        if(state.capture) {
            while(--state.cursor>=0) {
                const node=state.path[state.cursor],list=node._listeners && node._listeners[state.event.type];
                if(list && list.some(listener=>!listener.removed && listener.capture)) {
                    state.at=node;state.phase=state.cursor===0?2:1;return;
                }
            }
            state.capture=false;state.cursor=-1;
        }
        if(++state.cursor<state.path.length && (state.cursor===0 || state.bubble)) {
            state.at=state.path[state.cursor];state.phase=state.cursor===0?2:3;
        }
    }
    function endEvent(e) {
        e.currentTarget=null;e.eventPhase=0;e.stopped=false;e.immediate=false;e[eventDispatch]=false;
    }
    function dispatch(node, e, bubble = false) {
        const state=beginEvent(node,e,bubble);
        try {
            while(state.at){dispatchAt(state.at,e,state.capture,state.phase);nextEventTarget(state);}
        } finally {endEvent(e);}
        return !e.defaultPrevented;
    }
    function captureOption(options) {
        return typeof options==='boolean'?options:!!(options && options.capture);
    }
    function listen(target,type,callback,options) {
        type = String(type);
        if(callback!=null && typeof callback!=='function' && typeof callback!=='object')throw new TypeError('Listener required');
        const capture=captureOption(options),object=options && (typeof options==='object' || typeof options==='function');
        const once=!!(object && options.once),passive=!!(object && options.passive);
        if(callback==null)return;
        const node = type === 'click' && target instanceof Node, before = node && clickTarget(target);
        if (!target._listeners) target._listeners = Object.create(null);
        const list = target._listeners[type] || (target._listeners[type] = []);
        if (list.some(listener=>listener.callback===callback && listener.capture===capture)) return;
        if (list.length >= 16) throw new RangeError('event listener limit');
        list.push({callback,capture,once,passive,removed:false});
        if (node && before !== clickTarget(target)) changed(target);
    }
    function removeListener(target,type,listener) {
        const list=target._listeners && target._listeners[type];
        if(!list || listener.removed)return;
        const node=type==='click' && target instanceof Node,before=node && clickTarget(target);
        listener.removed=true;listener.callback=null;
        list.splice(list.indexOf(listener),1);
        if(!list.length)delete target._listeners[type];
        if(node && before!==clickTarget(target))changed(target);
    }
    function unlisten(target,type,callback,options) {
        type = String(type);
        const capture=captureOption(options);
        const list = target._listeners && target._listeners[type];
        if (!list) return;
        const listener=list.find(listener=>listener.callback===callback && listener.capture===capture);
        if(listener)removeListener(target,type,listener);
    }

    // Native input is one serialized action. Compilation, each bubbling node
    // and default activation can yield without running other page callbacks or
    // publishing a partial DOM. Script dispatchEvent remains synchronous.
    let nativeEvent=null;
    function startNativeEvent(node,type,kind,submitter=null) {
        const e=event(type,true,type==='click' || type==='submit');if(type==='submit')e.submitter=submitter;
        nativeEvent=beginEvent(node,e,true);nativeEvent.kind=kind;return true;
    }
    function cancelNativeEvent() {
        if(nativeEvent) {
            if(nativeEvent.kind==='submit')nativeEvent.node._submitting=false;
            endEvent(nativeEvent.event);
        }
        nativeEvent=null;
    }
    function stepNativeEvent() {
        const state=nativeEvent;if(!state)return false;
        if(state.at) {
            dispatchAt(state.at,state.event,state.capture,state.phase);
            nextEventTarget(state);
            if(state.at)return true;
            // Default activation has its own ordinary execution budget.
            endEvent(state.event);return true;
        }
        if(state.kind==='input')return startNativeEvent(state.node,'change','change');
        if(state.kind==='submit') {
            try {if(!state.event.defaultPrevented)submit(state.node,state.event.submitter,false);}
            finally {state.node._submitting=false;nativeEvent=null;}
        } else {
            nativeEvent=null;
            if(state.kind==='click' && !state.event.defaultPrevented && !disabledControl(state.node)) {
                if(state.node.nodeName==='A' && state.node.href)native.navigate(state.node.href);
                else if(submitControl(state.node)) {
                    const form=formOwner(state.node);
                    if(form && !form._submitting) {
                        form._submitting=true;return startNativeEvent(form,'submit','submit',state.node);
                    }
                }
            }
        }
        return false;
    }
    return {Event,CustomEvent,event,dispatch,listen,unlisten,
        handler(){const node=nativeEvent && !nativeEvent.capture && nativeEvent.at,type=nativeEvent && nativeEvent.event.type;return node && node['on'+type]===undefined && node._attrs && node._attrs['on'+type]?nativeEvent:null;},
        click(id){const node=core.hits()[id];return !!node && !disabledControl(node) && startNativeEvent(node,'click','click');},
        step:stepNativeEvent,cancel:cancelNativeEvent,
        field(id, value, checked) {
            const node=core.hits()[id];
            if(!node || disabledControl(node))return false;
            if(node.nodeName==='SELECT')return ensureSelect().field(node,value) && startNativeEvent(node,'input','input');
            if(node.nodeName!=='INPUT' && node.nodeName!=='TEXTAREA')return false;
            const checkable=node.nodeName==='INPUT' && (node.type.toLowerCase()==='checkbox' || node.type.toLowerCase()==='radio');
            const valueChanged=node.value!==value;
            if(!valueChanged && (!checkable || node.checked===checked))return false;
            if(valueChanged)node.value=value;if(checkable)node.checked=checked;
            return startNativeEvent(node,'input','input');
        }
    };
})

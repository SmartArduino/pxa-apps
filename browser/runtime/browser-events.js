// Signed native-event scheduler; instantiated only on the first UI action.
(function(core){
    'use strict';
    const {event,dispatchAt,disabledControl,submitControl,formOwner,submit,native,ensureSelect}=core;
    // Native input is one serialized action. Compilation, each bubbling node
    // and default activation can yield without running other page callbacks or
    // publishing a partial DOM. Script dispatchEvent remains synchronous.
    let nativeEvent=null;
    function startNativeEvent(node,type,kind,submitter=null) {
        const e=event(type);e.target=node;if(type==='submit')e.submitter=submitter;
        nativeEvent={node,at:node,event:e,kind,depth:0};return true;
    }
    function cancelNativeEvent() {
        if(nativeEvent && nativeEvent.kind==='submit')nativeEvent.node._submitting=false;
        nativeEvent=null;
    }
    function stepNativeEvent() {
        const state=nativeEvent;if(!state)return false;
        if(state.at && state.depth++<192) {
            const at=state.at;dispatchAt(at,state.event);
            state.at=state.event.stopped?null:at.parentNode;
            if(state.at && state.depth<192)return true;
            // Default activation has its own ordinary execution budget.
            state.at=null;return true;
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
    return {
        handler(){const node=nativeEvent && nativeEvent.at,type=nativeEvent && nativeEvent.event.type;return node && node['on'+type]===undefined && node._attrs && node._attrs['on'+type]?nativeEvent:null;},
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

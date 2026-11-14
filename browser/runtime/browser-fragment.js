// App-owned optional fragment support; loaded only on first fragment use.
(function(core) {
    'use strict';
    const {Node,changed,DOMException,before,mutation,uncheckRadioPeers,activateScripts,elementById}=core;
    function DocumentFragment() {
        if(!new.target)throw new TypeError('DocumentFragment requires new');
        const node=new Node('#document-fragment');node.nodeType=11;node.nodeName='#document-fragment';
        Object.setPrototypeOf(node,DocumentFragment.prototype);return node;
    }
    Object.setPrototypeOf(DocumentFragment.prototype,Node.prototype);
    DocumentFragment.prototype.getElementById=function(id) {
        if(!(this instanceof DocumentFragment))throw new TypeError('DocumentFragment required');
        return elementById(this,String(id));
    };
    function hierarchy() {throw new DOMException('Invalid fragment insertion','HierarchyRequestError');}
    function insert(parent,fragment,reference) {
        if(!(parent instanceof Node) || !(fragment instanceof DocumentFragment))throw new TypeError('Node required');
        if(parent.nodeType!==1 && parent.nodeType!==9 && parent.nodeType!==11)hierarchy();
        if(reference!==null && (!(reference instanceof Node) || reference.parentNode!==parent || !parent.childNodes.includes(reference)))
            throw new DOMException('Child not found','NotFoundError');
        let top=parent,depth=0;
        for(let at=parent;at;at=at.parentNode) {
            if(++depth>512)throw new RangeError('DOM ancestry limit');
            if(at===fragment)hierarchy();top=at;
        }
        const nodes=fragment.childNodes.slice();
        if(nodes.length>192 || parent.childNodes.length+nodes.length>192)throw new RangeError('child limit');
        if(!nodes.length)return fragment;
        // Validate all roots and bounded side effects before moving any node.
        // Keep source arrays live and preserve node/handler identity on failure.
        const todo=[],radios=[];let visited=0;
        for(let i=nodes.length-1;i>=0;--i) {
            if(!(nodes[i] instanceof Node) || nodes[i].parentNode!==fragment)hierarchy();
            todo.push(nodes[i]);
        }
        while(todo.length) {
            if(++visited>512)throw new RangeError('fragment traversal limit');
            const node=todo.pop();
            if(!(node instanceof Node) || node.nodeType!==1 && node.nodeType!==3)hierarchy();
            if(node.nodeName==='INPUT' && node.checked && node.type.toLowerCase()==='radio' && node.name)radios.push(node);
            for(let i=node.childNodes.length-1;i>=0;--i)todo.push(node.childNodes[i]);
        }
        if(radios.length) {
            const scan=[top];let count=visited;
            while(scan.length) {
                if(++count>512)throw new RangeError('radio traversal limit');
                const node=scan.pop();for(let i=node.childNodes.length-1;i>=0;--i)scan.push(node.childNodes[i]);
            }
        }
        before(parent);
        let index=reference===null?parent.childNodes.length:parent.childNodes.indexOf(reference);
        fragment.childNodes.length=0;
        for(const node of nodes) {parent.childNodes.splice(index++,0,node);node.parentNode=parent;}
        changed(parent);
        for(const node of nodes)mutation(parent,node);
        // Last checked radio in tree order wins after form ownership changes.
        for(let i=radios.length-1;i>=0;--i)if(radios[i].checked)uncheckRadioPeers(radios[i]);
        for(const node of nodes)if(node.nodeName==='SCRIPT' || node._hasScripts) {
            for(let at=parent;at;at=at.parentNode)at._hasScripts=true;
            activateScripts(node);
        }
        return fragment;
    }
    return {DocumentFragment,insert};
})

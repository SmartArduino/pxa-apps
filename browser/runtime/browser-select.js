// Signed optional DOM extension; instantiated only when a select/option is used.
(function (core) {
    'use strict';
    const {Node,changed,domString,collectionItem,formOwner,DOMException}=core,own=Object.hasOwn;
    const optionState=Symbol('optionSelection'),selectDirty=Symbol('selectDirty'),selectPreferred=Symbol('selectPreferred');
    const optionIdentity=Symbol('optionIdentity');let nextIdentity=1;
    let selectProto=null,optionProto=null,selectSeen=false;
    function nearestSelect(node) {
        if(!selectSeen)return null;
        let groups=0,depth=0;
        for(let at=node.parentNode;at;at=at.parentNode) {
            if(++depth>192)throw new RangeError('select ancestry limit');
            if(at.nodeName==='OPTION' || at.nodeName==='DATALIST' || at.nodeName==='HR')return null;
            if(at.nodeName==='OPTGROUP' && ++groups>1)return null;
            if(at.nodeName==='SELECT')return at;
        }
        return null;
    }
    function selectionContainer(node) {
        return node.nodeName==='SELECT'?node:node.nodeName==='OPTION'?null:nearestSelect(node);
    }
    function selectionBefore(node) {
        const select=selectionContainer(node);
        if(select && select[selectDirty]!==false)normalizeSelect(select);
    }
    function selectionBeforeAttribute(node,name) {
        if(name!=='selected' && name!=='disabled' && name!=='size' && name!=='multiple')return;
        if(node.nodeName==='SELECT')selectionBefore(node);
        else if(node.nodeName==='OPTION' || node.nodeName==='OPTGROUP') {
            const select=nearestSelect(node);if(select && select[selectDirty]!==false)normalizeSelect(select);
        }
    }
    function rawSelected(node) {return own(node,optionState)?node[optionState]&1:node.hasAttribute('selected');}
    function identity(node) {
        if(!node[optionIdentity]) {
            if(nextIdentity>4294967295)throw new RangeError('option identity limit');
            node[optionIdentity]=nextIdentity++;
        }
        return node[optionIdentity];
    }
    function selectOptions(node,work={visited:0}) {
        const result=[],todo=[];
        for(let i=node.childNodes.length-1;i>=0;--i)todo.push(node.childNodes[i]);
        while(todo.length) {
            if(++work.visited>512)throw new RangeError('select traversal limit');
            const n=todo.pop();
            if(n.nodeName==='OPTION' && nearestSelect(n)===node)result.push(n);
            for(let i=n.childNodes.length-1;i>=0;--i)todo.push(n.childNodes[i]);
        }
        return result;
    }
    function disabledOption(node) {
        if(node.disabled)return true;
        let depth=0;
        for(let at=node.parentNode;at;at=at.parentNode) {
            if(++depth>192)throw new RangeError('select ancestry limit');
            if(at.nodeName==='SELECT' || at.nodeName==='OPTION' || at.nodeName==='DATALIST' || at.nodeName==='HR')return false;
            if(at.nodeName==='OPTGROUP')return at.disabled;
        }
        return false;
    }
    function normalizeSelect(node,list=selectOptions(node)) {
        if(node[selectDirty]!==false) {
            let last=null,first=null;const preferred=node[selectPreferred];
            for(const option of list) {
                if(!first && !disabledOption(option))first=option;
                if(rawSelected(option))last=option;
            }
            if(!node.multiple) {
                if(preferred && list.includes(preferred) && rawSelected(preferred))last=preferred;
                if(!last && node.size<=1)last=first;
                for(const option of list)option[optionState]=(option[optionState]&2)|(option===last?1:0);
            }
            node[selectPreferred]=null;node[selectDirty]=false;
        }
        return list;
    }
    function selectionMutation(parent,child) {
        if(!child || child.nodeType===3)return;
        const select=selectionContainer(parent);if(!select)return;
        const todo=[child];let visited=0;
        while(todo.length) {
            if(++visited>512)throw new RangeError('select traversal limit');
            const node=todo.pop();
            if(node.nodeName==='OPTION') {
                if(nearestSelect(node)===select) {
                    select[selectDirty]=true;
                    if(rawSelected(node))select[selectPreferred]=node;
                }
                continue;
            }
            for(let i=node.childNodes.length-1;i>=0;--i)todo.push(node.childNodes[i]);
        }
    }
    function selectionAttribute(node,name) {
        if(node.nodeName==='OPTION' && (name==='selected' || name==='disabled')) {
            if(name==='selected' && !(node[optionState]&2))node[optionState]=node.hasAttribute('selected')?1:0;
            const select=nearestSelect(node);
            if(select){select[selectDirty]=true;if(name==='selected' && rawSelected(node))select[selectPreferred]=node;}
            return true;
        } else if(node.nodeName==='OPTGROUP' && name==='disabled') {
            const select=nearestSelect(node);if(select)select[selectDirty]=true;return true;
        } else if(node.nodeName==='SELECT' && (name==='multiple' || name==='size')) {
            if(name==='multiple' && !node.multiple)node[selectPreferred]=selectOptions(node).find(rawSelected)||null;
            node[selectDirty]=true;return true;
        }
        return false;
    }
    function optionText(node,work={visited:0}) {
        const todo=[],parts=[];let bytes=0;
        for(let i=node.childNodes.length-1;i>=0;--i)todo.push(node.childNodes[i]);
        while(todo.length) {
            if(++work.visited>512)throw new RangeError('option traversal limit');
            const n=todo.pop();
            if(n.nodeType===3){bytes+=n._text.length;if(bytes>4096)throw new RangeError('option text limit');parts.push(n._text);}
            else if(n.nodeName!=='SCRIPT')for(let i=n.childNodes.length-1;i>=0;--i)todo.push(n.childNodes[i]);
        }
        return parts.join('').replace(/[\t\n\f\r ]+/g,' ').replace(/^ | $/g,'');
    }
    function setSelectIndex(node,index,list=normalizeSelect(node)) {
        let updated=false;
        for(let i=0;i<list.length;++i) {
            const selected=i===index;updated ||= !!rawSelected(list[i])!==selected;
            list[i][optionState]=(list[i][optionState]&2)|(selected?3:0);
        }
        if(updated)changed(node);
    }
    function selectPrototype() {
        selectSeen=true;
        if(selectProto)return selectProto;
        selectProto=Object.create(Node.prototype);
        Object.defineProperties(selectProto,{
            multiple:{get(){return this.hasAttribute('multiple');},set(v){if(v)this.setAttribute('multiple','');else this.removeAttribute('multiple');}},
            value:{get(){const option=normalizeSelect(this).find(rawSelected);return option?option.value:'';},
                set(v){v=domString(v);const list=normalizeSelect(this);setSelectIndex(this,list.findIndex(n=>n.value===v),list);}},
            type:{get(){return this.multiple?'select-multiple':'select-one';},set(_){}},
            options:{get(){return Object.defineProperty(normalizeSelect(this),'item',{value:collectionItem});}},
            selectedOptions:{get(){return Object.defineProperty(normalizeSelect(this).filter(rawSelected),'item',{value:collectionItem});}},
            selectedIndex:{get(){return normalizeSelect(this).findIndex(rawSelected);},set(v){setSelectIndex(this,Number(v)>>0);}},
            length:{get(){return selectOptions(this).length;}},
            size:{get(){const n=parseInt(this.getAttribute('size')||'',10);return n>=0 && n<=4294967295?n:0;},set(v){this.setAttribute('size',Number(v)>>>0);}},
            form:{get(){return formOwner(this);}}
        });
        selectProto.add=function(node,before=null) {
            if(!(node instanceof Node) || node.nodeName!=='OPTION' && node.nodeName!=='OPTGROUP')throw new TypeError('option or optgroup required');
            if(typeof before==='number')before=selectOptions(this)[before>>0]||null;
            if(before && (!(before instanceof Node) || nearestSelect(before)!==this))throw new DOMException('Option not found','NotFoundError');
            (before?before.parentNode:this).insertBefore(node,before);
        };
        selectProto.remove=function(index=0){const option=selectOptions(this)[Number(index)>>0];if(option)option.remove();};
        return selectProto;
    }
    function optionPrototype() {
        if(optionProto)return optionProto;
        optionProto=Object.create(Node.prototype);
        Object.defineProperties(optionProto,{
            value:{get(){const value=this.getAttribute('value');return value===null?optionText(this):value;},set(v){this.setAttribute('value',v);}},
            label:{get(){return this.getAttribute('label')||optionText(this);},set(v){this.setAttribute('label',v);}},
            text:{get(){return optionText(this);},set(v){v=domString(v);if(v.length>4096)throw new RangeError('option text limit');this.textContent=v;}},
            selected:{get(){const select=nearestSelect(this);if(select)normalizeSelect(select);return !!rawSelected(this);},
                set(v){
                    const select=nearestSelect(this),list=select?normalizeSelect(select):null,selected=!!v;
                    this[optionState]=(selected?1:0)|2;
                    if(select){select[selectDirty]=true;if(selected)select[selectPreferred]=this;normalizeSelect(select,list);}
                    changed(this);
                }},
            defaultSelected:{get(){return this.hasAttribute('selected');},set(v){if(v)this.setAttribute('selected','');else this.removeAttribute('selected');}},
            index:{get(){const select=nearestSelect(this);return select?selectOptions(select).indexOf(this):0;}},
            form:{get(){const select=nearestSelect(this);return select?formOwner(select):null;}}
        });
        Option.prototype=optionProto;return optionProto;
    }
    function Option(text='',value,defaultSelected=false,selected=false) {
        const node=new Node('option');node.text=text;
        if(arguments.length>1)node.value=value;if(defaultSelected)node.defaultSelected=true;
        node[optionState]=selected?1:0;return node;
    }
    function nativeField(node,value) {
        const list=normalizeSelect(node),selected=new Array(list.length).fill(false),identities=[];
        for(const option of list)identities.push(option[optionIdentity]||0);
        if(value.length>704 || value && !/^[1-9][0-9]*(?:,[1-9][0-9]*)*$/.test(value))return false;
        const tokens=value?value.split(','):[];if(tokens.length>64 || !node.multiple && tokens.length>1)return false;
        for(const item of tokens) {
            const i=identities.indexOf(Number(item));if(i<0 || selected[i])return false;
            if(!rawSelected(list[i]) && (disabledOption(list[i]) || list[i].hidden))return false;
            selected[i]=true;
        }
        let updated=false;
        for(let i=0;i<list.length;++i)if(!!rawSelected(list[i])!==selected[i]) {list[i][optionState]=(selected[i]?1:0)|2;updated=true;}
        if(updated)changed(node);return updated;
    }
    return {selectPrototype,optionPrototype,Option,before:selectionBefore,beforeAttribute:selectionBeforeAttribute,attribute:selectionAttribute,mutation:selectionMutation,normalize:normalizeSelect,options:selectOptions,nearest:nearestSelect,raw:rawSelected,disabled:disabledOption,text:optionText,identity,field:nativeField};
})

"""Generic array semantics; no website replacement or site-specific source."""
def cases():
    bodies={
      'sparse-splice-left': "var a=new Array(100000);a[8]='x';var r=a.splice(1,1);ok(a.length===99999&&a[7]==='x'&&!own(a,8)&&r.length===1&&!own(r,0));",
      'sparse-splice-right': "var a=new Array(100000);a[8]='x';a.splice(1,0,'new');ok(a.length===100001&&a[1]==='new'&&a[9]==='x'&&!own(a,8));",
      'sparse-copy-holes': "var a=new Array(10000);a[8]='old';a[100]='x';a.copyWithin(0,50,1000);ok(a[50]==='x'&&!own(a,8)&&!own(a,100)&&a.length===10000);",
      'sparse-copy-backward': "var a=new Array(10000);a[2]='a';a[8]='b';a.copyWithin(2,0,9998);ok(a[4]==='a'&&a[10]==='b'&&!own(a,2)&&!own(a,8));",
      'sparse-dense-prefix': "var a=[0,1,2,3,4,5,6,7];a.length=100000;a.copyWithin(0,2);ok(a[0]===2&&a[5]===7&&!own(a,6)&&!own(a,7)&&a.length===100000);",
      'sparse-named-getter': "var a=new Array(100000);a[8]='x';Object.defineProperty(a,'ignored',{get(){throw Error('named getter visited')}});a.copyWithin(0,1);ok(a[7]==='x'&&!own(a,8));",
      'sparse-own-getter': "var a=new Array(1024);Object.defineProperty(a,8,{get(){a[900]='later';return 'x'},configurable:true});a.copyWithin(0,1);ok(a[7]==='x'&&a[899]==='later'&&!own(a,8));",
      'sparse-inherited-index': "var a=new Array(1024);a[8]='x';Array.prototype[900]='inherited';try{a.copyWithin(0,1);ok(a[7]==='x'&&own(a,899)&&a[899]==='inherited')}finally{delete Array.prototype[900]}",
      'sparse-proxy-prototype': "var a=new Array(1024),n=0,last='';Object.setPrototypeOf(a,new Proxy(Object.create(null),{has(t,k){n++;last=k;return false}}));Array.prototype.copyWithin.call(a,0,1);ok(n===1023&&last==='1023');",
      'sparse-nonconfigurable': "var a=new Array(1024);a[4]='four';Object.defineProperty(a,8,{value:'x',configurable:false,writable:false});var threw=false;try{a.copyWithin(0,1)}catch(e){threw=e instanceof TypeError}ok(threw&&a[3]==='four'&&!own(a,4)&&a[7]==='x'&&a[8]==='x');",
      'sparse-nonextensible': "var a=new Array(1024);a[8]='x';Object.preventExtensions(a);var threw=false;try{a.copyWithin(0,1)}catch(e){threw=e instanceof TypeError}ok(threw&&a[8]==='x'&&!own(a,7));",
      'sparse-u32-boundary': "var a=new Array(4294967295);a[8]='x';Object.defineProperty(a,'4294967295',{value:'sentinel',configurable:false,writable:true});var threw=false;try{a.unshift('new')}catch(e){threw=e instanceof TypeError}ok(threw&&a[8]==='x'&&!own(a,9)&&a[4294967295]==='sentinel');",
      'sparse-null-prototype': "var a=new Array(100000);a[8]='x';Object.setPrototypeOf(a,null);Array.prototype.copyWithin.call(a,0,1);ok(a[7]==='x'&&!own(a,8));",
      'sparse-max-length': "var a=new Array(4294967295);a[8]='x';a[4294967295]='sentinel';a.copyWithin(0,1);ok(a.length===4294967295&&a[7]==='x'&&!own(a,8)&&a[4294967295]==='sentinel');",
    }
    prefix="function ok(v){if(!v)throw Error('sparse array semantics')}function own(a,k){return Object.prototype.hasOwnProperty.call(a,k)};"
    return {name:prefix+body for name,body in bodies.items()}

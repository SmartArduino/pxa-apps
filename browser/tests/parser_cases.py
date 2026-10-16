"""Generated, valid JavaScript nesting cases; no downloaded website code."""


def cases(depth=100):
    return {
        'unary': '!' * depth + 'true',
        'group': '(' * depth + '1' + ')' * depth,
        'array': '[' * depth + '1' + ']' * depth,
        'object': 'var a=' + '{a:' * depth + '1' + '}' * depth,
        'function': 'function f(){' * depth + '1;' + '}' * depth,
        'block': '{' * depth + '1;' + '}' * depth,
        'conditional': 'true?1:' * depth + '1',
        'assignment': 'a=' * depth + '1',
        'logical': 'true&&(' * depth + 'true' + ')' * depth,
        'power': '1**' * depth + '1',
        'arrow': '()=>' * depth + '1',
        'class': 'class C{m(){' * (depth // 2) + '1;' + '}}' * (depth // 2),
        'template': '`x${' * depth + '1' + '}`' * depth,
        'json-parse': "JSON.parse('" + '[' * depth + '0' + ']' * depth + "')",
        'json-stringify': 'var v=0;for(var i=0;i<100;i++)v=[v];JSON.stringify(v)',
        'destructure': 'let ' + '[' * depth + 'a' + ']' * depth + '=[]',
        'factory-budget': "var v=(function(){return (function(){return (function(){return [{a:17}][0].a})()})()})();",
    }


def supported_cases(depth=100):
    json = "JSON.parse('" + '[' * depth + '0' + ']' * depth + "')"
    return {
        'nested-factories': "var v=(function(){var n=10;return (function(){var x=7;return (function(){return n+x})()})()})();if(v!==17)throw Error('factory');",
        'member-grammar': "class C{#x=3;m(){return this.#x}static ok(o){return #x in o}};var c=new C;if(c.m()!==3||!C.ok(c)||C.ok({}))throw Error('class');var o={v:4,m(){return this.v}};if(o?.m?.()!==4||null?.m!==undefined||new (function(){this.x=7})().x!==7)throw Error('member');",
        'supported-data': "window.__NUXT__={state:{reader:{},shelf:{books:[{bookId:'fixture',title:'Synthetic book'}],bookReaderUrls:[{bId:'fixture',param:'fixture-only'}],totalCount:1,kkIdx:2}}};if(window.__NUXT__.state.shelf.books[0].bookId!=='fixture')throw Error('data');",
        'getter-reentry': "var caught=false;var v={get x(){try{" + json + "}catch(e){caught=e instanceof InternalError}return 1}};if(JSON.stringify(v)!=='{\"x\":1}' || !caught)throw Error('getter');",
        'reviver-reentry': "var caught=false;var v=JSON.parse('[1]',function(k,v){if(k==='0'){try{" + json + "}catch(e){caught=e instanceof InternalError}}return v});if(!caught || v[0]!==1)throw Error('reviver');",
        'eval-reentry': "var caught=false;var v={get x(){try{eval('!'.repeat(1000)+'true')}catch(e){caught=e instanceof InternalError}return 1}};if(JSON.stringify(v)!=='{\"x\":1}' || !caught)throw Error('eval');",
    }

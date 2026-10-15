"""Live search / complete TOC / multipart chapter checks for each shipped rule.

Only metadata and hashes are written; parser operations use the shipped C++
implementation. A failed website is a failed check, never a fixture pass.
"""
import argparse, concurrent.futures, hashlib, json, os, subprocess, time
from pathlib import Path
from urllib.parse import quote, urljoin
from urllib.request import Request, urlopen

ROOT=next(p for p in Path(__file__).resolve().parents if (p/'tools/app.sh').exists())
APP=Path(__file__).resolve().parents[1]
CASES=[dict(key='nunu',query='长夜难明',match='/xuanyi/changyenanming/',indexes=[0,11]),
       dict(key='shuzhaige',query='三体',match='/santi/',indexes=[0,1]),
       dict(key='biquche',query='庆余年',match='/book/0/16/',indexes=[0,12]),
       dict(key='wenlinshe',query='儒林外史',match='/ru-lin-wai-shi.html',indexes=[0])]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
driver=a.output/'source_probe'
subprocess.run([os.environ.get('CXX','clang++'),'-std=c++2c','-O3','-Wno-attributes','-I'+str(ROOT/'deps/pxa-system/sdk/guest-cpp/include'),
    str(APP/'tests/source_probe.cpp'),'-o',str(driver)],check=True)
compiled=json.loads(subprocess.run([str(driver),'configs'],capture_output=True,check=True).stdout)
assert len(compiled)==len(CASES)
for config,case in zip(compiled,CASES):
    assert config==json.loads((APP/'source-examples'/(case['key']+'.json')).read_text()),'Built-in rules and downloadable examples differ'

def check(case):
    rule=APP/'source-examples'/(case['key']+'.json');config=json.loads(rule.read_text());requests=[]
    def parse(mode,data,base=None):
        args=[str(driver),mode,str(rule)]+([base] if base else [])
        return json.loads(subprocess.run(args,input=data,capture_output=True,check=True).stdout)
    def fetch(url,body=None,limit=96*1024):
        headers={}
        if config.get('referer'):headers['Referer']=config['referer']
        if config.get('userAgent'):headers['User-Agent']=config['userAgent']
        if body is not None:headers['Content-Type']='application/x-www-form-urlencoded'
        begin=time.monotonic()
        with urlopen(Request(url,data=body,headers=headers),timeout=20) as r:
            data=r.read(limit+1);assert r.status==200 and len(data)<=limit,(url,len(data),limit)
            requests.append(dict(url=url,method='POST' if body is not None else 'GET',status=r.status,final_url=r.url,
                bytes=len(data),sha256=hashlib.sha256(data).hexdigest(),ms=round((time.monotonic()-begin)*1000,2)))
            return data
    try:
        search=config['search'];url=config['searchUrl'].replace('{{key}}',quote(case['query'],safe=''))
        form=search['body'].replace('{{key}}',quote(case['query'],safe='')).encode() if search.get('method')=='POST' else None
        books=parse('books',fetch(url,form))['shelf'];assert books
        book=next(b for b in books if b['toc'].endswith(case['match']))
        raw=fetch(book['toc']);chapters=[];paged=config['toc'].get('paged')
        if not paged:chapters=parse('chapters',raw,book['toc'])
        elif paged.get('format')=='html':
            count=parse('pagecount',raw);assert count['current']==1
            chapters=parse('chapters',raw,book['toc'])
            for index in range(1,count['total']):
                address=urljoin(book['toc'],paged['url'].replace('{{page}}',str(index+paged.get('start',1))))
                page=fetch(address);page_count=parse('pagecount',page)
                assert page_count==dict(current=index+1,total=count['total'])
                chapters+=parse('chapters',page,address)
        else:
            book_id=subprocess.run([str(driver),'id',str(rule)],input=raw,capture_output=True,check=True).stdout.decode()
            index=0;pages=1;total=0
            while index<pages:
                address=urljoin(book['toc'],paged['url'].replace('{{id}}',quote(book_id,safe='')).replace('{{page}}',str(index+paged.get('start',0))))
                page=parse('page',fetch(address),address)
                assert not index or (page['total']==total and page['pages']==pages)
                total=page['total'];pages=page['pages'];chapters+=page['chapters'];index+=1
            assert len(chapters)==total
        assert 0<len(chapters)<=1024 and len({c['url'] for c in chapters})==len(chapters)
        bodies=[]
        for index in case['indexes']:
            chapter=chapters[index];address=chapter['url'];seen=set();texts=[];declared=0
            for part_index in range(128):
                assert address not in seen;seen.add(address)
                raw=fetch(address,limit=65536)
                envelope=json.dumps(dict(html=raw.decode('utf-8'),original=chapter['url'],current=address,index=part_index),ensure_ascii=False).encode()
                part=parse('part',envelope)
                assert not part_index or declared==part['pages'];declared=part['pages']
                texts.append(part['text']);assert sum(len(t.encode())+1 for t in texts)<=65536
                if not part['next']:break
                address=part['next']
            else:raise AssertionError('Too many chapter parts')
            text='\n'.join(texts).encode();assert len(text)>80
            bodies.append(dict(index=index,title=chapter['title'],url=chapter['url'],parts=len(texts),bytes=len(text),sha256=hashlib.sha256(text).hexdigest()))
        if case['key']=='biquche':assert max(b['parts'] for b in bodies)>1
        # An exact-match redirect and a regular results page must both work.
        extra=None
        if case['key']=='wenlinshe':
            extra=parse('books',fetch(config['searchUrl'].replace('{{key}}',quote('三国',safe=''))))['shelf'];assert len(extra)>1
        return dict(key=case['key'],passed=True,name=config['name'],query=case['query'],results=len(books),
                    extra_search_results=len(extra) if extra else 0,result_index=books.index(book),book=book,toc_count=len(chapters),bodies=bodies,requests=requests,
                    rule_sha256=hashlib.sha256(rule.read_bytes()).hexdigest())
    except Exception as e:
        return dict(key=case['key'],passed=False,error=str(e),requests=requests)

with concurrent.futures.ThreadPoolExecutor(max_workers=len(CASES)) as pool:results=list(pool.map(check,CASES))
report=dict(passed=all(r['passed'] for r in results),sources=results)
(a.output/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
for row in results:print(json.dumps({k:v for k,v in row.items() if k not in ['requests','book','bodies']},ensure_ascii=False))
assert report['passed'],'At least one live website failed; inspect report.json'

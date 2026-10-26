#!/usr/bin/env python3
"""Generate a readonly directory index; resource bytes stay streamed assets."""
import json
from pathlib import Path
import sys

root, output = map(Path, sys.argv[1:])
directories = {"data": {}}
files=[]
offset=0
for path in sorted(root.rglob("*")):
    if not path.is_file():
        continue
    relative = path.relative_to(root)
    size=path.stat().st_size
    files.append(("data/"+relative.as_posix(),offset+512,size))
    offset+=512+(size+511)//512*512
    parts = ("data", *relative.parts)
    for i in range(1, len(parts)):
        parent = "/".join(parts[:i])
        directories.setdefault(parent, {})[parts[i]] = i < len(parts)-1
    if len(("assets/data/"+str(relative)).encode("utf-8")) >= 256:
        raise ValueError(f"Asset path too long: {relative}")
lines = ["/* Generated names/USTAR offsets only. Resource bytes stay signed assets. */",
         "#include <string.h>", "#include <ctype.h>", "#include <stdint.h>",
         f"const uint32_t port_asset_archive_size={(offset+1024+10239)//10240*10240}u;",
         "typedef struct { const char *name; uint32_t offset,bytes; } Entry;",
         "static const Entry files[]={"]
for name,position,size in sorted(files):
    lines.append(f"  {{{json.dumps(name,ensure_ascii=False)},{position}u,{size}u}},")
lines.extend(["};",
         "int port_asset_file(const char *path,uint32_t *offset,uint32_t *bytes) {",
         "  char name[256]; unsigned i=0;",
         "  for (;path[i] && i<sizeof(name)-1;++i) name[i]=tolower((unsigned char)path[i]);",
         "  name[i]=0; unsigned low=0,high=sizeof(files)/sizeof(files[0]);",
         "  while(low<high) { unsigned mid=(low+high)/2; int c=strcmp(name,files[mid].name);",
         "    if(!c) { *offset=files[mid].offset; *bytes=files[mid].bytes; return 1; }",
         "    if(c<0)high=mid;else low=mid+1; } return 0;", "}",
         "const char *port_asset_directory(const char *path) {",
         "  char name[256]; unsigned i=0;",
         "  for (;path[i] && i<sizeof(name)-1;++i) name[i]=tolower((unsigned char)path[i]);",
         "  name[i]=0;"])
for directory, entries in sorted(directories.items()):
    listing = "".join(("D" if is_dir else "F")+name+"\0"
                      for name, is_dir in sorted(entries.items()))
    # json \u0000 becomes a fixed-length C octal escape, safe before digits.
    literal = json.dumps(listing, ensure_ascii=False).replace("\\u0000", "\\000")
    lines.append(f"  if (!strcmp(name,{json.dumps(directory,ensure_ascii=False)})) return {literal};")
lines.extend(["  return 0;", "}"])
text="\n".join(lines)+"\n"
if not output.exists() or output.read_text()!=text:
    output.write_text(text)

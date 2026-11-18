#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise source encoder against the exact bounded C++ runtime decoder."""
from pathlib import Path
import argparse,random,subprocess,sys
parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
app=Path(__file__).resolve().parents[1];sys.path.insert(0,str(app/'tools'));from art_codec import pack,unpack
source=args.output/'codec-runner.cpp';source.write_text('#include "'+str(app/'packed_art.hpp')+'"\n#include <fstream>\n#include <vector>\n#include <iterator>\nint main(int argc,char**argv){std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t> a((std::istreambuf_iterator<char>(in)),{}),b(std::stoul(argv[3]));if(!garden::unpack(a,b))return 2;std::ofstream out(argv[2],std::ios::binary);out.write((char*)b.data(),b.size());}')
binary=args.output/'codec-runner';subprocess.run(['c++','-std=c++20','-O2','-fsanitize=address,undefined',str(source),'-o',str(binary)],check=True)
r=random.Random(9281)
for size in [0,1,2,3,4,127,128,129,256,4096,65535,65536,70000]:
 for data in [bytes([17])*size,bytes(i%251 for i in range(size)),r.randbytes(size)]:
  encoded=pack(data);assert unpack(encoded,size)==data
  (args.output/'input.bin').write_bytes(encoded);subprocess.run([str(binary),str(args.output/'input.bin'),str(args.output/'output.bin'),str(size)],check=True);assert (args.output/'output.bin').read_bytes()==data
print('39 cross-language round trips passed, including overlap runs and 16-bit window boundaries')

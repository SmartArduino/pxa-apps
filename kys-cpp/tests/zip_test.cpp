#include "ZipFile2.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <unistd.h>
int main(int argc,char** argv) {
    assert(argc==2 && chdir(argv[1])==0);
    std::string content(8192,'x');content+="金庸";
    {ZipFile2 zip;zip.create("probe.zip");zip.addData("save.dat",content.data(),content.size());}
    {ZipFile2 zip;zip.openRead("probe.zip");assert(zip.opened());assert(zip.readFile("save.dat")==content);}
    remove("probe.zip");puts("ZIP stdio roundtrip: PASS");
}

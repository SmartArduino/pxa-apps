#pragma once
#include <cstdio>
#include <ios>
#include <string>
// libc++ filebuf calls internal stdio operations on FILE. PXA's guest FILE
// handles are opaque slots, so ZIP IO must stay on the public wrapped API.
class KysZipInput {
    FILE* file_;
    bool good_;
public:
    KysZipInput(const std::string& path,std::ios_base::openmode mode):
        file_(fopen(path.c_str(),"rb")),good_(file_!=nullptr) {
        if(file_ && (mode&std::ios::ate))good_=fseek(file_,0,SEEK_END)==0;
    }
    ~KysZipInput(){if(file_)fclose(file_);}
    explicit operator bool()const{return good_;}
    long tellg(){return file_?ftell(file_):-1;}
    void seekg(long offset,std::ios_base::seekdir dir=std::ios::beg){
        good_=file_ && fseek(file_,offset,dir==std::ios::cur?SEEK_CUR:dir==std::ios::end?SEEK_END:SEEK_SET)==0;
    }
    void read(char* data,std::streamsize count){
        if(count<0 || !good_){good_=false;return;}
        if(count && fread(data,1,size_t(count),file_)!=size_t(count))good_=false;
    }
};
class KysZipOutput {
    FILE* file_;
    bool good_;
public:
    KysZipOutput(const std::string& path,std::ios_base::openmode):file_(fopen(path.c_str(),"wb")),good_(file_!=nullptr){}
    ~KysZipOutput(){if(file_)fclose(file_);}
    explicit operator bool()const{return good_;}
    void write(const char* data,std::streamsize count){
        if(count<0 || !good_){good_=false;return;}
        if(count && fwrite(data,1,size_t(count),file_)!=size_t(count))good_=false;
    }
};

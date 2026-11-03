#include "Cifa.h"
#include "ZipFile2.h"
#include "PotConv.h"
#include "sqlite3.h"
#include <cstdio>
#include <string>
extern "C" {
#include "port.h"
}
// Invoked explicitly from the missing-resource screen. Never opens a game save.
bool kys_backend_checks() {
    bool ok=true;
    if(FILE* probe=fopen("/data/kys-port-media-probe.txt","rb")) {
        char bytes[64]={0};
        bool media_ok=fseek(probe,4,SEEK_SET)==0 && fread(bytes,1,9,probe)==9
            && std::string(bytes,9)=="cpp media";
        media_ok=media_ok && fseek(probe,0,SEEK_SET)==0;
        memset(bytes,0,sizeof(bytes));size_t count=fread(bytes,1,sizeof(bytes),probe);
        media_ok=media_ok && std::string(bytes,count)=="kys-cpp media probe\n";
        fclose(probe);ok &= media_ok;
        port_log("KYS CHECK sd-read-at=%s",media_ok?"PASS":"FAIL");
    } else port_log("KYS CHECK sd-read-at=SKIP (probe not installed)");
    cifa::Cifa script;
    auto answer=script.run_script("int answer = 6 * 7; return answer;");
    bool cifa_ok=!script.has_error() && !script.has_runtime_error() && int(answer)==42;
    const char* script_path="/data/kys-port-check.c";
    if(FILE* file=fopen(script_path,"wb")) {
        const char source[]="return 6 * 7;";
        bool wrote=fwrite(source,1,sizeof(source)-1,file)==sizeof(source)-1;
        fclose(file);
        cifa::Cifa from_file;
        auto result=from_file.run_file(script_path);
        cifa_ok=cifa_ok && wrote && !from_file.has_error() && !from_file.has_runtime_error() && int(result)==42;
    } else cifa_ok=false;
    remove(script_path);
    port_log("KYS CHECK cifa=%s",cifa_ok?"PASS":"FAIL");ok &= cifa_ok;
    const char* dbpath="/data/kys-port-check.db";
    remove(dbpath);
    sqlite3* db=nullptr;
    int rc=sqlite3_open(dbpath,&db);
    if(rc==SQLITE_OK)rc=sqlite3_exec(db,"CREATE TABLE probe (n INTEGER, name TEXT); INSERT INTO probe VALUES(42,'金庸');",nullptr,nullptr,nullptr);
    if(db)sqlite3_close(db);db=nullptr;
    if(rc==SQLITE_OK)rc=sqlite3_open(dbpath,&db);
    sqlite3_stmt* stmt=nullptr;
    if(rc==SQLITE_OK)rc=sqlite3_prepare_v2(db,"SELECT n,name FROM probe",-1,&stmt,nullptr);
    bool sql_ok=rc==SQLITE_OK && sqlite3_step(stmt)==SQLITE_ROW && sqlite3_column_int(stmt,0)==42
        && std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt,1)))=="金庸";
    if(stmt)sqlite3_finalize(stmt);if(db)sqlite3_close(db);remove(dbpath);
    port_log("KYS CHECK sqlite-persist-utf8=%s rc=%d",sql_ok?"PASS":"FAIL",rc);ok &= sql_ok;
    const char* zipath="/data/kys-port-check.zip";
    std::string payload(1024,'x');payload+="金庸";
    {ZipFile2 zip;zip.create(zipath);zip.addData("probe.txt",payload.data(),payload.size());}
    bool zip_ok;
    {ZipFile2 zip;zip.openRead(zipath);zip_ok=zip.opened() && zip.readFile("probe.txt")==payload;}
    remove(zipath);port_log("KYS CHECK zip-roundtrip=%s",zip_ok?"PASS":"FAIL");ok &= zip_ok;
    bool encoding_ok=PotConv::cp936toutf8(std::string("\xbd\xf0\xd3\xb9",4))=="金庸"
        && PotConv::cp950toutf8(std::string("\xaa\xf7\xb1\x65",4))=="金庸";
    port_log("KYS CHECK gbk-big5=%s",encoding_ok?"PASS":"FAIL");ok &= encoding_ok;
    port_log("KYS CHECK result=%s (platform checks; not gameplay)",ok?"PASS":"FAIL");
    return ok;
}

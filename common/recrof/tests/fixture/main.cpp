#include "file.h"
#include "port.h"
#include "rawfile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
struct File {
  std::string name;
  File()
      : name("a long local string that allocates and survives the file "
             "request") {}
  ~File() {
    if (name !=
        "a long local string that allocates and survives the file request")
      port_error("CORRUPTED DESTRUCTOR");
  }
};
extern "C" void port_game_main() {
  for (int i = 0; i < 30; i++) {
    std::string base = "/data";
    bool ok = RawFileClass((base + "/" + "fixture.txt").c_str()).Is_Available();
    if (base != "/data")
      port_error("CORRUPTED BASE");
    (void)ok;
    RawFileClass probe("/data/fixture.txt");
    probe.Is_Available();
    if (!probe.File_Name() || strcmp(probe.File_Name(), "/data/fixture.txt"))
      port_error("CORRUPTED VIRTUAL FILE");
    char name[128] = "/data/fixture.txt";
    Resolve_File(name);
    File object;
    char sentinel[96];
    memset(sentinel, 0x5a, sizeof(sentinel));
    struct stat st;
    stat("/data/fixture.txt", &st);
    if (object.name !=
        "a long local string that allocates and survives the file request")
      port_error("CORRUPTED STRING");
    for (unsigned j = 0; j < sizeof(sentinel); j++)
      if (sentinel[j] != 0x5a)
        port_error("CORRUPTED STACK");
    FILE *f = fopen("/data/fixture.txt", "wb");
    if (!f)
      port_error("OPEN FAILED");
    fputs("fixture sentinel", f);
    fclose(f);
    f = fopen("/data/fixture.txt", "rb");
    if (!f)
      port_error("READ OPEN FAILED");
    char s[32] = {0};
    fread(s, 1, 16, f);
    fclose(f);
    if (strcmp(s, "fixture sentinel"))
      port_error("CONTENT FAILED");
  }
  for(int i=0;i<2;++i) {
    File object;
    FILE *f=fopen("/data/pending", "wb");
    if(!f || fwrite("published",1,9,f)!=9 || fclose(f) ||
        port_replace("/data/pending","/data/committed"))
      port_error("PUBLICATION FAILED");
    f=fopen("/data/committed","rb");char text[10]={0};
    if(!f || fread(text,1,9,f)!=9 || fclose(f) || strcmp(text,"published"))
      port_error("PUBLISHED CONTENT FAILED");
  }
  port_error("FIXTURE PASS");
}
extern "C" void port_game_key(int, int) {}
extern "C" void port_game_motion(int, int) {}
extern "C" void port_game_audio() {}

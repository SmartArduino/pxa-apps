#include "sqlite3.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int main(int argc,char** argv) {
    assert(argc==2 && chdir(argv[1])==0);
    sqlite3 *db=0;sqlite3_stmt *stmt=0;
    assert(sqlite3_open("probe.db",&db)==SQLITE_OK);
    assert(sqlite3_exec(db,"CREATE TABLE save(id INTEGER PRIMARY KEY,name TEXT); INSERT INTO save VALUES(1,'金庸');",0,0,0)==SQLITE_OK);
    assert(sqlite3_exec(db,"BEGIN; UPDATE save SET name='坏数据'; ROLLBACK;",0,0,0)==SQLITE_OK);
    assert(sqlite3_close(db)==SQLITE_OK);
    assert(sqlite3_open("probe.db",&db)==SQLITE_OK);
    assert(sqlite3_prepare_v2(db,"SELECT name FROM save WHERE id=1",-1,&stmt,0)==SQLITE_OK);
    assert(sqlite3_step(stmt)==SQLITE_ROW);
    assert(strcmp((const char*)sqlite3_column_text(stmt,0),"金庸")==0);
    sqlite3_finalize(stmt);
    assert(sqlite3_exec(db,"PRAGMA integrity_check",0,0,0)==SQLITE_OK);
    sqlite3_close(db);remove("probe.db");
    puts("SQLite VFS create, persist, UTF-8 and rollback: PASS");
}

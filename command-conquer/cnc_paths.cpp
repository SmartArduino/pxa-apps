/**
 * @file cnc_paths.cpp
 * @brief PathsClass for the ESP32-S3 port: the game data, the configuration and
 * the savegames are all in CNC_DATA_DIR on the SD card (replaces
 * paths_posix.cpp).
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 */
#include "board_config.h"
#include "paths.h"
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

const char *PathsClass::Program_Path() {
  if (ProgramPath.empty()) {
    ProgramPath = CNC_DATA_DIR;
  }
  return ProgramPath.c_str();
}

const char *PathsClass::Data_Path() {
  if (DataPath.empty()) {
    DataPath = CNC_DATA_DIR;
  }
  return DataPath.c_str();
}

const char *PathsClass::User_Path() {
  if (UserPath.empty()) {
    UserPath = CNC_DATA_DIR;
  }
  return UserPath.c_str();
}

bool PathsClass::Create_Directory(const char *dirname) {
  if (dirname == nullptr) {
    return true;
  }
  std::string temp = dirname;
  size_t pos = 0;
  do {
    pos = temp.find_first_of("/", pos + 1);
    if (mkdir(temp.substr(0, pos).c_str(), 0700) != 0 && errno != EEXIST) {
      return false;
    }
  } while (pos != std::string::npos);
  return true;
}

bool PathsClass::Is_Absolute(const char *path) {
  return path != nullptr && path[0] == '/';
}

std::string PathsClass::Concatenate_Paths(const char *path1,
                                          const char *path2) {
  return std::string(path1) + SEP + path2;
}

std::string PathsClass::Get_Filename(const char *path) {
  const char *name = strrchr(path, '/');
  return std::string(name ? name + 1 : path);
}

std::string PathsClass::Argv_Path(const char *cmd_arg) {
  std::string ret = cmd_arg;
  size_t sep = ret.find_last_of('/');
  return sep == std::string::npos ? std::string(CNC_DATA_DIR)
                                  : ret.substr(0, sep);
}

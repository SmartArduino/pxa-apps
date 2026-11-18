// SPDX-License-Identifier: MIT
#include "save.hpp"
#include <cstdio>
using namespace iron;
int main(){std::printf("{\"size\":%zu,\"world\":%zu,\"entities\":%zu,\"extra\":%zu,\"mission\":%zu,\"time\":%zu,\"side\":%zu}\n",sizeof(Save),offsetof(Save,world),offsetof(Save,world)+offsetof(World,e),offsetof(Save,world)+offsetof(World,extra),offsetof(Save,world)+offsetof(World,mission),offsetof(Save,world)+offsetof(World,time),offsetof(Save,world)+offsetof(World,side));}

#pragma once

// #define DEBUG

#ifdef DEBUG
  #define DO_DEBUG(code) code
#else
  #define DO_DEBUG(code) do {} while(0)
#endif
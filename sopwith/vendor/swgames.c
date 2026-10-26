//
// Copyright(C) 1984-2000 David L. Clark
// Copyright(C) 2001-2005 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//
//
//        swgames  -      SW definition of games
//

#include <assert.h>
#include <errno.h>
#include <string.h>

#include "sw.h"
#include "swgames.h"
#include "swsound.h"
#include "swtitle.h"
#include "video.h"
#include "yocton.h"

GRNDTYPE original_ground[] =
    {/* Original ground height by pixel */

     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 198, 197, 196, 195, 194, 193, 192, 191, 190, 189, 188, 187,
     186, 185, 184, 183, 182, 181, 180, 179, 178, 177, 176, 175, 174, 173, 172,
     171, 170, 169, 168, 167, 166, 165, 164, 163, 162, 161, 160, 159, 158, 157,
     156, 155, 154, 153, 152, 151, 150, 149, 148, 147, 146, 145, 144, 143, 142,
     141, 140, 139, 138, 137, 136, 135, 134, 133, 132, 131, 130, 129, 128, 127,
     126, 125, 124, 123, 122, 121, 120, 119, 118, 117, 116, 115, 114, 113, 112,
     111, 110, 109, 108, 107, 106, 105, 104, 103, 102, 101, 100, 99,  98,  97,
     96,  95,  94,  93,  92,  91,  90,  89,  88,  87,  86,  85,  84,  83,  82,
     81,  80,  79,  78,  77,  76,  75,  74,  73,  72,  71,  70,  69,  68,  67,
     66,  65,  64,  63,  62,  61,  60,  59,  58,  57,  56,  55,  54,  53,  52,
     51,  50,  49,  48,  47,  46,  45,  44,  43,  42,  41,  40,  39,  38,  37,
     36,  35,  34,  33,  32,  31,  30,  29,  28,  27,  26,  26,  26,  26,  26,
     26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,
     26,  26,  26,  26,  26,  26,  26,  27,  27,  26,  26,  26,  26,  26,  26,
     26,  26,  27,  27,  28,  28,  29,  29,  30,  30,  31,  31,  32,  32,  32,
     32,  32,  32,  32,  32,  32,  32,  31,  31,  30,  29,  28,  28,  27,  26,
     26,  26,  26,  27,  27,  28,  28,  28,  28,  29,  29,  29,  29,  29,  28,
     28,  28,  28,  27,  27,  27,  27,  26,  26,  26,  26,  26,  26,  26,  26,
     26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,
     26,  26,  26,  26,  26,  27,  27,  28,  28,  29,  29,  30,  30,  31,  31,
     32,  32,  33,  34,  35,  35,  36,  37,  38,  38,  39,  39,  40,  40,  41,
     41,  42,  42,  43,  44,  44,  45,  46,  47,  48,  49,  50,  51,  52,  54,
     55,  56,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,
     71,  73,  74,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,
     75,  75,  75,  75,  75,  75,  75,  75,  75,  74,  74,  74,  74,  73,  73,
     72,  72,  71,  71,  70,  69,  69,  68,  67,  66,  64,  63,  62,  60,  58,
     57,  55,  53,  51,  50,  48,  46,  44,  43,  41,  39,  37,  36,  34,  32,
     30,  29,  27,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,
     26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,
     26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,
     26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,
     26,  26,  26,  26,  26,  26,  27,  27,  27,  27,  28,  28,  29,  29,  30,
     30,  31,  32,  33,  34,  35,  36,  36,  37,  38,  39,  40,  41,  42,  42,
     43,  44,  44,  45,  45,  46,  46,  47,  47,  48,  49,  50,  51,  52,  53,
     54,  55,  56,  56,  57,  58,  59,  59,  60,  61,  62,  62,  63,  63,  64,
     65,  65,  66,  66,  67,  68,  69,  69,  70,  71,  72,  72,  73,  74,  75,
     75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  74,  74,  74,
     74,  74,  74,  75,  75,  74,  74,  74,  74,  73,  73,  73,  73,  72,  72,
     71,  71,  71,  71,  70,  70,  69,  69,  68,  68,  67,  67,  66,  66,  66,
     66,  65,  65,  65,  65,  64,  64,  63,  63,  62,  62,  61,  61,  60,  60,
     59,  59,  59,  58,  58,  58,  58,  58,  58,  58,  58,  58,  59,  59,  59,
     59,  60,  60,  61,  61,  61,  61,  62,  62,  62,  62,  63,  63,  63,  63,
     62,  62,  62,  62,  61,  61,  61,  61,  62,  62,  62,  62,  63,  63,  63,
     63,  63,  63,  62,  62,  62,  62,  61,  61,  60,  60,  59,  59,  58,  58,
     57,  57,  56,  55,  55,  54,  53,  53,  52,  52,  51,  51,  50,  50,  49,
     49,  48,  48,  47,  47,  46,  46,  45,  45,  44,  44,  43,  43,  42,  42,
     42,  42,  42,  42,  41,  41,  41,  41,  41,  41,  41,  41,  41,  41,  41,
     41,  42,  42,  42,  43,  43,  44,  44,  45,  45,  46,  47,  47,  48,  49,
     50,  50,  51,  52,  53,  53,  54,  55,  56,  56,  57,  58,  59,  59,  60,
     61,  62,  62,  63,  64,  64,  65,  65,  66,  67,  68,  69,  70,  71,  72,
     73,  74,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,
     75,  75,  75,  75,  75,  75,  75,  75,  74,  74,  73,  73,  73,  72,  72,
     72,  72,  72,  72,  72,  71,  71,  71,  71,  70,  70,  69,  69,  68,  68,
     67,  67,  66,  66,  65,  65,  64,  64,  63,  62,  61,  60,  59,  58,  57,
     56,  54,  53,  52,  51,  49,  48,  47,  46,  45,  44,  44,  44,  44,  44,
     43,  43,  43,  43,  42,  42,  42,  42,  42,  41,  41,  41,  41,  41,  41,
     40,  40,  40,  40,  39,  39,  39,  39,  39,  38,  38,  38,  38,  38,  37,
     37,  37,  37,  37,  36,  36,  36,  36,  36,  35,  35,  35,  35,  34,  34,
     34,  34,  33,  33,  33,  33,  32,  32,  32,  32,  31,  31,  31,  30,  30,
     30,  29,  29,  29,  28,  28,  27,  27,  27,  26,  26,  26,  26,  26,  26,
     26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,
     26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,
     26,  26,  26,  27,  27,  27,  27,  27,  27,  27,  27,  27,  28,  28,  28,
     28,  29,  29,  29,  29,  30,  30,  30,  30,  31,  31,  31,  32,  32,  32,
     33,  33,  34,  34,  34,  35,  35,  36,  36,  37,  37,  38,  38,  39,  39,
     40,  40,  41,  41,  42,  42,  43,  43,  43,  44,  44,  45,  45,  46,  46,
     46,  47,  47,  47,  48,  48,  49,  49,  49,  50,  50,  50,  51,  51,  52,
     52,  53,  53,  53,  54,  54,  55,  55,  56,  56,  57,  57,  58,  58,  59,
     59,  60,  60,  60,  61,  61,  62,  62,  63,  63,  64,  64,  64,  64,  64,
     64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,
     64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,
     64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,
     64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,
     64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,
     64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,
     64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,
     64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,
     64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,
     64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,
     64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,
     64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  63,  63,  62,  62,
     61,  61,  61,  61,  60,  60,  60,  60,  60,  60,  59,  59,  59,  59,  59,
     59,  59,  59,  58,  58,  58,  58,  58,  57,  57,  57,  57,  56,  56,  56,
     56,  56,  56,  55,  55,  55,  55,  55,  55,  54,  54,  54,  54,  54,  54,
     53,  53,  53,  53,  53,  53,  52,  52,  52,  52,  52,  52,  52,  52,  51,
     51,  51,  51,  51,  51,  51,  51,  50,  50,  50,  50,  50,  50,  50,  50,
     50,  50,  50,  50,  50,  50,  51,  51,  51,  51,  51,  51,  51,  52,  52,
     52,  52,  53,  53,  53,  53,  54,  54,  54,  54,  55,  55,  56,  56,  56,
     57,  57,  58,  58,  59,  59,  59,  60,  60,  60,  61,  61,  61,  62,  62,
     62,  63,  63,  63,  64,  64,  64,  64,  65,  65,  65,  65,  66,  66,  66,
     66,  67,  67,  67,  67,  67,  68,  68,  68,  68,  68,  68,  68,  69,  69,
     69,  69,  69,  69,  69,  69,  70,  70,  70,  70,  71,  71,  71,  71,  72,
     72,  72,  72,  73,  73,  73,  73,  74,  74,  74,  74,  75,  75,  75,  74,
     74,  74,  74,  74,  74,  74,  74,  74,  74,  74,  74,  74,  74,  74,  74,
     74,  74,  74,  74,  74,  74,  74,  74,  74,  74,  74,  74,  74,  74,  74,
     75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,
     75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,
     75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,
     75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,
     75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,
     75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,
     75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,
     75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,
     75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,
     75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,
     75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,
     75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,
     75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  74,  73,  72,
     71,  70,  69,  68,  67,  66,  65,  64,  63,  62,  61,  60,  59,  58,  57,
     56,  55,  54,  54,  54,  54,  53,  53,  53,  53,  53,  53,  53,  53,  52,
     52,  52,  52,  51,  51,  51,  51,  50,  50,  50,  50,  50,  49,  49,  49,
     49,  49,  49,  49,  48,  48,  48,  48,  48,  48,  48,  48,  48,  48,  47,
     47,  47,  47,  47,  47,  47,  47,  46,  46,  46,  46,  46,  46,  46,  46,
     46,  46,  46,  46,  46,  47,  47,  46,  46,  46,  46,  46,  46,  46,  46,
     46,  46,  46,  46,  45,  45,  45,  45,  45,  45,  45,  44,  44,  44,  44,
     44,  44,  44,  43,  43,  43,  43,  43,  43,  42,  42,  42,  42,  42,  42,
     41,  41,  41,  41,  41,  40,  40,  40,  39,  39,  39,  38,  38,  38,  38,
     37,  37,  37,  36,  36,  35,  35,  35,  34,  34,  34,  33,  33,  32,  32,
     31,  31,  31,  30,  30,  29,  29,  28,  28,  28,  27,  27,  26,  26,  26,
     26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,
     26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,
     26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,
     26,  26,  26,  26,  27,  27,  27,  27,  28,  28,  28,  29,  29,  30,  30,
     31,  31,  32,  32,  33,  33,  34,  34,  34,  35,  35,  36,  36,  37,  37,
     38,  39,  40,  40,  41,  42,  43,  43,  44,  45,  46,  47,  48,  49,  50,
     51,  52,  53,  54,  55,  56,  57,  57,  58,  58,  59,  59,  60,  60,  60,
     60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,
     60,  60,  60,  59,  59,  59,  59,  58,  58,  58,  58,  57,  57,  57,  57,
     56,  56,  56,  56,  55,  55,  55,  55,  54,  54,  54,  54,  54,  55,  55,
     56,  57,  58,  58,  59,  60,  61,  61,  62,  63,  64,  65,  66,  67,  68,
     69,  70,  71,  72,  73,  74,  75,  75,  75,  75,  75,  75,  75,  75,  74,
     74,  74,  74,  73,  73,  73,  73,  72,  72,  72,  71,  71,  70,  70,  69,
     68,  67,  67,  66,  65,  64,  63,  62,  61,  60,  59,  58,  57,  56,  54,
     53,  52,  51,  49,  48,  47,  46,  44,  43,  41,  40,  38,  37,  35,  34,
     32,  31,  29,  28,  26,  26,  26,  26,  26,  26,  27,  27,  27,  28,  28,
     29,  29,  29,  30,  30,  30,  30,  30,  30,  30,  30,  30,  31,  31,  31,
     31,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,
     32,  32,  32,  32,  32,  32,  31,  31,  31,  30,  30,  30,  30,  29,  29,
     29,  28,  28,  27,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,
     27,  27,  27,  27,  28,  28,  28,  29,  29,  30,  30,  30,  30,  30,  30,
     30,  30,  30,  31,  31,  32,  32,  32,  33,  33,  33,  33,  34,  34,  34,
     34,  35,  35,  36,  36,  37,  37,  38,  39,  39,  40,  41,  41,  42,  42,
     43,  43,  43,  44,  44,  44,  45,  45,  46,  46,  47,  47,  47,  47,  47,
     48,  48,  49,  49,  50,  50,  51,  51,  52,  52,  53,  53,  54,  55,  56,
     57,  57,  58,  59,  59,  59,  59,  59,  59,  59,  59,  59,  59,  59,  59,
     59,  59,  58,  58,  57,  57,  56,  56,  55,  55,  54,  54,  53,  53,  52,
     52,  51,  51,  51,  50,  50,  50,  49,  49,  48,  48,  47,  47,  46,  46,
     45,  44,  43,  42,  41,  40,  39,  38,  37,  36,  35,  34,  34,  34,  33,
     33,  33,  32,  32,  31,  31,  31,  30,  30,  30,  29,  29,  28,  28,  27,
     27,  27,  27,  28,  28,  28,  28,  28,  28,  28,  28,  28,  28,  28,  28,
     28,  27,  27,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,
     26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,
     26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,
     27,  27,  27,  27,  28,  28,  28,  29,  29,  30,  30,  31,  31,  31,  31,
     32,  32,  32,  31,  31,  31,  31,  31,  31,  31,  31,  30,  30,  30,  30,
     29,  29,  28,  28,  28,  28,  27,  27,  27,  27,  26,  26,  26,  26,  26,
     26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  27,  27,  27,  27,
     27,  27,  27,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,
     27,  27,  28,  28,  29,  29,  29,  29,  29,  28,  28,  28,  28,  27,  27,
     27,  27,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,
     26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  27,  27,  27,
     27,  27,  27,  27,  27,  27,  27,  27,  27,  27,  27,  27,  27,  28,  28,
     28,  28,  29,  29,  29,  29,  29,  29,  29,  30,  31,  32,  33,  34,  35,
     36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,
     51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,  65,
     66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,
     81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,
     96,  97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110,
     111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125,
     126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140,
     141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155,
     156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170,
     171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185,
     186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
     199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199};

static original_ob_t original_targets[] = {
    // Left side of map -
    {TARGET, 191, 1, 0, 0, FACTION_PLAYER3},
    {TARGET, 284, 3, 0, 0, FACTION_PLAYER3},
    {FLOCK, 370, 0, 370, 2630, FACTION_NONE},
    {TARGET, 409, 1, 0, 0, FACTION_PLAYER3},
    {TARGET, 539, 1, 0, 0, FACTION_PLAYER3},
    {PLANE, 588, 0, 0, 1154, FACTION_PLAYER3},
    {TARGET, 685, 3, 0, 0, FACTION_PLAYER3, TRANSFORM_MIRROR},
    {TARGET, 807, 0, 0, 0, FACTION_PLAYER3},
    {TARGET, 934, 1, 0, 0, FACTION_PLAYER3},

    // Player 1 base
    {FLOCK, 1000, 0, 370, 2630, FACTION_NONE},
    {TARGET, 1210, 2, 0, 0, FACTION_PLAYER1},
    {TARGET, 1240, 0, 0, 0, FACTION_PLAYER1},
    {PLANE, 1270, 0, 901, 1835, FACTION_PLAYER1},
    // Extra planes for multiplayer
    {PLANE, 1330, 0, 901, 1835, FACTION_PLAYER5},
    {PLANE, 1360, 0, 901, 1835, FACTION_PLAYER6},
    {OX, 1376, 0, 0, 0, FACTION_NONE},
    {TARGET, 1440, 3, 0, 0, FACTION_PLAYER1},

    // Player 2 base
    {TARGET, 1550, 3, 0, 0, FACTION_PLAYER2, TRANSFORM_MIRROR},
    {OX, 1608, 0, 0, 0, FACTION_NONE},
    {FLOCK, 1630, 0, 370, 2630, FACTION_NONE},
    // Extra planes for multiplayer
    {PLANE, 1630, 1, 1155, 2089, FACTION_PLAYER7},
    {PLANE, 1660, 1, 1155, 2089, FACTION_PLAYER8},
    {PLANE, 1720, 1, 1155, 2089, FACTION_PLAYER2},
    {TARGET, 1750, 0, 0, 0, FACTION_PLAYER2},
    {TARGET, 1780, 2, 0, 0, FACTION_PLAYER2},
    {TARGET, 2024, 1, 0, 0, FACTION_PLAYER2},

    // Right side of map -
    {TARGET, 2159, 1, 0, 0, FACTION_PLAYER4},
    {TARGET, 2279, 3, 0, 0, FACTION_PLAYER4, TRANSFORM_MIRROR},
    {TARGET, 2390, 3, 0, 0, FACTION_PLAYER4},
    {PLANE, 2456, 1, 2089, 9999, FACTION_PLAYER4},
    {TARGET, 2549, 0, 0, 0, FACTION_PLAYER4},
    {FLOCK, 2630, 0, 370, 2630, FACTION_NONE},
    {TARGET, 2678, 0, 0, 0, FACTION_PLAYER4},
    {TARGET, 2763, 1, 0, 0, FACTION_PLAYER4},
};

const GAMES original_level = {
    7491,
    original_targets,
    arrlen(original_targets),
    original_ground,
    arrlen(original_ground),
};

static const char *obtype_names[] = {
    "GROUND",    "PLANE",     "BOMB",    "SHOT",    "TARGET",
    "EXPLOSION", "SMOKE",     "FLOCK",   "BIRD",    "OX",
    "MISSILE",   "STARBURST", "BALLOON", "POWERUP", NULL,
};

static const char *transform_names[] = {
    "NONE",
    "ROTATE90",
    "ROTATE180",
    "ROTATE270",
    "FLIP",
    "FLIP_ROTATE90",
    "MIRROR",
    "MIRROR_ROTATE90",
    YOCTON_ENUM_TRY_INDEX,
};

static const char *faction_names[] = {
    "NONE",    "PLAYER1", "PLAYER2", "PLAYER3", "PLAYER4",
    "PLAYER5", "PLAYER6", "PLAYER7", "PLAYER8", NULL,
};

GAMES custom_level;
bool have_custom_level;

#define cl custom_level

static void FreeCustomLevel(void)
{
	free(cl.gm_objects);
	cl.gm_objects = NULL;
	cl.gm_num_objects = 0;
	free(cl.gm_ground);
	cl.gm_ground = NULL;
	cl.gm_max_x = 0;
	cl.gm_rseed = 12345;
}

static void AddObject(original_ob_t *ob, struct yocton_object *yo)
{
	struct yocton_prop *p;

	memset(ob, 0, sizeof(*ob));

	while ((p = yocton_next_prop(yo)) != NULL) {
		YOCTON_VAR_INT(p, "x", int, ob->x);
		YOCTON_VAR_INT(p, "orient", int, ob->orient);
		YOCTON_VAR_INT(p, "territory_l", int, ob->territory_l);
		YOCTON_VAR_INT(p, "territory_r", int, ob->territory_r);
		YOCTON_VAR_ENUM(p, "transform", ob->transform, transform_names);
		YOCTON_VAR_ENUM(p, "type", ob->type, obtype_names);
		YOCTON_VAR_ENUM(p, "faction", ob->faction, faction_names);

		// Old name for faction field.
		YOCTON_VAR_ENUM(p, "owner", ob->faction, faction_names);
	}
}

static void SetGround(struct yocton_object *yo, GRNDTYPE **ground,
                      unsigned int *ground_len)
{
	struct yocton_prop *p;

	while ((p = yocton_next_prop(yo)) != NULL) {
		const char *name = yocton_prop_name(p);
		yocton_check(yo, !strcmp(name, "_"),
		             "expected prop name '_', got '%s'", name);
		YOCTON_VAR_INT_ARRAY(p, "_", GRNDTYPE, *ground, *ground_len);
	}
}

static void ProcessLevel(struct yocton_object *obj)
{
	struct yocton_prop *p;

	while ((p = yocton_next_prop(obj)) != NULL) {
		YOCTON_VAR_ARRAY(
		    p, "object", cl.gm_objects, cl.gm_num_objects, {
			    AddObject(&cl.gm_objects[cl.gm_num_objects],
			              yocton_prop_inner(p));
			    ++cl.gm_num_objects;
		    });
		YOCTON_IF_PROP(p, "ground", {
			SetGround(yocton_prop_inner(p), &cl.gm_ground,
			          &cl.gm_max_x);
		});
	}
}

static void ProcessSymbol(const char *name, struct yocton_object *obj)
{
	symset_t *s;
	struct yocton_prop *p;
	char *ptr;
	const char *value;
	unsigned long l;

	while ((p = yocton_next_prop(obj)) != NULL) {
		const char *pn = yocton_prop_name(p);
		errno = 0;
		l = strtoul(pn, &ptr, 10);
		yocton_check(obj, *ptr == '\0' && errno != ERANGE && l < 256,
		             "expecting frame number as property, got '%s'",
		             pn);

		s = LookupSymset(name, l);
		yocton_check(obj, s != NULL,
		             "failed to look up symbol '%s' frame %d", name,
		             (int) l);
		value = yocton_prop_value(p);
		if (s != NULL && value != NULL) {
			SymsetFromText(s, value);
		}
	}
}

static void ProcessSounds(struct yocton_object *obj)
{
	struct yocton_prop *p;
	char *title_tune = NULL;

	while ((p = yocton_next_prop(obj)) != NULL) {
		YOCTON_VAR_STRING(p, "title_tune", title_tune);
	}

	if (title_tune != NULL) {
		expltune = title_tune;
	}
}

static void ProcessSymbols(struct yocton_object *obj)
{
	struct yocton_prop *p;

	while ((p = yocton_next_prop(obj)) != NULL) {
		ProcessSymbol(yocton_prop_name(p), yocton_prop_inner(p));
	}
}

static const char *text_align_names[] = {
    "LEFT",
    "CENTER",
    "RIGHT",
    NULL,
};

static void ProcessTitleText(struct yocton_object *obj)
{
	struct yocton_prop *p;
	int x = 0, y = 0, color = 3;
	char *text = NULL;
	enum text_align align = TEXT_ALIGN_LEFT;

	while ((p = yocton_next_prop(obj)) != NULL) {
		YOCTON_VAR_STRING(p, "text", text);
		YOCTON_VAR_INT(p, "x", int, x);
		YOCTON_VAR_INT(p, "y", int, y);
		YOCTON_VAR_INT(p, "color", int, color);
		YOCTON_VAR_ENUM(p, "align", align, text_align_names);
	}

	if (text != NULL) {
		AddTitleText(text, x, y, color, align);
	}
}

static void ProcessTitleSymbol(struct yocton_object *obj)
{
	struct yocton_prop *p;
	char *symname = NULL;
	int x = 0, y = 0;
	int frame = 0, transform = 0;
	faction_t faction = FACTION_PLAYER1;
	symset_t *symset;

	while ((p = yocton_next_prop(obj)) != NULL) {
		YOCTON_VAR_STRING(p, "name", symname);
		YOCTON_VAR_INT(p, "x", int, x);
		YOCTON_VAR_INT(p, "y", int, y);
		YOCTON_VAR_INT(p, "frame", int, frame);
		YOCTON_VAR_ENUM(p, "transform", transform, transform_names);
		YOCTON_VAR_ENUM(p, "faction", faction, faction_names);
	}

	yocton_check(obj, symname != NULL,
	             "symbol block must specify name property");
	yocton_check(obj, transform >= 0 && transform < 8,
	             "transform must be in range 0-7, got %d", transform);

	if (yocton_have_error(obj, NULL, NULL)) {
		return;
	}

	symset = LookupSymset(symname, frame);
	if (symset == NULL) {
		yocton_check(obj, 0, "failed to look up symbol '%s' frame %d",
		             symname, frame);
		return;
	}

	AddTitleSymbol(&symset->sym[transform], x, y, faction);
}

static void ProcessTitleGround(struct yocton_object *obj,
                               ground_render_t render)
{
	GRNDTYPE *ground = NULL;
	unsigned int ground_len = 0;

	SetGround(obj, &ground, &ground_len);
	AddTitleGround(ground, ground_len, render);
}

static void ProcessTitleLine(struct yocton_object *obj)
{
	struct yocton_prop *p;
	int x1 = -1, y1 = -1, x2 = -1, y2 = -1;
	int color = 3;

	while ((p = yocton_next_prop(obj)) != NULL) {
		YOCTON_VAR_INT(p, "x1", int, x1);
		YOCTON_VAR_INT(p, "y1", int, y1);
		YOCTON_VAR_INT(p, "x2", int, x2);
		YOCTON_VAR_INT(p, "y2", int, y2);
		YOCTON_VAR_INT(p, "color", int, color);
	}

	yocton_check(obj, x1 != -1 && x2 != -1 && y1 != -1 && y2 != -1,
	             "must specify x1, y1, x2 and y2 properties");

	if (!yocton_have_error(obj, NULL, NULL)) {
		AddTitleLine(x1, y1, x2, y2, color);
	}
}

static void ProcessTitle(struct yocton_object *obj)
{
	struct yocton_prop *p;

	ClearTitleScreen();

	while ((p = yocton_next_prop(obj)) != NULL) {
		YOCTON_IF_PROP(p, "text",
		               { ProcessTitleText(yocton_prop_inner(p)); });
		YOCTON_IF_PROP(p, "ground", {
			ProcessTitleGround(yocton_prop_inner(p),
			                   GROUND_RENDER_PREF);
		});
		YOCTON_IF_PROP(p, "ground_line", {
			ProcessTitleGround(yocton_prop_inner(p),
			                   GROUND_RENDER_LINE);
		});
		YOCTON_IF_PROP(p, "ground_solid", {
			ProcessTitleGround(yocton_prop_inner(p),
			                   GROUND_RENDER_SOLID);
		});
		YOCTON_IF_PROP(p, "symbol",
		               { ProcessTitleSymbol(yocton_prop_inner(p)); });
		YOCTON_IF_PROP(p, "line",
		               { ProcessTitleLine(yocton_prop_inner(p)); });
	}
}

void LoadCustomLevel(const char *filename)
{
	FILE *fs;
	struct yocton_object *obj;
	const char *error_msg;
	int lineno;
	struct yocton_prop *p;
	bool processed_level = false;

	FreeCustomLevel();

	fs = fopen(filename, "r");
	if (fs == NULL) {
		ErrorExit("Failed to open file: %s", filename);
	}
	obj = yocton_read_from(fs);
	assert(obj != NULL);

	while ((p = yocton_next_prop(obj)) != NULL) {
		// TODO: Add support for multiple levels within a mission file.
		// The level data gets embedded within a level {} object with
		// the expectation that files will be able to contain multiple
		// levels in the future.
		YOCTON_IF_PROP(p, "level", {
			yocton_check(obj, !processed_level,
			             "only one level per file supported");
			ProcessLevel(yocton_prop_inner(p));
			processed_level = true;
		});
		YOCTON_IF_PROP(p, "symbols",
		               { ProcessSymbols(yocton_prop_inner(p)); });
		YOCTON_IF_PROP(p, "sounds",
		               { ProcessSounds(yocton_prop_inner(p)); });
		YOCTON_IF_PROP(p, "title",
		               { ProcessTitle(yocton_prop_inner(p)); });
	}

	if (yocton_have_error(obj, &lineno, &error_msg)) {
		ErrorExit("Error in %s at line %d:\n%s", filename, lineno,
		          error_msg);
	}

	yocton_free(obj);
	have_custom_level = processed_level;
}

#undef cl

//
// 2003-02-14: Code was checked into version control; no further entries
// will be added to this log.
//
// sdh 14/2/2003: change license header to GPL
// sdh 21/10/2001: rearranged file headers, added cvs tags
// sdh 21/10/2001: reformatted with indent, adjusted some code by hand
//                 to make more readable
// sdh 20/10/2001: added parentheses to shut up compiler
//
// 87-03-09        Microsoft compiler.
// 84-02-08        Development
//

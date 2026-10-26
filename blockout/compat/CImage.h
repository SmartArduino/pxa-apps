/* SPDX-License-Identifier: GPL-2.0-or-later
 * Embedded official PNGs are decoded losslessly by prepare.py at build time.
 * The original sprite/font/texture loaders still see their expected BGR data.
 */
#ifndef PXA_BLOCKOUT_IMAGE_H
#define PXA_BLOCKOUT_IMAGE_H
typedef unsigned char BYTE;
class CImage {
  int index;
public:
  CImage():index(-1){}
  int LoadImage(char*);
  int Width(); int Height(); unsigned char *GetData();
  void Release(){index=-1;}
  char *GetErrorMessage(){return (char*)"embedded official image not found";}
};
#endif

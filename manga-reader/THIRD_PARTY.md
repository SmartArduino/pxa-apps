# Third-party notices

`vendor/picojpeg/picojpeg.c` and `picojpeg.h` are vendored from Rich Geldreich's [picojpeg](https://github.com/richgel999/picojpeg/tree/8ab33a909b4115ace4a952b7ffcb64c350f9298d), commit `8ab33a909b4115ace4a952b7ffcb64c350f9298d`. The source/header place this implementation in the public domain; their original notices are retained. It is used only inside the application, for bounded baseline JPEG fallback decoding. No library is added to PXA system/firmware/Host services.

MangaDex supplies metadata and images at runtime. No manga content is bundled. No source plugin code from dragonx943, Keiyoushi or CopyManga is copied.

Local portability changes replace left shifts of the negative literal -1 with equivalent defined arithmetic and sequence channel-pointer increments after their pixel assignments. Original notices remain intact.

`vendor/tiny-aes/` is tiny-AES-c by kokke and contributors, pinned to `23856752fbd139da0b8ca6e471a13d5bcc99a08d` ([source](https://github.com/kokke/tiny-AES-c/tree/23856752fbd139da0b8ca6e471a13d5bcc99a08d)). The files are unmodified; the Unlicense notice is retained. Only AES-128 CBC is compiled into the application to decode public CopyManga desktop image-list responses. It adds no dependency to the system or Host.

`vendor/stb/stb_image.h` is stb_image v2.32, pinned to nothings/stb commit `9600522de53973336fa033a289858a4354cbb04c`. The MIT option is selected; the original source and both upstream notices are retained. Only the JPEG decoder is compiled, without SIMD, HDR or stdio; it is used inside the app for progressive JPEG fallback with a 6 MiB allocation ceiling. No system/Host dependency is added.

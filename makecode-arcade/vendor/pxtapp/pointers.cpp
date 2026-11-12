#include "pxt.h"
#include "configkeys.h"
#include "pxtbase.h"
#include "pxt.h"
#include "platform.h"
#include "pxtcore.h"
#include "pins.h"
#include "platform_includes.h"
#include "vm.h"
#include "mixer---ext/SoundOutput.h"
#include "mixer---ext/melody.h"
namespace pxt {
    typedef TValueStruct *TValue;
    typedef TValue TNumber;
    typedef TValue Action;
    typedef TValue ImageLiteral;
    typedef TValue (*RunActionType)(Action a, TValue arg0, TValue arg1, TValue arg2);
    typedef void (*RefObjectMethod)(RefObject *self);
    typedef unsigned (*RefObjectSizeMethod)(RefObject *self);
    typedef void *PVoid;
    typedef void **PPVoid;
    typedef void *Object_;
    typedef TValue (*ActionCB)(TValue *captured, TValue arg0, TValue arg1, TValue arg2);
    typedef int color;
    typedef BoxedBuffer *Buffer;
    typedef BoxedString *String;
    typedef RefImage *Image_;
    void deleteRefObject(RefObject *obj);
}
namespace Array_ {
    bool isArray(TValue arr);
}
namespace pxtrt {
    RefCollection* keysOf(TValue v);
    TValue mapDeleteByString(RefMap *map, String key);
}
namespace pxt {
    Buffer getGCStats();
    void popThreadContext(ThreadContext *ctx);
    ThreadContext* pushThreadContext(void *sp, void *endSP);
    void seedRandom(unsigned seed);
    void seedAddRandom(unsigned seed);
}
namespace String_ {
    String mkEmpty();
    String fromCharCode(int code);
    TNumber charCodeAt(String s, int pos);
    String charAt(String s, int pos);
    String concat(String s, String other);
    int length(String s);
    TNumber toNumber(String s);
    String substr(String s, int start, int length);
    int indexOf(String s, String searchString, int start);
    int includes(String s, String searchString, int start);
}
namespace Boolean_ {
    bool bang(bool v);
}
namespace pxt {
    bool switch_eq(TValue a, TValue b);
}
namespace numops {
    TNumber adds(TNumber a, TNumber b);
    TNumber subs(TNumber a, TNumber b);
    TNumber muls(TNumber a, TNumber b);
    TNumber div(TNumber a, TNumber b);
    TNumber mod(TNumber a, TNumber b);
    TNumber lsls(TNumber a, TNumber b);
    TNumber lsrs(TNumber a, TNumber b);
    TNumber asrs(TNumber a, TNumber b);
    TNumber eors(TNumber a, TNumber b);
    TNumber orrs(TNumber a, TNumber b);
    TNumber bnot(TNumber a);
    TNumber ands(TNumber a, TNumber b);
    bool lt_bool(TNumber a, TNumber b);
    TNumber le(TNumber a, TNumber b);
    TNumber lt(TNumber a, TNumber b);
    TNumber ge(TNumber a, TNumber b);
    TNumber gt(TNumber a, TNumber b);
    TNumber eq(TNumber a, TNumber b);
    TNumber neq(TNumber a, TNumber b);
    TNumber eqq(TNumber a, TNumber b);
    TNumber neqq(TNumber a, TNumber b);
}
namespace Math_ {
    TNumber pow(TNumber x, TNumber y);
    TNumber random();
    TNumber randomRange(TNumber min, TNumber max);
    TNumber log(TNumber x);
    TNumber log10(TNumber x);
    TNumber floor(TNumber x);
    TNumber ceil(TNumber x);
    TNumber trunc(TNumber x);
    TNumber round(TNumber x);
    int imul(int x, int y);
    int idiv(int x, int y);
}
namespace pxt {
    void* ptrOfLiteral(int offset);
    unsigned programSize();
    void deepSleep();
    int getConfig(int key, int defl);
}
namespace pxtrt {
    TValue ldlocRef(RefRefLocal *r);
    void stlocRef(RefRefLocal *r, TValue v);
    RefRefLocal* mklocRef();
    RefAction* stclo(RefAction *a, int idx, TValue v);
    void panic(int code);
    String emptyToNull(String s);
    int ptrToBool(TValue p);
    void* getGlobalsPtr();
    void runtimeWarning(String s);
}
namespace pxt {
    ValType valType(TValue v);
    String typeOf(TValue v);
    typedef void (*RestoreStateType)(TryFrame *, ThreadContext *);
    TryFrame* beginTry();
    void endTry();
    void throwValue(TValue v);
    TValue getThrownValue();
    void endFinally();
}
namespace Math_ {
    TNumber log2(TNumber x);
    TNumber exp(TNumber x);
    TNumber tanh(TNumber x);
    TNumber sinh(TNumber x);
    TNumber cosh(TNumber x);
    TNumber atanh(TNumber x);
    TNumber asinh(TNumber x);
    TNumber acosh(TNumber x);
    TNumber atan2(TNumber y, TNumber x);
    TNumber tan(TNumber x);
    TNumber sin(TNumber x);
    TNumber cos(TNumber x);
    TNumber atan(TNumber x);
    TNumber asin(TNumber x);
    TNumber acos(TNumber x);
    TNumber sqrt(TNumber x);
}
namespace BufferMethods {
    uint8_t* getBytes(Buffer buf);
    int getByte(Buffer buf, int off);
    void setByte(Buffer buf, int off, int v);
    int getUint8(Buffer buf, int off);
    bool isReadOnly(Buffer buf);
    void setUint8(Buffer buf, int off, int v);
    void setNumber(Buffer buf, NumberFormat format, int offset, TNumber value);
    TNumber getNumber(Buffer buf, NumberFormat format, int offset);
    int length(Buffer s);
    void fill(Buffer buf, int value, int offset = 0, int length = -1);
    Buffer slice(Buffer buf, int offset = 0, int length = -1);
    void shift(Buffer buf, int offset, int start = 0, int length = -1);
    String toString(Buffer buf);
    String toHex(Buffer buf);
    void rotate(Buffer buf, int offset, int start = 0, int length = -1);
    void write(Buffer buf, int dstOffset, Buffer src);
    uint32_t hash(Buffer buf, int bits);
}
namespace control {
    Buffer createBuffer(int size);
    Buffer createBufferFromUTF8(String str);
}
namespace loops {
    void forever(Action a);
    void pause(int ms);
}
namespace control {
    int millis();
    int micros();
    void internalOnEvent(int src, int value, Action handler, int flags = 16);
    void reset();
    void waitMicros(int micros);
    void runInParallel(Action a);
    void waitForEvent(int src, int value);
    int deviceSerialNumber();
    Buffer deviceLongSerialNumber();
    void __log(int prority, String text);
    void dmesgValue(TValue v);
    void gc();
    void heapDump();
    void setDebugFlags(int flags);
    void heapSnapshot();
    bool profilingEnabled();
}
namespace pxt {
    typedef Button *Button_;
    typedef Buffer Sound;
    typedef void (*reset_fn_t)();
}
    typedef CODAL_PIN DevicePin;
    typedef DevicePin *DigitalInOutPin;
    typedef DevicePin *AnalogInOutPin;
    typedef DevicePin *AnalogInPin;
    typedef DevicePin *AnalogOutPin;
    typedef DevicePin *PwmPin;
    typedef DevicePin *PwmOnlyPin;
    typedef Button *Button_;
namespace pxt {
    typedef void (*OpFun)(FiberContext *ctx, unsigned arg);
    typedef void (*ApiFun)(FiberContext *ctx);
    typedef TValue (*fiber_resume_t)(void *);
}
namespace control {
    void raiseEvent(int src, int value);
    int allocateNotifyEvent();
    String deviceDalVersion();
    void dmesg(String s);
    uint32_t _ramSize();
    bool isUSBInitialized();
}
namespace serial {
    void writeDmesg();
}
namespace pxt {
    int pressureLevelByButtonId(int btnId, int codalId);
    void setupButton(int buttonId, int key);
    void op_stloc(FiberContext *ctx, unsigned arg);
    void op_ldloc(FiberContext *ctx, unsigned arg);
    void op_ldcap(FiberContext *ctx, unsigned arg);
    void op_bitconv(FiberContext *ctx, unsigned arg);
    void op_stglb(FiberContext *ctx, unsigned arg);
    void op_ldglb(FiberContext *ctx, unsigned arg);
    void op_ldlit(FiberContext *ctx, unsigned arg);
    void op_ldnumber(FiberContext *ctx, unsigned arg);
    void op_jmp(FiberContext *ctx, unsigned arg);
    void op_jmpz(FiberContext *ctx, unsigned arg);
    void op_jmpnz(FiberContext *ctx, unsigned arg);
    void op_newobj(FiberContext *ctx, unsigned arg);
    void op_ldfld(FiberContext *ctx, unsigned arg);
    void op_stfld(FiberContext *ctx, unsigned arg);
    void op_callproc(FiberContext *ctx, unsigned arg);
    void op_callind(FiberContext *ctx, unsigned arg);
    void op_ret(FiberContext *ctx, unsigned arg);
    void op_pop(FiberContext *ctx, unsigned);
    void op_popmany(FiberContext *ctx, unsigned arg);
    void op_pushmany(FiberContext *ctx, unsigned arg);
    void op_push(FiberContext *ctx, unsigned);
    void op_ldspecial(FiberContext *ctx, unsigned arg);
    void op_ldint(FiberContext *ctx, unsigned arg);
    void op_ldintneg(FiberContext *ctx, unsigned arg);
    void op_try(FiberContext *ctx, unsigned arg);
    void op_calliface(FiberContext *ctx, unsigned arg);
    void op_callget(FiberContext *ctx, unsigned arg);
    void op_callset(FiberContext *ctx, unsigned arg);
    void op_mapget(FiberContext *ctx, unsigned arg);
    void op_mapset(FiberContext *ctx, unsigned arg);
    void op_checkinst(FiberContext *ctx, unsigned arg);
}
namespace vmcache {
    RefCollection* list();
    void run(String name);
    void del(String name);
}
namespace Keyboard {
    void promptForText(int maxLength, bool numberOnly);
    void cancelTextPrompt();
    char* getTextPromptString();
    char* getLocalizedInstructions();
    int getTextPromptSelectionStart();
    int getTextPromptSelectionEnd();
    bool isSystemKeyboardSupported();
}
namespace pxt {
    int pressureLevelByButtonId(int btnId, int codalId);
    void setupButton(int buttonId, int key);
}
namespace multiplayer {
    void postImage(Image_ im);
    void postIcon(int type, int icon, Image_ im);
    void setOrigin(String origin);
    Image_ getCurrentImage();
    String getOrigin();
}
    enum Key : int;
    typedef void (*get_pixels_t)(int width, int height, uint32_t *screen);
    typedef void (*raise_event_t)(int src, int val);
    typedef void (*vm_start_t)(const char *fn);
    typedef void (*vm_start_buffer_t)(uint8_t *data, unsigned len);
    typedef int (*get_logs_t)(int logtype, char *dst, int maxSize);
    typedef int (*get_panic_code_t)();
    typedef void (*get_audio_samples_t)(int16_t *buf, unsigned numSamples);
namespace music {
    typedef int (*gentone_t)(PlayingSound *sound, uint32_t position, uint8_t cycle);
    void enableAmp(int enabled);
    void forceOutput(int outp);
    void queuePlayInstructions(int when, Buffer buf);
    void stopPlaying();
    int _createSequencer();
    String _sequencerState(int id);
    int _sequencerCurrentTick(int id);
    void _sequencerPlaySong(int id, Buffer buf, bool loop);
    void _sequencerStop(int id);
    void _sequencerSetVolume(int id, int volume);
    void _sequencerSetVolumeForAll(int volume);
    void _sequencerSetTrackVolume(int id, int trackIndex, int volume);
    void _sequencerSetDrumTrackVolume(int id, int trackIndex, int drumIndex, int volume);
    void _sequencerDispose(int id);
}
namespace pxt {
    int setScreenBrightnessSupported();
    void setScreenBrightness(int level);
    void setPalette(Buffer buf);
    void updateScreen(Image_ img);
    void updateStats(String msg);
}
namespace ImageMethods {
    int width(Image_ img);
    int height(Image_ img);
    bool isMono(Image_ img);
    bool isStatic(Image_ img);
    bool revision(Image_ img);
    void copyFrom(Image_ img, Image_ from);
    void setPixel(Image_ img, int x, int y, int c);
    int getPixel(Image_ img, int x, int y);
    void fill(Image_ img, int c);
    void getRows(Image_ img, int x, Buffer dst);
    void setRows(Image_ img, int x, Buffer src);
    void _fillRect(Image_ img, int xy, int wh, int c);
    void _mapRect(Image_ img, int xy, int wh, Buffer c);
    bool equals(Image_ img, Image_ other);
    Image_ clone(Image_ img);
    void flipX(Image_ img);
    void flipY(Image_ img);
    Image_ transposed(Image_ img);
    void scroll(Image_ img, int dx, int dy);
    Image_ doubledX(Image_ img);
    Image_ doubledY(Image_ img);
    void replace(Image_ img, int from, int to);
    Image_ doubled(Image_ img);
    void drawImage(Image_ img, Image_ from, int x, int y);
    void drawTransparentImage(Image_ img, Image_ from, int x, int y);
    bool overlapsWith(Image_ img, Image_ other, int x, int y);
    void _drawIcon(Image_ img, Buffer icon, int xy, int c);
    void _drawLine(Image_ img, int xy, int wh, int c);
    void _blitRow(Image_ img, int xy, Image_ from, int xh);
    void _drawScaledRotatedImage(Image_ dst, Image_ src, pxt::RefCollection *args);
    bool _checkOverlapsScaledRotatedImage(Image_ dst, Image_ src, pxt::RefCollection *args);
    bool _checkOverlapsTwoScaledRotatedImages(Image_ dst, Image_ src, pxt::RefCollection *args);
    bool _blit(Image_ img, Image_ src, pxt::RefCollection *args);
    void _fillCircle(Image_ img, int cxy, int r, int c);
    void _fillTriangle(Image_ img, pxt::RefCollection *args);
    void _fillPolygon4(Image_ img, pxt::RefCollection *args);
}
namespace image {
    Image_ create(int width, int height);
    Image_ ofBuffer(Buffer buf);
    Buffer doubledIcon(Buffer icon);
}
namespace settings {
    int _set(String key, Buffer data);
    int _remove(String key);
    bool _exists(String key);
    Buffer _get(String key);
    void _userClean();
    RefCollection* _list(String prefix);
}

void _wrp_pxt__mkAction(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->sp[0]);
  RefAction* a1 = (RefAction*) asRefAction(ctx->r0);
  ctx->r0 = (TValue)::pxt::mkAction(a0, a1);
  ctx->sp += 1;
}

void _wrp_pxt__templateHash(FiberContext *ctx) {
  ctx->r0 = fromInt(::pxt::templateHash());
}

void _wrp_pxt__programHash(FiberContext *ctx) {
  ctx->r0 = fromInt(::pxt::programHash());
}

void _wrp_pxt__programName(FiberContext *ctx) {
  ctx->r0 = (TValue)::pxt::programName();
}

void _wrp_pxt__programSize(FiberContext *ctx) {
  ctx->r0 = fromInt(::pxt::programSize());
}

void _wrp_pxt__getConfig(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->sp[0]);
  int a1 = (int) toInt(ctx->r0);
  ctx->r0 = fromInt(::pxt::getConfig(a0, a1));
  ctx->sp += 1;
}

void _wrp_pxt__dumpPerfCounters(FiberContext *ctx) {
  ::pxt::dumpPerfCounters();
  ctx->r0 = NULL;
}

void _wrp_numops__toString(FiberContext *ctx) {
  TValue a0 = (TValue) (ctx->r0);
  ctx->r0 = (TValue)::numops::toString(a0);
}

void _wrp_numops__toBool(FiberContext *ctx) {
  TValue a0 = (TValue) (ctx->r0);
  ctx->r0 = fromInt(::numops::toBool(a0));
}

void _wrp_numops__toBoolDecr(FiberContext *ctx) {
  TValue a0 = (TValue) (ctx->r0);
  ctx->r0 = fromInt(::numops::toBoolDecr(a0));
}

void _wrp_pxtrt__mkMap(FiberContext *ctx) {
  ctx->r0 = (TValue)::pxtrt::mkMap();
}

void _wrp_pxtrt__mapSetByString(FiberContext *ctx) {
  auto prevSP = ctx->sp;
  RefMap* a0 = (RefMap*) asRefMap(ctx->sp[1]);
  String a1 = (String) convertToString(ctx, ctx->sp[0]);
  TValue a2 = (TValue) (ctx->r0);
  if (panicCode) { ctx->sp = prevSP; return; }
  ::pxtrt::mapSetByString(a0, a1, a2);
  ctx->r0 = NULL;
  ctx->sp = prevSP;
  ctx->sp += 2;
}

void _wrp_pxtrt__mapSet(FiberContext *ctx) {
  RefMap* a0 = (RefMap*) asRefMap(ctx->sp[1]);
  unsigned a1 = (unsigned) toInt(ctx->sp[0]);
  TValue a2 = (TValue) (ctx->r0);
  ::pxtrt::mapSet(a0, a1, a2);
  ctx->r0 = NULL;
  ctx->sp += 2;
}

void _wrp_String___compare(FiberContext *ctx) {
  auto prevSP = ctx->sp;
  String a0 = (String) convertToString(ctx, ctx->sp[0]);
  String a1 = (String) convertToString(ctx, ctx->r0);
  if (panicCode) { ctx->sp = prevSP; return; }
  ctx->r0 = fromInt(::String_::compare(a0, a1));
  ctx->sp = prevSP;
  ctx->sp += 1;
}

void _wrp_Array___mk(FiberContext *ctx) {
  ctx->r0 = (TValue)::Array_::mk();
}

void _wrp_Array___length(FiberContext *ctx) {
  RefCollection* a0 = (RefCollection*) asRefCollection(ctx->r0);
  ctx->r0 = fromInt(::Array_::length(a0));
}

void _wrp_Array___setLength(FiberContext *ctx) {
  RefCollection* a0 = (RefCollection*) asRefCollection(ctx->sp[0]);
  int a1 = (int) toInt(ctx->r0);
  ::Array_::setLength(a0, a1);
  ctx->r0 = NULL;
  ctx->sp += 1;
}

void _wrp_Array___push(FiberContext *ctx) {
  RefCollection* a0 = (RefCollection*) asRefCollection(ctx->sp[0]);
  TValue a1 = (TValue) (ctx->r0);
  ::Array_::push(a0, a1);
  ctx->r0 = NULL;
  ctx->sp += 1;
}

void _wrp_Array___pop(FiberContext *ctx) {
  RefCollection* a0 = (RefCollection*) asRefCollection(ctx->r0);
  ctx->r0 = (TValue)::Array_::pop(a0);
}

void _wrp_Array___getAt(FiberContext *ctx) {
  RefCollection* a0 = (RefCollection*) asRefCollection(ctx->sp[0]);
  int a1 = (int) toInt(ctx->r0);
  ctx->r0 = (TValue)::Array_::getAt(a0, a1);
  ctx->sp += 1;
}

void _wrp_Array___setAt(FiberContext *ctx) {
  RefCollection* a0 = (RefCollection*) asRefCollection(ctx->sp[1]);
  int a1 = (int) toInt(ctx->sp[0]);
  TValue a2 = (TValue) (ctx->r0);
  ::Array_::setAt(a0, a1, a2);
  ctx->r0 = NULL;
  ctx->sp += 2;
}

void _wrp_Array___removeAt(FiberContext *ctx) {
  RefCollection* a0 = (RefCollection*) asRefCollection(ctx->sp[0]);
  int a1 = (int) toInt(ctx->r0);
  ctx->r0 = (TValue)::Array_::removeAt(a0, a1);
  ctx->sp += 1;
}

void _wrp_Array___insertAt(FiberContext *ctx) {
  RefCollection* a0 = (RefCollection*) asRefCollection(ctx->sp[1]);
  int a1 = (int) toInt(ctx->sp[0]);
  TValue a2 = (TValue) (ctx->r0);
  ::Array_::insertAt(a0, a1, a2);
  ctx->r0 = NULL;
  ctx->sp += 2;
}

void _wrp_Array___indexOf(FiberContext *ctx) {
  RefCollection* a0 = (RefCollection*) asRefCollection(ctx->sp[1]);
  TValue a1 = (TValue) (ctx->sp[0]);
  int a2 = (int) toInt(ctx->r0);
  ctx->r0 = fromInt(::Array_::indexOf(a0, a1, a2));
  ctx->sp += 2;
}

void _wrp_Array___removeElement(FiberContext *ctx) {
  RefCollection* a0 = (RefCollection*) asRefCollection(ctx->sp[0]);
  TValue a1 = (TValue) (ctx->r0);
  ctx->r0 = fromBool(::Array_::removeElement(a0, a1));
  ctx->sp += 1;
}

void _wrp_Array___isArray(FiberContext *ctx) {
  TValue a0 = (TValue) (ctx->r0);
  ctx->r0 = fromBool(::Array_::isArray(a0));
}

void _wrp_pxtrt__keysOf(FiberContext *ctx) {
  TValue a0 = (TValue) (ctx->r0);
  ctx->r0 = (TValue)::pxtrt::keysOf(a0);
}

void _wrp_pxtrt__mapDeleteByString(FiberContext *ctx) {
  auto prevSP = ctx->sp;
  RefMap* a0 = (RefMap*) asRefMap(ctx->sp[0]);
  String a1 = (String) convertToString(ctx, ctx->r0);
  if (panicCode) { ctx->sp = prevSP; return; }
  ctx->r0 = (TValue)::pxtrt::mapDeleteByString(a0, a1);
  ctx->sp = prevSP;
  ctx->sp += 1;
}

void _wrp_pxt__getGCStats(FiberContext *ctx) {
  ctx->r0 = (TValue)::pxt::getGCStats();
}

void _wrp_pxt__seedAddRandom(FiberContext *ctx) {
  unsigned a0 = (unsigned) toInt(ctx->r0);
  ::pxt::seedAddRandom(a0);
  ctx->r0 = NULL;
}

void _wrp_String___mkEmpty(FiberContext *ctx) {
  ctx->r0 = (TValue)::String_::mkEmpty();
}

void _wrp_String___fromCharCode(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->r0);
  ctx->r0 = (TValue)::String_::fromCharCode(a0);
}

void _wrp_String___charCodeAt(FiberContext *ctx) {
  auto prevSP = ctx->sp;
  String a0 = (String) convertToString(ctx, ctx->sp[0]);
  int a1 = (int) toInt(ctx->r0);
  if (panicCode) { ctx->sp = prevSP; return; }
  ctx->r0 = (TValue)::String_::charCodeAt(a0, a1);
  ctx->sp = prevSP;
  ctx->sp += 1;
}

void _wrp_String___charAt(FiberContext *ctx) {
  auto prevSP = ctx->sp;
  String a0 = (String) convertToString(ctx, ctx->sp[0]);
  int a1 = (int) toInt(ctx->r0);
  if (panicCode) { ctx->sp = prevSP; return; }
  ctx->r0 = (TValue)::String_::charAt(a0, a1);
  ctx->sp = prevSP;
  ctx->sp += 1;
}

void _wrp_String___concat(FiberContext *ctx) {
  auto prevSP = ctx->sp;
  String a0 = (String) convertToString(ctx, ctx->sp[0]);
  String a1 = (String) convertToString(ctx, ctx->r0);
  if (panicCode) { ctx->sp = prevSP; return; }
  ctx->r0 = (TValue)::String_::concat(a0, a1);
  ctx->sp = prevSP;
  ctx->sp += 1;
}

void _wrp_String___length(FiberContext *ctx) {
  auto prevSP = ctx->sp;
  String a0 = (String) convertToString(ctx, ctx->r0);
  if (panicCode) { ctx->sp = prevSP; return; }
  ctx->r0 = fromInt(::String_::length(a0));
  ctx->sp = prevSP;
}

void _wrp_String___toNumber(FiberContext *ctx) {
  auto prevSP = ctx->sp;
  String a0 = (String) convertToString(ctx, ctx->r0);
  if (panicCode) { ctx->sp = prevSP; return; }
  ctx->r0 = (TValue)::String_::toNumber(a0);
  ctx->sp = prevSP;
}

void _wrp_String___substr(FiberContext *ctx) {
  auto prevSP = ctx->sp;
  String a0 = (String) convertToString(ctx, ctx->sp[1]);
  int a1 = (int) toInt(ctx->sp[0]);
  int a2 = (int) toInt(ctx->r0);
  if (panicCode) { ctx->sp = prevSP; return; }
  ctx->r0 = (TValue)::String_::substr(a0, a1, a2);
  ctx->sp = prevSP;
  ctx->sp += 2;
}

void _wrp_String___indexOf(FiberContext *ctx) {
  auto prevSP = ctx->sp;
  String a0 = (String) convertToString(ctx, ctx->sp[1]);
  String a1 = (String) convertToString(ctx, ctx->sp[0]);
  int a2 = (int) toInt(ctx->r0);
  if (panicCode) { ctx->sp = prevSP; return; }
  ctx->r0 = fromInt(::String_::indexOf(a0, a1, a2));
  ctx->sp = prevSP;
  ctx->sp += 2;
}

void _wrp_String___includes(FiberContext *ctx) {
  auto prevSP = ctx->sp;
  String a0 = (String) convertToString(ctx, ctx->sp[1]);
  String a1 = (String) convertToString(ctx, ctx->sp[0]);
  int a2 = (int) toInt(ctx->r0);
  if (panicCode) { ctx->sp = prevSP; return; }
  ctx->r0 = fromInt(::String_::includes(a0, a1, a2));
  ctx->sp = prevSP;
  ctx->sp += 2;
}

void _wrp_Boolean___bang(FiberContext *ctx) {
  bool a0 = (bool) numops::toBool(ctx->r0);
  ctx->r0 = fromBool(::Boolean_::bang(a0));
}

void _wrp_numops__adds(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->sp[0]);
  TNumber a1 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::numops::adds(a0, a1);
  ctx->sp += 1;
}

void _wrp_numops__subs(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->sp[0]);
  TNumber a1 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::numops::subs(a0, a1);
  ctx->sp += 1;
}

void _wrp_numops__muls(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->sp[0]);
  TNumber a1 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::numops::muls(a0, a1);
  ctx->sp += 1;
}

void _wrp_numops__div(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->sp[0]);
  TNumber a1 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::numops::div(a0, a1);
  ctx->sp += 1;
}

void _wrp_numops__mod(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->sp[0]);
  TNumber a1 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::numops::mod(a0, a1);
  ctx->sp += 1;
}

void _wrp_numops__lsls(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->sp[0]);
  TNumber a1 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::numops::lsls(a0, a1);
  ctx->sp += 1;
}

void _wrp_numops__lsrs(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->sp[0]);
  TNumber a1 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::numops::lsrs(a0, a1);
  ctx->sp += 1;
}

void _wrp_numops__asrs(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->sp[0]);
  TNumber a1 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::numops::asrs(a0, a1);
  ctx->sp += 1;
}

void _wrp_numops__eors(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->sp[0]);
  TNumber a1 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::numops::eors(a0, a1);
  ctx->sp += 1;
}

void _wrp_numops__orrs(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->sp[0]);
  TNumber a1 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::numops::orrs(a0, a1);
  ctx->sp += 1;
}

void _wrp_numops__bnot(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::numops::bnot(a0);
}

void _wrp_numops__ands(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->sp[0]);
  TNumber a1 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::numops::ands(a0, a1);
  ctx->sp += 1;
}

void _wrp_numops__lt_bool(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->sp[0]);
  TNumber a1 = (TNumber) (ctx->r0);
  ctx->r0 = fromBool(::numops::lt_bool(a0, a1));
  ctx->sp += 1;
}

void _wrp_numops__le(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->sp[0]);
  TNumber a1 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::numops::le(a0, a1);
  ctx->sp += 1;
}

void _wrp_numops__lt(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->sp[0]);
  TNumber a1 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::numops::lt(a0, a1);
  ctx->sp += 1;
}

void _wrp_numops__ge(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->sp[0]);
  TNumber a1 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::numops::ge(a0, a1);
  ctx->sp += 1;
}

void _wrp_numops__gt(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->sp[0]);
  TNumber a1 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::numops::gt(a0, a1);
  ctx->sp += 1;
}

void _wrp_numops__eq(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->sp[0]);
  TNumber a1 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::numops::eq(a0, a1);
  ctx->sp += 1;
}

void _wrp_numops__neq(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->sp[0]);
  TNumber a1 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::numops::neq(a0, a1);
  ctx->sp += 1;
}

void _wrp_numops__eqq(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->sp[0]);
  TNumber a1 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::numops::eqq(a0, a1);
  ctx->sp += 1;
}

void _wrp_numops__neqq(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->sp[0]);
  TNumber a1 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::numops::neqq(a0, a1);
  ctx->sp += 1;
}

void _wrp_Math___pow(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->sp[0]);
  TNumber a1 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::Math_::pow(a0, a1);
  ctx->sp += 1;
}

void _wrp_Math___random(FiberContext *ctx) {
  ctx->r0 = (TValue)::Math_::random();
}

void _wrp_Math___randomRange(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->sp[0]);
  TNumber a1 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::Math_::randomRange(a0, a1);
  ctx->sp += 1;
}

void _wrp_Math___log(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::Math_::log(a0);
}

void _wrp_Math___log10(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::Math_::log10(a0);
}

void _wrp_Math___floor(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::Math_::floor(a0);
}

void _wrp_Math___ceil(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::Math_::ceil(a0);
}

void _wrp_Math___trunc(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::Math_::trunc(a0);
}

void _wrp_Math___round(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::Math_::round(a0);
}

void _wrp_Math___imul(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->sp[0]);
  int a1 = (int) toInt(ctx->r0);
  ctx->r0 = fromInt(::Math_::imul(a0, a1));
  ctx->sp += 1;
}

void _wrp_Math___idiv(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->sp[0]);
  int a1 = (int) toInt(ctx->r0);
  ctx->r0 = fromInt(::Math_::idiv(a0, a1));
  ctx->sp += 1;
}

void _wrp_pxt__deepSleep(FiberContext *ctx) {
  ::pxt::deepSleep();
  ctx->r0 = NULL;
}

void _wrp_pxtrt__ldlocRef(FiberContext *ctx) {
  RefRefLocal* a0 = (RefRefLocal*) asRefRefLocal(ctx->r0);
  ctx->r0 = (TValue)::pxtrt::ldlocRef(a0);
}

void _wrp_pxtrt__stlocRef(FiberContext *ctx) {
  RefRefLocal* a0 = (RefRefLocal*) asRefRefLocal(ctx->sp[0]);
  TValue a1 = (TValue) (ctx->r0);
  ::pxtrt::stlocRef(a0, a1);
  ctx->r0 = NULL;
  ctx->sp += 1;
}

void _wrp_pxtrt__mklocRef(FiberContext *ctx) {
  ctx->r0 = (TValue)::pxtrt::mklocRef();
}

void _wrp_pxtrt__stclo(FiberContext *ctx) {
  RefAction* a0 = (RefAction*) asRefAction(ctx->sp[1]);
  int a1 = (int) toInt(ctx->sp[0]);
  TValue a2 = (TValue) (ctx->r0);
  ctx->r0 = (TValue)::pxtrt::stclo(a0, a1, a2);
  ctx->sp += 2;
}

void _wrp_pxtrt__panic(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->r0);
  ::pxtrt::panic(a0);
  ctx->r0 = NULL;
}

void _wrp_pxt__typeOf(FiberContext *ctx) {
  TValue a0 = (TValue) (ctx->r0);
  ctx->r0 = (TValue)::pxt::typeOf(a0);
}

void _wrp_pxt__endTry(FiberContext *ctx) {
  ::pxt::endTry();
  ctx->r0 = NULL;
}

void _wrp_pxt__throwValue(FiberContext *ctx) {
  TValue a0 = (TValue) (ctx->r0);
  ::pxt::throwValue(a0);
  ctx->r0 = NULL;
}

void _wrp_pxt__getThrownValue(FiberContext *ctx) {
  ctx->r0 = (TValue)::pxt::getThrownValue();
}

void _wrp_pxt__endFinally(FiberContext *ctx) {
  ::pxt::endFinally();
  ctx->r0 = NULL;
}

void _wrp_Math___log2(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::Math_::log2(a0);
}

void _wrp_Math___exp(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::Math_::exp(a0);
}

void _wrp_Math___tanh(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::Math_::tanh(a0);
}

void _wrp_Math___sinh(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::Math_::sinh(a0);
}

void _wrp_Math___cosh(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::Math_::cosh(a0);
}

void _wrp_Math___atanh(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::Math_::atanh(a0);
}

void _wrp_Math___asinh(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::Math_::asinh(a0);
}

void _wrp_Math___acosh(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::Math_::acosh(a0);
}

void _wrp_Math___atan2(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->sp[0]);
  TNumber a1 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::Math_::atan2(a0, a1);
  ctx->sp += 1;
}

void _wrp_Math___tan(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::Math_::tan(a0);
}

void _wrp_Math___sin(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::Math_::sin(a0);
}

void _wrp_Math___cos(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::Math_::cos(a0);
}

void _wrp_Math___atan(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::Math_::atan(a0);
}

void _wrp_Math___asin(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::Math_::asin(a0);
}

void _wrp_Math___acos(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::Math_::acos(a0);
}

void _wrp_Math___sqrt(FiberContext *ctx) {
  TNumber a0 = (TNumber) (ctx->r0);
  ctx->r0 = (TValue)::Math_::sqrt(a0);
}

void _wrp_BufferMethods__getBytes(FiberContext *ctx) {
  Buffer a0 = (Buffer) asBuffer(ctx->r0);
  ctx->r0 = (TValue)::BufferMethods::getBytes(a0);
}

void _wrp_BufferMethods__getByte(FiberContext *ctx) {
  Buffer a0 = (Buffer) asBuffer(ctx->sp[0]);
  int a1 = (int) toInt(ctx->r0);
  ctx->r0 = fromInt(::BufferMethods::getByte(a0, a1));
  ctx->sp += 1;
}

void _wrp_BufferMethods__setByte(FiberContext *ctx) {
  Buffer a0 = (Buffer) asBuffer(ctx->sp[1]);
  int a1 = (int) toInt(ctx->sp[0]);
  int a2 = (int) toInt(ctx->r0);
  ::BufferMethods::setByte(a0, a1, a2);
  ctx->r0 = NULL;
  ctx->sp += 2;
}

void _wrp_BufferMethods__getUint8(FiberContext *ctx) {
  Buffer a0 = (Buffer) asBuffer(ctx->sp[0]);
  int a1 = (int) toInt(ctx->r0);
  ctx->r0 = fromInt(::BufferMethods::getUint8(a0, a1));
  ctx->sp += 1;
}

void _wrp_BufferMethods__isReadOnly(FiberContext *ctx) {
  Buffer a0 = (Buffer) asBuffer(ctx->r0);
  ctx->r0 = fromBool(::BufferMethods::isReadOnly(a0));
}

void _wrp_BufferMethods__setUint8(FiberContext *ctx) {
  Buffer a0 = (Buffer) asBuffer(ctx->sp[1]);
  int a1 = (int) toInt(ctx->sp[0]);
  int a2 = (int) toInt(ctx->r0);
  ::BufferMethods::setUint8(a0, a1, a2);
  ctx->r0 = NULL;
  ctx->sp += 2;
}

void _wrp_BufferMethods__setNumber(FiberContext *ctx) {
  Buffer a0 = (Buffer) asBuffer(ctx->sp[2]);
  NumberFormat a1 = (NumberFormat) toInt(ctx->sp[1]);
  int a2 = (int) toInt(ctx->sp[0]);
  TNumber a3 = (TNumber) (ctx->r0);
  ::BufferMethods::setNumber(a0, a1, a2, a3);
  ctx->r0 = NULL;
  ctx->sp += 3;
}

void _wrp_BufferMethods__getNumber(FiberContext *ctx) {
  Buffer a0 = (Buffer) asBuffer(ctx->sp[1]);
  NumberFormat a1 = (NumberFormat) toInt(ctx->sp[0]);
  int a2 = (int) toInt(ctx->r0);
  ctx->r0 = (TValue)::BufferMethods::getNumber(a0, a1, a2);
  ctx->sp += 2;
}

void _wrp_BufferMethods__length(FiberContext *ctx) {
  Buffer a0 = (Buffer) asBuffer(ctx->r0);
  ctx->r0 = fromInt(::BufferMethods::length(a0));
}

void _wrp_BufferMethods__fill(FiberContext *ctx) {
  Buffer a0 = (Buffer) asBuffer(ctx->sp[2]);
  int a1 = (int) toInt(ctx->sp[1]);
  int a2 = (int) toInt(ctx->sp[0]);
  int a3 = (int) toInt(ctx->r0);
  ::BufferMethods::fill(a0, a1, a2, a3);
  ctx->r0 = NULL;
  ctx->sp += 3;
}

void _wrp_BufferMethods__slice(FiberContext *ctx) {
  Buffer a0 = (Buffer) asBuffer(ctx->sp[1]);
  int a1 = (int) toInt(ctx->sp[0]);
  int a2 = (int) toInt(ctx->r0);
  ctx->r0 = (TValue)::BufferMethods::slice(a0, a1, a2);
  ctx->sp += 2;
}

void _wrp_BufferMethods__shift(FiberContext *ctx) {
  Buffer a0 = (Buffer) asBuffer(ctx->sp[2]);
  int a1 = (int) toInt(ctx->sp[1]);
  int a2 = (int) toInt(ctx->sp[0]);
  int a3 = (int) toInt(ctx->r0);
  ::BufferMethods::shift(a0, a1, a2, a3);
  ctx->r0 = NULL;
  ctx->sp += 3;
}

void _wrp_BufferMethods__toString(FiberContext *ctx) {
  Buffer a0 = (Buffer) asBuffer(ctx->r0);
  ctx->r0 = (TValue)::BufferMethods::toString(a0);
}

void _wrp_BufferMethods__toHex(FiberContext *ctx) {
  Buffer a0 = (Buffer) asBuffer(ctx->r0);
  ctx->r0 = (TValue)::BufferMethods::toHex(a0);
}

void _wrp_BufferMethods__rotate(FiberContext *ctx) {
  Buffer a0 = (Buffer) asBuffer(ctx->sp[2]);
  int a1 = (int) toInt(ctx->sp[1]);
  int a2 = (int) toInt(ctx->sp[0]);
  int a3 = (int) toInt(ctx->r0);
  ::BufferMethods::rotate(a0, a1, a2, a3);
  ctx->r0 = NULL;
  ctx->sp += 3;
}

void _wrp_BufferMethods__write(FiberContext *ctx) {
  Buffer a0 = (Buffer) asBuffer(ctx->sp[1]);
  int a1 = (int) toInt(ctx->sp[0]);
  Buffer a2 = (Buffer) asBuffer(ctx->r0);
  ::BufferMethods::write(a0, a1, a2);
  ctx->r0 = NULL;
  ctx->sp += 2;
}

void _wrp_BufferMethods__hash(FiberContext *ctx) {
  Buffer a0 = (Buffer) asBuffer(ctx->sp[0]);
  int a1 = (int) toInt(ctx->r0);
  ctx->r0 = fromInt(::BufferMethods::hash(a0, a1));
  ctx->sp += 1;
}

void _wrp_control__createBuffer(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->r0);
  ctx->r0 = (TValue)::control::createBuffer(a0);
}

void _wrp_control__createBufferFromUTF8(FiberContext *ctx) {
  auto prevSP = ctx->sp;
  String a0 = (String) convertToString(ctx, ctx->r0);
  if (panicCode) { ctx->sp = prevSP; return; }
  ctx->r0 = (TValue)::control::createBufferFromUTF8(a0);
  ctx->sp = prevSP;
}

void _wrp_loops__forever(FiberContext *ctx) {
  Action a0 = (Action) asRefAction(ctx->r0);
  ::loops::forever(a0);
  ctx->r0 = NULL;
}

void _wrp_loops__pause(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->r0);
  ::loops::pause(a0);
  ctx->r0 = NULL;
}

void _wrp_control__millis(FiberContext *ctx) {
  ctx->r0 = fromInt(::control::millis());
}

void _wrp_control__micros(FiberContext *ctx) {
  ctx->r0 = fromInt(::control::micros());
}

void _wrp_control__internalOnEvent(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->sp[2]);
  int a1 = (int) toInt(ctx->sp[1]);
  Action a2 = (Action) asRefAction(ctx->sp[0]);
  int a3 = (int) toInt(ctx->r0);
  ::control::internalOnEvent(a0, a1, a2, a3);
  ctx->r0 = NULL;
  ctx->sp += 3;
}

void _wrp_control__reset(FiberContext *ctx) {
  ::control::reset();
  ctx->r0 = NULL;
}

void _wrp_control__waitMicros(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->r0);
  ::control::waitMicros(a0);
  ctx->r0 = NULL;
}

void _wrp_control__runInParallel(FiberContext *ctx) {
  Action a0 = (Action) asRefAction(ctx->r0);
  ::control::runInParallel(a0);
  ctx->r0 = NULL;
}

void _wrp_control__waitForEvent(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->sp[0]);
  int a1 = (int) toInt(ctx->r0);
  ::control::waitForEvent(a0, a1);
  ctx->r0 = NULL;
  ctx->sp += 1;
}

void _wrp_control__deviceSerialNumber(FiberContext *ctx) {
  ctx->r0 = fromInt(::control::deviceSerialNumber());
}

void _wrp_control__deviceLongSerialNumber(FiberContext *ctx) {
  ctx->r0 = (TValue)::control::deviceLongSerialNumber();
}

void _wrp_control____log(FiberContext *ctx) {
  auto prevSP = ctx->sp;
  int a0 = (int) toInt(ctx->sp[0]);
  String a1 = (String) convertToString(ctx, ctx->r0);
  if (panicCode) { ctx->sp = prevSP; return; }
  ::control::__log(a0, a1);
  ctx->r0 = NULL;
  ctx->sp = prevSP;
  ctx->sp += 1;
}

void _wrp_control__dmesgValue(FiberContext *ctx) {
  TValue a0 = (TValue) (ctx->r0);
  ::control::dmesgValue(a0);
  ctx->r0 = NULL;
}

void _wrp_control__gc(FiberContext *ctx) {
  ::control::gc();
  ctx->r0 = NULL;
}

void _wrp_control__heapDump(FiberContext *ctx) {
  ::control::heapDump();
  ctx->r0 = NULL;
}

void _wrp_control__setDebugFlags(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->r0);
  ::control::setDebugFlags(a0);
  ctx->r0 = NULL;
}

void _wrp_control__heapSnapshot(FiberContext *ctx) {
  ::control::heapSnapshot();
  ctx->r0 = NULL;
}

void _wrp_control__profilingEnabled(FiberContext *ctx) {
  ctx->r0 = fromBool(::control::profilingEnabled());
}

void _wrp_control__raiseEvent(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->sp[0]);
  int a1 = (int) toInt(ctx->r0);
  ::control::raiseEvent(a0, a1);
  ctx->r0 = NULL;
  ctx->sp += 1;
}

void _wrp_control__allocateNotifyEvent(FiberContext *ctx) {
  ctx->r0 = fromInt(::control::allocateNotifyEvent());
}

void _wrp_control__deviceDalVersion(FiberContext *ctx) {
  ctx->r0 = (TValue)::control::deviceDalVersion();
}

void _wrp_control__dmesg(FiberContext *ctx) {
  auto prevSP = ctx->sp;
  String a0 = (String) convertToString(ctx, ctx->r0);
  if (panicCode) { ctx->sp = prevSP; return; }
  ::control::dmesg(a0);
  ctx->r0 = NULL;
  ctx->sp = prevSP;
}

void _wrp_control___ramSize(FiberContext *ctx) {
  ctx->r0 = fromInt(::control::_ramSize());
}

void _wrp_control__isUSBInitialized(FiberContext *ctx) {
  ctx->r0 = fromBool(::control::isUSBInitialized());
}

void _wrp_serial__writeDmesg(FiberContext *ctx) {
  ::serial::writeDmesg();
  ctx->r0 = NULL;
}

void _wrp_pxt__pressureLevelByButtonId(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->sp[0]);
  int a1 = (int) toInt(ctx->r0);
  ctx->r0 = fromInt(::pxt::pressureLevelByButtonId(a0, a1));
  ctx->sp += 1;
}

void _wrp_pxt__setupButton(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->sp[0]);
  int a1 = (int) toInt(ctx->r0);
  ::pxt::setupButton(a0, a1);
  ctx->r0 = NULL;
  ctx->sp += 1;
}

void _wrp_vmcache__list(FiberContext *ctx) {
  ctx->r0 = (TValue)::vmcache::list();
}

void _wrp_vmcache__run(FiberContext *ctx) {
  auto prevSP = ctx->sp;
  String a0 = (String) convertToString(ctx, ctx->r0);
  if (panicCode) { ctx->sp = prevSP; return; }
  ::vmcache::run(a0);
  ctx->r0 = NULL;
  ctx->sp = prevSP;
}

void _wrp_vmcache__del(FiberContext *ctx) {
  auto prevSP = ctx->sp;
  String a0 = (String) convertToString(ctx, ctx->r0);
  if (panicCode) { ctx->sp = prevSP; return; }
  ::vmcache::del(a0);
  ctx->r0 = NULL;
  ctx->sp = prevSP;
}

void _wrp_Keyboard__promptForText(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->sp[0]);
  bool a1 = (bool) numops::toBool(ctx->r0);
  ::Keyboard::promptForText(a0, a1);
  ctx->r0 = NULL;
  ctx->sp += 1;
}

void _wrp_Keyboard__cancelTextPrompt(FiberContext *ctx) {
  ::Keyboard::cancelTextPrompt();
  ctx->r0 = NULL;
}

void _wrp_Keyboard__getTextPromptString(FiberContext *ctx) {
  ctx->r0 = (TValue)::Keyboard::getTextPromptString();
}

void _wrp_Keyboard__getLocalizedInstructions(FiberContext *ctx) {
  ctx->r0 = (TValue)::Keyboard::getLocalizedInstructions();
}

void _wrp_Keyboard__getTextPromptSelectionStart(FiberContext *ctx) {
  ctx->r0 = fromInt(::Keyboard::getTextPromptSelectionStart());
}

void _wrp_Keyboard__getTextPromptSelectionEnd(FiberContext *ctx) {
  ctx->r0 = fromInt(::Keyboard::getTextPromptSelectionEnd());
}

void _wrp_Keyboard__isSystemKeyboardSupported(FiberContext *ctx) {
  ctx->r0 = fromBool(::Keyboard::isSystemKeyboardSupported());
}

void _wrp_multiplayer__postImage(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->r0);
  ::multiplayer::postImage(a0);
  ctx->r0 = NULL;
}

void _wrp_multiplayer__postIcon(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->sp[1]);
  int a1 = (int) toInt(ctx->sp[0]);
  Image_ a2 = (Image_) asImage_(ctx->r0);
  ::multiplayer::postIcon(a0, a1, a2);
  ctx->r0 = NULL;
  ctx->sp += 2;
}

void _wrp_multiplayer__setOrigin(FiberContext *ctx) {
  auto prevSP = ctx->sp;
  String a0 = (String) convertToString(ctx, ctx->r0);
  if (panicCode) { ctx->sp = prevSP; return; }
  ::multiplayer::setOrigin(a0);
  ctx->r0 = NULL;
  ctx->sp = prevSP;
}

void _wrp_multiplayer__getCurrentImage(FiberContext *ctx) {
  ctx->r0 = (TValue)::multiplayer::getCurrentImage();
}

void _wrp_multiplayer__getOrigin(FiberContext *ctx) {
  ctx->r0 = (TValue)::multiplayer::getOrigin();
}

void _wrp_music__enableAmp(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->r0);
  ::music::enableAmp(a0);
  ctx->r0 = NULL;
}

void _wrp_music__forceOutput(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->r0);
  ::music::forceOutput(a0);
  ctx->r0 = NULL;
}

void _wrp_music__queuePlayInstructions(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->sp[0]);
  Buffer a1 = (Buffer) asBuffer(ctx->r0);
  ::music::queuePlayInstructions(a0, a1);
  ctx->r0 = NULL;
  ctx->sp += 1;
}

void _wrp_music__stopPlaying(FiberContext *ctx) {
  ::music::stopPlaying();
  ctx->r0 = NULL;
}

void _wrp_music___createSequencer(FiberContext *ctx) {
  ctx->r0 = fromInt(::music::_createSequencer());
}

void _wrp_music___sequencerState(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->r0);
  ctx->r0 = (TValue)::music::_sequencerState(a0);
}

void _wrp_music___sequencerCurrentTick(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->r0);
  ctx->r0 = fromInt(::music::_sequencerCurrentTick(a0));
}

void _wrp_music___sequencerPlaySong(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->sp[1]);
  Buffer a1 = (Buffer) asBuffer(ctx->sp[0]);
  bool a2 = (bool) numops::toBool(ctx->r0);
  ::music::_sequencerPlaySong(a0, a1, a2);
  ctx->r0 = NULL;
  ctx->sp += 2;
}

void _wrp_music___sequencerStop(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->r0);
  ::music::_sequencerStop(a0);
  ctx->r0 = NULL;
}

void _wrp_music___sequencerSetVolume(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->sp[0]);
  int a1 = (int) toInt(ctx->r0);
  ::music::_sequencerSetVolume(a0, a1);
  ctx->r0 = NULL;
  ctx->sp += 1;
}

void _wrp_music___sequencerSetVolumeForAll(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->r0);
  ::music::_sequencerSetVolumeForAll(a0);
  ctx->r0 = NULL;
}

void _wrp_music___sequencerSetTrackVolume(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->sp[1]);
  int a1 = (int) toInt(ctx->sp[0]);
  int a2 = (int) toInt(ctx->r0);
  ::music::_sequencerSetTrackVolume(a0, a1, a2);
  ctx->r0 = NULL;
  ctx->sp += 2;
}

void _wrp_music___sequencerSetDrumTrackVolume(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->sp[2]);
  int a1 = (int) toInt(ctx->sp[1]);
  int a2 = (int) toInt(ctx->sp[0]);
  int a3 = (int) toInt(ctx->r0);
  ::music::_sequencerSetDrumTrackVolume(a0, a1, a2, a3);
  ctx->r0 = NULL;
  ctx->sp += 3;
}

void _wrp_music___sequencerDispose(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->r0);
  ::music::_sequencerDispose(a0);
  ctx->r0 = NULL;
}

void _wrp_pxt__setScreenBrightnessSupported(FiberContext *ctx) {
  ctx->r0 = fromInt(::pxt::setScreenBrightnessSupported());
}

void _wrp_pxt__setScreenBrightness(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->r0);
  ::pxt::setScreenBrightness(a0);
  ctx->r0 = NULL;
}

void _wrp_pxt__setPalette(FiberContext *ctx) {
  Buffer a0 = (Buffer) asBuffer(ctx->r0);
  ::pxt::setPalette(a0);
  ctx->r0 = NULL;
}

void _wrp_pxt__updateScreen(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->r0);
  ::pxt::updateScreen(a0);
  ctx->r0 = NULL;
}

void _wrp_pxt__updateStats(FiberContext *ctx) {
  auto prevSP = ctx->sp;
  String a0 = (String) convertToString(ctx, ctx->r0);
  if (panicCode) { ctx->sp = prevSP; return; }
  ::pxt::updateStats(a0);
  ctx->r0 = NULL;
  ctx->sp = prevSP;
}

void _wrp_ImageMethods__width(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->r0);
  ctx->r0 = fromInt(::ImageMethods::width(a0));
}

void _wrp_ImageMethods__height(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->r0);
  ctx->r0 = fromInt(::ImageMethods::height(a0));
}

void _wrp_ImageMethods__isMono(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->r0);
  ctx->r0 = fromBool(::ImageMethods::isMono(a0));
}

void _wrp_ImageMethods__isStatic(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->r0);
  ctx->r0 = fromBool(::ImageMethods::isStatic(a0));
}

void _wrp_ImageMethods__revision(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->r0);
  ctx->r0 = fromBool(::ImageMethods::revision(a0));
}

void _wrp_ImageMethods__copyFrom(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->sp[0]);
  Image_ a1 = (Image_) asImage_(ctx->r0);
  ::ImageMethods::copyFrom(a0, a1);
  ctx->r0 = NULL;
  ctx->sp += 1;
}

void _wrp_ImageMethods__setPixel(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->sp[2]);
  int a1 = (int) toInt(ctx->sp[1]);
  int a2 = (int) toInt(ctx->sp[0]);
  int a3 = (int) toInt(ctx->r0);
  ::ImageMethods::setPixel(a0, a1, a2, a3);
  ctx->r0 = NULL;
  ctx->sp += 3;
}

void _wrp_ImageMethods__getPixel(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->sp[1]);
  int a1 = (int) toInt(ctx->sp[0]);
  int a2 = (int) toInt(ctx->r0);
  ctx->r0 = fromInt(::ImageMethods::getPixel(a0, a1, a2));
  ctx->sp += 2;
}

void _wrp_ImageMethods__fill(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->sp[0]);
  int a1 = (int) toInt(ctx->r0);
  ::ImageMethods::fill(a0, a1);
  ctx->r0 = NULL;
  ctx->sp += 1;
}

void _wrp_ImageMethods__getRows(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->sp[1]);
  int a1 = (int) toInt(ctx->sp[0]);
  Buffer a2 = (Buffer) asBuffer(ctx->r0);
  ::ImageMethods::getRows(a0, a1, a2);
  ctx->r0 = NULL;
  ctx->sp += 2;
}

void _wrp_ImageMethods__setRows(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->sp[1]);
  int a1 = (int) toInt(ctx->sp[0]);
  Buffer a2 = (Buffer) asBuffer(ctx->r0);
  ::ImageMethods::setRows(a0, a1, a2);
  ctx->r0 = NULL;
  ctx->sp += 2;
}

void _wrp_ImageMethods___fillRect(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->sp[2]);
  int a1 = (int) toInt(ctx->sp[1]);
  int a2 = (int) toInt(ctx->sp[0]);
  int a3 = (int) toInt(ctx->r0);
  ::ImageMethods::_fillRect(a0, a1, a2, a3);
  ctx->r0 = NULL;
  ctx->sp += 3;
}

void _wrp_ImageMethods___mapRect(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->sp[2]);
  int a1 = (int) toInt(ctx->sp[1]);
  int a2 = (int) toInt(ctx->sp[0]);
  Buffer a3 = (Buffer) asBuffer(ctx->r0);
  ::ImageMethods::_mapRect(a0, a1, a2, a3);
  ctx->r0 = NULL;
  ctx->sp += 3;
}

void _wrp_ImageMethods__equals(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->sp[0]);
  Image_ a1 = (Image_) asImage_(ctx->r0);
  ctx->r0 = fromBool(::ImageMethods::equals(a0, a1));
  ctx->sp += 1;
}

void _wrp_ImageMethods__clone(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->r0);
  ctx->r0 = (TValue)::ImageMethods::clone(a0);
}

void _wrp_ImageMethods__flipX(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->r0);
  ::ImageMethods::flipX(a0);
  ctx->r0 = NULL;
}

void _wrp_ImageMethods__flipY(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->r0);
  ::ImageMethods::flipY(a0);
  ctx->r0 = NULL;
}

void _wrp_ImageMethods__transposed(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->r0);
  ctx->r0 = (TValue)::ImageMethods::transposed(a0);
}

void _wrp_ImageMethods__scroll(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->sp[1]);
  int a1 = (int) toInt(ctx->sp[0]);
  int a2 = (int) toInt(ctx->r0);
  ::ImageMethods::scroll(a0, a1, a2);
  ctx->r0 = NULL;
  ctx->sp += 2;
}

void _wrp_ImageMethods__doubledX(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->r0);
  ctx->r0 = (TValue)::ImageMethods::doubledX(a0);
}

void _wrp_ImageMethods__doubledY(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->r0);
  ctx->r0 = (TValue)::ImageMethods::doubledY(a0);
}

void _wrp_ImageMethods__replace(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->sp[1]);
  int a1 = (int) toInt(ctx->sp[0]);
  int a2 = (int) toInt(ctx->r0);
  ::ImageMethods::replace(a0, a1, a2);
  ctx->r0 = NULL;
  ctx->sp += 2;
}

void _wrp_ImageMethods__doubled(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->r0);
  ctx->r0 = (TValue)::ImageMethods::doubled(a0);
}

void _wrp_ImageMethods__drawImage(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->sp[2]);
  Image_ a1 = (Image_) asImage_(ctx->sp[1]);
  int a2 = (int) toInt(ctx->sp[0]);
  int a3 = (int) toInt(ctx->r0);
  ::ImageMethods::drawImage(a0, a1, a2, a3);
  ctx->r0 = NULL;
  ctx->sp += 3;
}

void _wrp_ImageMethods__drawTransparentImage(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->sp[2]);
  Image_ a1 = (Image_) asImage_(ctx->sp[1]);
  int a2 = (int) toInt(ctx->sp[0]);
  int a3 = (int) toInt(ctx->r0);
  ::ImageMethods::drawTransparentImage(a0, a1, a2, a3);
  ctx->r0 = NULL;
  ctx->sp += 3;
}

void _wrp_ImageMethods__overlapsWith(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->sp[2]);
  Image_ a1 = (Image_) asImage_(ctx->sp[1]);
  int a2 = (int) toInt(ctx->sp[0]);
  int a3 = (int) toInt(ctx->r0);
  ctx->r0 = fromBool(::ImageMethods::overlapsWith(a0, a1, a2, a3));
  ctx->sp += 3;
}

void _wrp_ImageMethods___drawIcon(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->sp[2]);
  Buffer a1 = (Buffer) asBuffer(ctx->sp[1]);
  int a2 = (int) toInt(ctx->sp[0]);
  int a3 = (int) toInt(ctx->r0);
  ::ImageMethods::_drawIcon(a0, a1, a2, a3);
  ctx->r0 = NULL;
  ctx->sp += 3;
}

void _wrp_ImageMethods___drawLine(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->sp[2]);
  int a1 = (int) toInt(ctx->sp[1]);
  int a2 = (int) toInt(ctx->sp[0]);
  int a3 = (int) toInt(ctx->r0);
  ::ImageMethods::_drawLine(a0, a1, a2, a3);
  ctx->r0 = NULL;
  ctx->sp += 3;
}

void _wrp_ImageMethods___blitRow(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->sp[2]);
  int a1 = (int) toInt(ctx->sp[1]);
  Image_ a2 = (Image_) asImage_(ctx->sp[0]);
  int a3 = (int) toInt(ctx->r0);
  ::ImageMethods::_blitRow(a0, a1, a2, a3);
  ctx->r0 = NULL;
  ctx->sp += 3;
}

void _wrp_ImageMethods___drawScaledRotatedImage(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->sp[1]);
  Image_ a1 = (Image_) asImage_(ctx->sp[0]);
  pxt::RefCollection* a2 = (pxt::RefCollection*) pxt::asRefCollection(ctx->r0);
  ::ImageMethods::_drawScaledRotatedImage(a0, a1, a2);
  ctx->r0 = NULL;
  ctx->sp += 2;
}

void _wrp_ImageMethods___checkOverlapsScaledRotatedImage(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->sp[1]);
  Image_ a1 = (Image_) asImage_(ctx->sp[0]);
  pxt::RefCollection* a2 = (pxt::RefCollection*) pxt::asRefCollection(ctx->r0);
  ctx->r0 = fromBool(::ImageMethods::_checkOverlapsScaledRotatedImage(a0, a1, a2));
  ctx->sp += 2;
}

void _wrp_ImageMethods___checkOverlapsTwoScaledRotatedImages(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->sp[1]);
  Image_ a1 = (Image_) asImage_(ctx->sp[0]);
  pxt::RefCollection* a2 = (pxt::RefCollection*) pxt::asRefCollection(ctx->r0);
  ctx->r0 = fromBool(::ImageMethods::_checkOverlapsTwoScaledRotatedImages(a0, a1, a2));
  ctx->sp += 2;
}

void _wrp_ImageMethods___blit(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->sp[1]);
  Image_ a1 = (Image_) asImage_(ctx->sp[0]);
  pxt::RefCollection* a2 = (pxt::RefCollection*) pxt::asRefCollection(ctx->r0);
  ctx->r0 = fromBool(::ImageMethods::_blit(a0, a1, a2));
  ctx->sp += 2;
}

void _wrp_ImageMethods___fillCircle(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->sp[2]);
  int a1 = (int) toInt(ctx->sp[1]);
  int a2 = (int) toInt(ctx->sp[0]);
  int a3 = (int) toInt(ctx->r0);
  ::ImageMethods::_fillCircle(a0, a1, a2, a3);
  ctx->r0 = NULL;
  ctx->sp += 3;
}

void _wrp_ImageMethods___fillTriangle(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->sp[0]);
  pxt::RefCollection* a1 = (pxt::RefCollection*) pxt::asRefCollection(ctx->r0);
  ::ImageMethods::_fillTriangle(a0, a1);
  ctx->r0 = NULL;
  ctx->sp += 1;
}

void _wrp_ImageMethods___fillPolygon4(FiberContext *ctx) {
  Image_ a0 = (Image_) asImage_(ctx->sp[0]);
  pxt::RefCollection* a1 = (pxt::RefCollection*) pxt::asRefCollection(ctx->r0);
  ::ImageMethods::_fillPolygon4(a0, a1);
  ctx->r0 = NULL;
  ctx->sp += 1;
}

void _wrp_image__create(FiberContext *ctx) {
  int a0 = (int) toInt(ctx->sp[0]);
  int a1 = (int) toInt(ctx->r0);
  ctx->r0 = (TValue)::image::create(a0, a1);
  ctx->sp += 1;
}

void _wrp_image__ofBuffer(FiberContext *ctx) {
  Buffer a0 = (Buffer) asBuffer(ctx->r0);
  ctx->r0 = (TValue)::image::ofBuffer(a0);
}

void _wrp_image__doubledIcon(FiberContext *ctx) {
  Buffer a0 = (Buffer) asBuffer(ctx->r0);
  ctx->r0 = (TValue)::image::doubledIcon(a0);
}

void _wrp_settings___set(FiberContext *ctx) {
  auto prevSP = ctx->sp;
  String a0 = (String) convertToString(ctx, ctx->sp[0]);
  Buffer a1 = (Buffer) asBuffer(ctx->r0);
  if (panicCode) { ctx->sp = prevSP; return; }
  ctx->r0 = fromInt(::settings::_set(a0, a1));
  ctx->sp = prevSP;
  ctx->sp += 1;
}

void _wrp_settings___remove(FiberContext *ctx) {
  auto prevSP = ctx->sp;
  String a0 = (String) convertToString(ctx, ctx->r0);
  if (panicCode) { ctx->sp = prevSP; return; }
  ctx->r0 = fromInt(::settings::_remove(a0));
  ctx->sp = prevSP;
}

void _wrp_settings___exists(FiberContext *ctx) {
  auto prevSP = ctx->sp;
  String a0 = (String) convertToString(ctx, ctx->r0);
  if (panicCode) { ctx->sp = prevSP; return; }
  ctx->r0 = fromBool(::settings::_exists(a0));
  ctx->sp = prevSP;
}

void _wrp_settings___get(FiberContext *ctx) {
  auto prevSP = ctx->sp;
  String a0 = (String) convertToString(ctx, ctx->r0);
  if (panicCode) { ctx->sp = prevSP; return; }
  ctx->r0 = (TValue)::settings::_get(a0);
  ctx->sp = prevSP;
}

void _wrp_settings___userClean(FiberContext *ctx) {
  ::settings::_userClean();
  ctx->r0 = NULL;
}

void _wrp_settings___list(FiberContext *ctx) {
  auto prevSP = ctx->sp;
  String a0 = (String) convertToString(ctx, ctx->r0);
  if (panicCode) { ctx->sp = prevSP; return; }
  ctx->r0 = (TValue)::settings::_list(a0);
  ctx->sp = prevSP;
}

PXT_SHIMS_BEGIN
{ "pxt::mkAction", (OpFun)(void*)_wrp_pxt__mkAction, 2 },
{ "pxt::templateHash", (OpFun)(void*)_wrp_pxt__templateHash, 0 },
{ "pxt::programHash", (OpFun)(void*)_wrp_pxt__programHash, 0 },
{ "pxt::programName", (OpFun)(void*)_wrp_pxt__programName, 0 },
{ "pxt::programSize", (OpFun)(void*)_wrp_pxt__programSize, 0 },
{ "pxt::getConfig", (OpFun)(void*)_wrp_pxt__getConfig, 2 },
{ "pxt::dumpPerfCounters", (OpFun)(void*)_wrp_pxt__dumpPerfCounters, 0 },
{ "numops::toString", (OpFun)(void*)_wrp_numops__toString, 1 },
{ "numops::toBool", (OpFun)(void*)_wrp_numops__toBool, 1 },
{ "numops::toBoolDecr", (OpFun)(void*)_wrp_numops__toBoolDecr, 1 },
{ "pxtrt::mkMap", (OpFun)(void*)_wrp_pxtrt__mkMap, 0 },
{ "pxtrt::mapSetByString", (OpFun)(void*)_wrp_pxtrt__mapSetByString, 3 },
{ "pxtrt::mapSet", (OpFun)(void*)_wrp_pxtrt__mapSet, 3 },
{ "String_::compare", (OpFun)(void*)_wrp_String___compare, 2 },
{ "Array_::mk", (OpFun)(void*)_wrp_Array___mk, 0 },
{ "Array_::length", (OpFun)(void*)_wrp_Array___length, 1 },
{ "Array_::setLength", (OpFun)(void*)_wrp_Array___setLength, 2 },
{ "Array_::push", (OpFun)(void*)_wrp_Array___push, 2 },
{ "Array_::pop", (OpFun)(void*)_wrp_Array___pop, 1 },
{ "Array_::getAt", (OpFun)(void*)_wrp_Array___getAt, 2 },
{ "Array_::setAt", (OpFun)(void*)_wrp_Array___setAt, 3 },
{ "Array_::removeAt", (OpFun)(void*)_wrp_Array___removeAt, 2 },
{ "Array_::insertAt", (OpFun)(void*)_wrp_Array___insertAt, 3 },
{ "Array_::indexOf", (OpFun)(void*)_wrp_Array___indexOf, 3 },
{ "Array_::removeElement", (OpFun)(void*)_wrp_Array___removeElement, 2 },
{ "Array_::isArray", (OpFun)(void*)_wrp_Array___isArray, 1 },
{ "pxtrt::keysOf", (OpFun)(void*)_wrp_pxtrt__keysOf, 1 },
{ "pxtrt::mapDeleteByString", (OpFun)(void*)_wrp_pxtrt__mapDeleteByString, 2 },
{ "pxt::getGCStats", (OpFun)(void*)_wrp_pxt__getGCStats, 0 },
{ "pxt::seedAddRandom", (OpFun)(void*)_wrp_pxt__seedAddRandom, 1 },
{ "String_::mkEmpty", (OpFun)(void*)_wrp_String___mkEmpty, 0 },
{ "String_::fromCharCode", (OpFun)(void*)_wrp_String___fromCharCode, 1 },
{ "String_::charCodeAt", (OpFun)(void*)_wrp_String___charCodeAt, 2 },
{ "String_::charAt", (OpFun)(void*)_wrp_String___charAt, 2 },
{ "String_::concat", (OpFun)(void*)_wrp_String___concat, 2 },
{ "String_::length", (OpFun)(void*)_wrp_String___length, 1 },
{ "String_::toNumber", (OpFun)(void*)_wrp_String___toNumber, 1 },
{ "String_::substr", (OpFun)(void*)_wrp_String___substr, 3 },
{ "String_::indexOf", (OpFun)(void*)_wrp_String___indexOf, 3 },
{ "String_::includes", (OpFun)(void*)_wrp_String___includes, 3 },
{ "Boolean_::bang", (OpFun)(void*)_wrp_Boolean___bang, 1 },
{ "numops::adds", (OpFun)(void*)_wrp_numops__adds, 2 },
{ "numops::subs", (OpFun)(void*)_wrp_numops__subs, 2 },
{ "numops::muls", (OpFun)(void*)_wrp_numops__muls, 2 },
{ "numops::div", (OpFun)(void*)_wrp_numops__div, 2 },
{ "numops::mod", (OpFun)(void*)_wrp_numops__mod, 2 },
{ "numops::lsls", (OpFun)(void*)_wrp_numops__lsls, 2 },
{ "numops::lsrs", (OpFun)(void*)_wrp_numops__lsrs, 2 },
{ "numops::asrs", (OpFun)(void*)_wrp_numops__asrs, 2 },
{ "numops::eors", (OpFun)(void*)_wrp_numops__eors, 2 },
{ "numops::orrs", (OpFun)(void*)_wrp_numops__orrs, 2 },
{ "numops::bnot", (OpFun)(void*)_wrp_numops__bnot, 1 },
{ "numops::ands", (OpFun)(void*)_wrp_numops__ands, 2 },
{ "numops::lt_bool", (OpFun)(void*)_wrp_numops__lt_bool, 2 },
{ "numops::le", (OpFun)(void*)_wrp_numops__le, 2 },
{ "numops::lt", (OpFun)(void*)_wrp_numops__lt, 2 },
{ "numops::ge", (OpFun)(void*)_wrp_numops__ge, 2 },
{ "numops::gt", (OpFun)(void*)_wrp_numops__gt, 2 },
{ "numops::eq", (OpFun)(void*)_wrp_numops__eq, 2 },
{ "numops::neq", (OpFun)(void*)_wrp_numops__neq, 2 },
{ "numops::eqq", (OpFun)(void*)_wrp_numops__eqq, 2 },
{ "numops::neqq", (OpFun)(void*)_wrp_numops__neqq, 2 },
{ "Math_::pow", (OpFun)(void*)_wrp_Math___pow, 2 },
{ "Math_::random", (OpFun)(void*)_wrp_Math___random, 0 },
{ "Math_::randomRange", (OpFun)(void*)_wrp_Math___randomRange, 2 },
{ "Math_::log", (OpFun)(void*)_wrp_Math___log, 1 },
{ "Math_::log10", (OpFun)(void*)_wrp_Math___log10, 1 },
{ "Math_::floor", (OpFun)(void*)_wrp_Math___floor, 1 },
{ "Math_::ceil", (OpFun)(void*)_wrp_Math___ceil, 1 },
{ "Math_::trunc", (OpFun)(void*)_wrp_Math___trunc, 1 },
{ "Math_::round", (OpFun)(void*)_wrp_Math___round, 1 },
{ "Math_::imul", (OpFun)(void*)_wrp_Math___imul, 2 },
{ "Math_::idiv", (OpFun)(void*)_wrp_Math___idiv, 2 },
{ "pxt::deepSleep", (OpFun)(void*)_wrp_pxt__deepSleep, 0 },
{ "pxt::getConfig", (OpFun)(void*)_wrp_pxt__getConfig, 2 },
{ "pxtrt::ldlocRef", (OpFun)(void*)_wrp_pxtrt__ldlocRef, 1 },
{ "pxtrt::stlocRef", (OpFun)(void*)_wrp_pxtrt__stlocRef, 2 },
{ "pxtrt::mklocRef", (OpFun)(void*)_wrp_pxtrt__mklocRef, 0 },
{ "pxtrt::stclo", (OpFun)(void*)_wrp_pxtrt__stclo, 3 },
{ "pxtrt::panic", (OpFun)(void*)_wrp_pxtrt__panic, 1 },
{ "pxt::typeOf", (OpFun)(void*)_wrp_pxt__typeOf, 1 },
{ "pxt::endTry", (OpFun)(void*)_wrp_pxt__endTry, 0 },
{ "pxt::throwValue", (OpFun)(void*)_wrp_pxt__throwValue, 1 },
{ "pxt::getThrownValue", (OpFun)(void*)_wrp_pxt__getThrownValue, 0 },
{ "pxt::endFinally", (OpFun)(void*)_wrp_pxt__endFinally, 0 },
{ "Math_::log2", (OpFun)(void*)_wrp_Math___log2, 1 },
{ "Math_::exp", (OpFun)(void*)_wrp_Math___exp, 1 },
{ "Math_::tanh", (OpFun)(void*)_wrp_Math___tanh, 1 },
{ "Math_::sinh", (OpFun)(void*)_wrp_Math___sinh, 1 },
{ "Math_::cosh", (OpFun)(void*)_wrp_Math___cosh, 1 },
{ "Math_::atanh", (OpFun)(void*)_wrp_Math___atanh, 1 },
{ "Math_::asinh", (OpFun)(void*)_wrp_Math___asinh, 1 },
{ "Math_::acosh", (OpFun)(void*)_wrp_Math___acosh, 1 },
{ "Math_::atan2", (OpFun)(void*)_wrp_Math___atan2, 2 },
{ "Math_::tan", (OpFun)(void*)_wrp_Math___tan, 1 },
{ "Math_::sin", (OpFun)(void*)_wrp_Math___sin, 1 },
{ "Math_::cos", (OpFun)(void*)_wrp_Math___cos, 1 },
{ "Math_::atan", (OpFun)(void*)_wrp_Math___atan, 1 },
{ "Math_::asin", (OpFun)(void*)_wrp_Math___asin, 1 },
{ "Math_::acos", (OpFun)(void*)_wrp_Math___acos, 1 },
{ "Math_::sqrt", (OpFun)(void*)_wrp_Math___sqrt, 1 },
{ "BufferMethods::getBytes", (OpFun)(void*)_wrp_BufferMethods__getBytes, 1 },
{ "BufferMethods::getByte", (OpFun)(void*)_wrp_BufferMethods__getByte, 2 },
{ "BufferMethods::setByte", (OpFun)(void*)_wrp_BufferMethods__setByte, 3 },
{ "BufferMethods::getUint8", (OpFun)(void*)_wrp_BufferMethods__getUint8, 2 },
{ "BufferMethods::isReadOnly", (OpFun)(void*)_wrp_BufferMethods__isReadOnly, 1 },
{ "BufferMethods::setUint8", (OpFun)(void*)_wrp_BufferMethods__setUint8, 3 },
{ "BufferMethods::setNumber", (OpFun)(void*)_wrp_BufferMethods__setNumber, 4 },
{ "BufferMethods::getNumber", (OpFun)(void*)_wrp_BufferMethods__getNumber, 3 },
{ "BufferMethods::length", (OpFun)(void*)_wrp_BufferMethods__length, 1 },
{ "BufferMethods::fill", (OpFun)(void*)_wrp_BufferMethods__fill, 4 },
{ "BufferMethods::slice", (OpFun)(void*)_wrp_BufferMethods__slice, 3 },
{ "BufferMethods::shift", (OpFun)(void*)_wrp_BufferMethods__shift, 4 },
{ "BufferMethods::toString", (OpFun)(void*)_wrp_BufferMethods__toString, 1 },
{ "BufferMethods::toHex", (OpFun)(void*)_wrp_BufferMethods__toHex, 1 },
{ "BufferMethods::rotate", (OpFun)(void*)_wrp_BufferMethods__rotate, 4 },
{ "BufferMethods::write", (OpFun)(void*)_wrp_BufferMethods__write, 3 },
{ "BufferMethods::hash", (OpFun)(void*)_wrp_BufferMethods__hash, 2 },
{ "control::createBuffer", (OpFun)(void*)_wrp_control__createBuffer, 1 },
{ "control::createBufferFromUTF8", (OpFun)(void*)_wrp_control__createBufferFromUTF8, 1 },
{ "loops::forever", (OpFun)(void*)_wrp_loops__forever, 1 },
{ "loops::pause", (OpFun)(void*)_wrp_loops__pause, 1 },
{ "control::millis", (OpFun)(void*)_wrp_control__millis, 0 },
{ "control::micros", (OpFun)(void*)_wrp_control__micros, 0 },
{ "control::internalOnEvent", (OpFun)(void*)_wrp_control__internalOnEvent, 4 },
{ "control::reset", (OpFun)(void*)_wrp_control__reset, 0 },
{ "control::waitMicros", (OpFun)(void*)_wrp_control__waitMicros, 1 },
{ "control::runInParallel", (OpFun)(void*)_wrp_control__runInParallel, 1 },
{ "control::waitForEvent", (OpFun)(void*)_wrp_control__waitForEvent, 2 },
{ "control::deviceSerialNumber", (OpFun)(void*)_wrp_control__deviceSerialNumber, 0 },
{ "control::deviceLongSerialNumber", (OpFun)(void*)_wrp_control__deviceLongSerialNumber, 0 },
{ "control::__log", (OpFun)(void*)_wrp_control____log, 2 },
{ "control::dmesgValue", (OpFun)(void*)_wrp_control__dmesgValue, 1 },
{ "control::gc", (OpFun)(void*)_wrp_control__gc, 0 },
{ "control::heapDump", (OpFun)(void*)_wrp_control__heapDump, 0 },
{ "control::setDebugFlags", (OpFun)(void*)_wrp_control__setDebugFlags, 1 },
{ "control::heapSnapshot", (OpFun)(void*)_wrp_control__heapSnapshot, 0 },
{ "control::profilingEnabled", (OpFun)(void*)_wrp_control__profilingEnabled, 0 },
{ "control::raiseEvent", (OpFun)(void*)_wrp_control__raiseEvent, 2 },
{ "control::allocateNotifyEvent", (OpFun)(void*)_wrp_control__allocateNotifyEvent, 0 },
{ "control::deviceDalVersion", (OpFun)(void*)_wrp_control__deviceDalVersion, 0 },
{ "control::dmesg", (OpFun)(void*)_wrp_control__dmesg, 1 },
{ "control::_ramSize", (OpFun)(void*)_wrp_control___ramSize, 0 },
{ "control::isUSBInitialized", (OpFun)(void*)_wrp_control__isUSBInitialized, 0 },
{ "serial::writeDmesg", (OpFun)(void*)_wrp_serial__writeDmesg, 0 },
{ "pxt::pressureLevelByButtonId", (OpFun)(void*)_wrp_pxt__pressureLevelByButtonId, 2 },
{ "pxt::setupButton", (OpFun)(void*)_wrp_pxt__setupButton, 2 },
{ "pxt::op_stloc", (OpFun)(void*)::pxt::op_stloc, 2 },
{ "pxt::op_ldloc", (OpFun)(void*)::pxt::op_ldloc, 2 },
{ "pxt::op_ldcap", (OpFun)(void*)::pxt::op_ldcap, 2 },
{ "pxt::op_bitconv", (OpFun)(void*)::pxt::op_bitconv, 2 },
{ "pxt::op_stglb", (OpFun)(void*)::pxt::op_stglb, 2 },
{ "pxt::op_ldglb", (OpFun)(void*)::pxt::op_ldglb, 2 },
{ "pxt::op_ldlit", (OpFun)(void*)::pxt::op_ldlit, 2 },
{ "pxt::op_ldnumber", (OpFun)(void*)::pxt::op_ldnumber, 2 },
{ "pxt::op_jmp", (OpFun)(void*)::pxt::op_jmp, 2 },
{ "pxt::op_jmpz", (OpFun)(void*)::pxt::op_jmpz, 2 },
{ "pxt::op_jmpnz", (OpFun)(void*)::pxt::op_jmpnz, 2 },
{ "pxt::op_newobj", (OpFun)(void*)::pxt::op_newobj, 2 },
{ "pxt::op_ldfld", (OpFun)(void*)::pxt::op_ldfld, 2 },
{ "pxt::op_stfld", (OpFun)(void*)::pxt::op_stfld, 2 },
{ "pxt::op_callproc", (OpFun)(void*)::pxt::op_callproc, 2 },
{ "pxt::op_callind", (OpFun)(void*)::pxt::op_callind, 2 },
{ "pxt::op_ret", (OpFun)(void*)::pxt::op_ret, 2 },
{ "pxt::op_pop", (OpFun)(void*)::pxt::op_pop, 2 },
{ "pxt::op_popmany", (OpFun)(void*)::pxt::op_popmany, 2 },
{ "pxt::op_pushmany", (OpFun)(void*)::pxt::op_pushmany, 2 },
{ "pxt::op_push", (OpFun)(void*)::pxt::op_push, 2 },
{ "pxt::op_ldspecial", (OpFun)(void*)::pxt::op_ldspecial, 2 },
{ "pxt::op_ldint", (OpFun)(void*)::pxt::op_ldint, 2 },
{ "pxt::op_ldintneg", (OpFun)(void*)::pxt::op_ldintneg, 2 },
{ "pxt::op_try", (OpFun)(void*)::pxt::op_try, 2 },
{ "pxt::op_calliface", (OpFun)(void*)::pxt::op_calliface, 2 },
{ "pxt::op_callget", (OpFun)(void*)::pxt::op_callget, 2 },
{ "pxt::op_callset", (OpFun)(void*)::pxt::op_callset, 2 },
{ "pxt::op_mapget", (OpFun)(void*)::pxt::op_mapget, 2 },
{ "pxt::op_mapset", (OpFun)(void*)::pxt::op_mapset, 2 },
{ "pxt::op_checkinst", (OpFun)(void*)::pxt::op_checkinst, 2 },
{ "vmcache::list", (OpFun)(void*)_wrp_vmcache__list, 0 },
{ "vmcache::run", (OpFun)(void*)_wrp_vmcache__run, 1 },
{ "vmcache::del", (OpFun)(void*)_wrp_vmcache__del, 1 },
{ "Keyboard::promptForText", (OpFun)(void*)_wrp_Keyboard__promptForText, 2 },
{ "Keyboard::cancelTextPrompt", (OpFun)(void*)_wrp_Keyboard__cancelTextPrompt, 0 },
{ "Keyboard::getTextPromptString", (OpFun)(void*)_wrp_Keyboard__getTextPromptString, 0 },
{ "Keyboard::getLocalizedInstructions", (OpFun)(void*)_wrp_Keyboard__getLocalizedInstructions, 0 },
{ "Keyboard::getTextPromptSelectionStart", (OpFun)(void*)_wrp_Keyboard__getTextPromptSelectionStart, 0 },
{ "Keyboard::getTextPromptSelectionEnd", (OpFun)(void*)_wrp_Keyboard__getTextPromptSelectionEnd, 0 },
{ "Keyboard::isSystemKeyboardSupported", (OpFun)(void*)_wrp_Keyboard__isSystemKeyboardSupported, 0 },
{ "pxt::pressureLevelByButtonId", (OpFun)(void*)_wrp_pxt__pressureLevelByButtonId, 2 },
{ "pxt::setupButton", (OpFun)(void*)_wrp_pxt__setupButton, 2 },
{ "multiplayer::postImage", (OpFun)(void*)_wrp_multiplayer__postImage, 1 },
{ "multiplayer::postIcon", (OpFun)(void*)_wrp_multiplayer__postIcon, 3 },
{ "multiplayer::setOrigin", (OpFun)(void*)_wrp_multiplayer__setOrigin, 1 },
{ "multiplayer::getCurrentImage", (OpFun)(void*)_wrp_multiplayer__getCurrentImage, 0 },
{ "multiplayer::getOrigin", (OpFun)(void*)_wrp_multiplayer__getOrigin, 0 },
{ "music::enableAmp", (OpFun)(void*)_wrp_music__enableAmp, 1 },
{ "music::forceOutput", (OpFun)(void*)_wrp_music__forceOutput, 1 },
{ "music::queuePlayInstructions", (OpFun)(void*)_wrp_music__queuePlayInstructions, 2 },
{ "music::stopPlaying", (OpFun)(void*)_wrp_music__stopPlaying, 0 },
{ "music::_createSequencer", (OpFun)(void*)_wrp_music___createSequencer, 0 },
{ "music::_sequencerState", (OpFun)(void*)_wrp_music___sequencerState, 1 },
{ "music::_sequencerCurrentTick", (OpFun)(void*)_wrp_music___sequencerCurrentTick, 1 },
{ "music::_sequencerPlaySong", (OpFun)(void*)_wrp_music___sequencerPlaySong, 3 },
{ "music::_sequencerStop", (OpFun)(void*)_wrp_music___sequencerStop, 1 },
{ "music::_sequencerSetVolume", (OpFun)(void*)_wrp_music___sequencerSetVolume, 2 },
{ "music::_sequencerSetVolumeForAll", (OpFun)(void*)_wrp_music___sequencerSetVolumeForAll, 1 },
{ "music::_sequencerSetTrackVolume", (OpFun)(void*)_wrp_music___sequencerSetTrackVolume, 3 },
{ "music::_sequencerSetDrumTrackVolume", (OpFun)(void*)_wrp_music___sequencerSetDrumTrackVolume, 4 },
{ "music::_sequencerDispose", (OpFun)(void*)_wrp_music___sequencerDispose, 1 },
{ "pxt::setScreenBrightnessSupported", (OpFun)(void*)_wrp_pxt__setScreenBrightnessSupported, 0 },
{ "pxt::setScreenBrightness", (OpFun)(void*)_wrp_pxt__setScreenBrightness, 1 },
{ "pxt::setPalette", (OpFun)(void*)_wrp_pxt__setPalette, 1 },
{ "pxt::updateScreen", (OpFun)(void*)_wrp_pxt__updateScreen, 1 },
{ "pxt::updateStats", (OpFun)(void*)_wrp_pxt__updateStats, 1 },
{ "ImageMethods::width", (OpFun)(void*)_wrp_ImageMethods__width, 1 },
{ "ImageMethods::height", (OpFun)(void*)_wrp_ImageMethods__height, 1 },
{ "ImageMethods::isMono", (OpFun)(void*)_wrp_ImageMethods__isMono, 1 },
{ "ImageMethods::isStatic", (OpFun)(void*)_wrp_ImageMethods__isStatic, 1 },
{ "ImageMethods::revision", (OpFun)(void*)_wrp_ImageMethods__revision, 1 },
{ "ImageMethods::copyFrom", (OpFun)(void*)_wrp_ImageMethods__copyFrom, 2 },
{ "ImageMethods::setPixel", (OpFun)(void*)_wrp_ImageMethods__setPixel, 4 },
{ "ImageMethods::getPixel", (OpFun)(void*)_wrp_ImageMethods__getPixel, 3 },
{ "ImageMethods::fill", (OpFun)(void*)_wrp_ImageMethods__fill, 2 },
{ "ImageMethods::getRows", (OpFun)(void*)_wrp_ImageMethods__getRows, 3 },
{ "ImageMethods::setRows", (OpFun)(void*)_wrp_ImageMethods__setRows, 3 },
{ "ImageMethods::_fillRect", (OpFun)(void*)_wrp_ImageMethods___fillRect, 4 },
{ "ImageMethods::_mapRect", (OpFun)(void*)_wrp_ImageMethods___mapRect, 4 },
{ "ImageMethods::equals", (OpFun)(void*)_wrp_ImageMethods__equals, 2 },
{ "ImageMethods::clone", (OpFun)(void*)_wrp_ImageMethods__clone, 1 },
{ "ImageMethods::flipX", (OpFun)(void*)_wrp_ImageMethods__flipX, 1 },
{ "ImageMethods::flipY", (OpFun)(void*)_wrp_ImageMethods__flipY, 1 },
{ "ImageMethods::transposed", (OpFun)(void*)_wrp_ImageMethods__transposed, 1 },
{ "ImageMethods::scroll", (OpFun)(void*)_wrp_ImageMethods__scroll, 3 },
{ "ImageMethods::doubledX", (OpFun)(void*)_wrp_ImageMethods__doubledX, 1 },
{ "ImageMethods::doubledY", (OpFun)(void*)_wrp_ImageMethods__doubledY, 1 },
{ "ImageMethods::replace", (OpFun)(void*)_wrp_ImageMethods__replace, 3 },
{ "ImageMethods::doubled", (OpFun)(void*)_wrp_ImageMethods__doubled, 1 },
{ "ImageMethods::drawImage", (OpFun)(void*)_wrp_ImageMethods__drawImage, 4 },
{ "ImageMethods::drawTransparentImage", (OpFun)(void*)_wrp_ImageMethods__drawTransparentImage, 4 },
{ "ImageMethods::overlapsWith", (OpFun)(void*)_wrp_ImageMethods__overlapsWith, 4 },
{ "ImageMethods::_drawIcon", (OpFun)(void*)_wrp_ImageMethods___drawIcon, 4 },
{ "ImageMethods::_drawLine", (OpFun)(void*)_wrp_ImageMethods___drawLine, 4 },
{ "ImageMethods::_blitRow", (OpFun)(void*)_wrp_ImageMethods___blitRow, 4 },
{ "ImageMethods::_drawScaledRotatedImage", (OpFun)(void*)_wrp_ImageMethods___drawScaledRotatedImage, 3 },
{ "ImageMethods::_checkOverlapsScaledRotatedImage", (OpFun)(void*)_wrp_ImageMethods___checkOverlapsScaledRotatedImage, 3 },
{ "ImageMethods::_checkOverlapsTwoScaledRotatedImages", (OpFun)(void*)_wrp_ImageMethods___checkOverlapsTwoScaledRotatedImages, 3 },
{ "ImageMethods::_blit", (OpFun)(void*)_wrp_ImageMethods___blit, 3 },
{ "ImageMethods::_fillCircle", (OpFun)(void*)_wrp_ImageMethods___fillCircle, 4 },
{ "ImageMethods::_fillTriangle", (OpFun)(void*)_wrp_ImageMethods___fillTriangle, 2 },
{ "ImageMethods::_fillPolygon4", (OpFun)(void*)_wrp_ImageMethods___fillPolygon4, 2 },
{ "image::create", (OpFun)(void*)_wrp_image__create, 2 },
{ "image::ofBuffer", (OpFun)(void*)_wrp_image__ofBuffer, 1 },
{ "image::doubledIcon", (OpFun)(void*)_wrp_image__doubledIcon, 1 },
{ "settings::_set", (OpFun)(void*)_wrp_settings___set, 2 },
{ "settings::_remove", (OpFun)(void*)_wrp_settings___remove, 1 },
{ "settings::_exists", (OpFun)(void*)_wrp_settings___exists, 1 },
{ "settings::_get", (OpFun)(void*)_wrp_settings___get, 1 },
{ "settings::_userClean", (OpFun)(void*)_wrp_settings___userClean, 0 },
{ "settings::_list", (OpFun)(void*)_wrp_settings___list, 1 },

PXT_SHIMS_END

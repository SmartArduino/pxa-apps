(() => {
  var __defProp = Object.defineProperty;
  var __typeError = (msg) => {
    throw TypeError(msg);
  };
  var __defNormalProp = (obj, key, value) => key in obj ? __defProp(obj, key, { enumerable: true, configurable: true, writable: true, value }) : obj[key] = value;
  var __publicField = (obj, key, value) => __defNormalProp(obj, typeof key !== "symbol" ? key + "" : key, value);
  var __accessCheck = (obj, member, msg) => member.has(obj) || __typeError("Cannot " + msg);
  var __privateGet = (obj, member, getter) => (__accessCheck(obj, member, "read from private field"), getter ? getter.call(obj) : member.get(obj));
  var __privateAdd = (obj, member, value) => member.has(obj) ? __typeError("Cannot add the same private member more than once") : member instanceof WeakSet ? member.add(obj) : member.set(obj, value);
  var __privateSet = (obj, member, value, setter) => (__accessCheck(obj, member, "write to private field"), setter ? setter.call(obj, value) : member.set(obj, value), value);

  // tools/node_modules/solid-js/dist/solid.js
  var equalFn = (a3, b) => a3 === b;
  var $PROXY = Symbol("solid-proxy");
  var $TRACK = Symbol("solid-track");
  var $DEVCOMP = Symbol("solid-dev-component");
  var signalOptions = {
    equals: equalFn
  };
  var ERROR = null;
  var runEffects = runQueue;
  var STALE = 1;
  var PENDING = 2;
  var UNOWNED = {
    owned: null,
    cleanups: null,
    context: null,
    owner: null
  };
  var Owner = null;
  var Transition = null;
  var Scheduler = null;
  var ExternalSourceConfig = null;
  var Listener = null;
  var Updates = null;
  var Effects = null;
  var ExecCount = 0;
  function createSignal(value, options) {
    options = options ? Object.assign({}, signalOptions, options) : signalOptions;
    const s3 = {
      value,
      observers: null,
      observerSlots: null,
      comparator: options.equals || void 0
    };
    const setter = (value2) => {
      if (typeof value2 === "function") {
        if (Transition && Transition.running && Transition.sources.has(s3)) value2 = value2(s3.tValue);
        else value2 = value2(s3.value);
      }
      return writeSignal(s3, value2);
    };
    return [readSignal.bind(s3), setter];
  }
  function createRenderEffect(fn, value, options) {
    const c3 = createComputation(fn, value, false, STALE);
    if (Scheduler && Transition && Transition.running) Updates.push(c3);
    else updateComputation(c3);
  }
  function createMemo(fn, value, options) {
    options = options ? Object.assign({}, signalOptions, options) : signalOptions;
    const c3 = createComputation(fn, value, true, 0);
    c3.observers = null;
    c3.observerSlots = null;
    c3.comparator = options.equals || void 0;
    if (Scheduler && Transition && Transition.running) {
      c3.tState = STALE;
      Updates.push(c3);
    } else updateComputation(c3);
    return readSignal.bind(c3);
  }
  function batch(fn) {
    return runUpdates(fn, false);
  }
  function untrack(fn) {
    if (!ExternalSourceConfig && Listener === null) return fn();
    const listener = Listener;
    Listener = null;
    try {
      if (ExternalSourceConfig) return ExternalSourceConfig.untrack(fn);
      return fn();
    } finally {
      Listener = listener;
    }
  }
  function onCleanup(fn) {
    if (Owner === null) ;
    else if (Owner.cleanups === null) Owner.cleanups = [fn];
    else Owner.cleanups.push(fn);
    return fn;
  }
  function getListener() {
    return Listener;
  }
  function startTransition(fn) {
    if (Transition && Transition.running) {
      fn();
      return Transition.done;
    }
    const l = Listener;
    const o = Owner;
    return Promise.resolve().then(() => {
      Listener = l;
      Owner = o;
      let t3;
      if (Scheduler || SuspenseContext) {
        t3 = Transition || (Transition = {
          sources: /* @__PURE__ */ new Set(),
          effects: [],
          promises: /* @__PURE__ */ new Set(),
          disposed: /* @__PURE__ */ new Set(),
          queue: /* @__PURE__ */ new Set(),
          running: true
        });
        t3.done || (t3.done = new Promise((res) => t3.resolve = res));
        t3.running = true;
      }
      runUpdates(fn, false);
      Listener = Owner = null;
      return t3 ? t3.done : void 0;
    });
  }
  var [transPending, setTransPending] = /* @__PURE__ */ createSignal(false);
  function createContext(defaultValue, options) {
    const id = Symbol("context");
    return {
      id,
      Provider: createProvider(id),
      defaultValue
    };
  }
  function children(fn) {
    const children2 = createMemo(fn);
    const memo = createMemo(() => resolveChildren(children2()));
    memo.toArray = () => {
      const c3 = memo();
      return Array.isArray(c3) ? c3 : c3 != null ? [c3] : [];
    };
    return memo;
  }
  var SuspenseContext;
  function readSignal() {
    const runningTransition = Transition && Transition.running;
    if (this.sources && (runningTransition ? this.tState : this.state)) {
      if ((runningTransition ? this.tState : this.state) === STALE) updateComputation(this);
      else {
        const updates = Updates;
        Updates = null;
        runUpdates(() => lookUpstream(this), false);
        Updates = updates;
      }
    }
    if (Listener) {
      const sSlot = this.observers ? this.observers.length : 0;
      if (!Listener.sources) {
        Listener.sources = [this];
        Listener.sourceSlots = [sSlot];
      } else {
        Listener.sources.push(this);
        Listener.sourceSlots.push(sSlot);
      }
      if (!this.observers) {
        this.observers = [Listener];
        this.observerSlots = [Listener.sources.length - 1];
      } else {
        this.observers.push(Listener);
        this.observerSlots.push(Listener.sources.length - 1);
      }
    }
    if (runningTransition && Transition.sources.has(this)) return this.tValue;
    return this.value;
  }
  function writeSignal(node, value, isComp) {
    let current = Transition && Transition.running && Transition.sources.has(node) ? node.tValue : node.value;
    if (!node.comparator || !node.comparator(current, value)) {
      if (Transition) {
        const TransitionRunning = Transition.running;
        if (TransitionRunning || !isComp && Transition.sources.has(node)) {
          Transition.sources.add(node);
          node.tValue = value;
        }
        if (!TransitionRunning) node.value = value;
      } else node.value = value;
      if (node.observers && node.observers.length) {
        runUpdates(() => {
          for (let i2 = 0; i2 < node.observers.length; i2 += 1) {
            const o = node.observers[i2];
            const TransitionRunning = Transition && Transition.running;
            if (TransitionRunning && Transition.disposed.has(o)) continue;
            if (TransitionRunning ? !o.tState : !o.state) {
              if (o.pure) Updates.push(o);
              else Effects.push(o);
              if (o.observers) markDownstream(o);
            }
            if (!TransitionRunning) o.state = STALE;
            else o.tState = STALE;
          }
          if (Updates.length > 1e6) {
            Updates = [];
            if (false) ;
            throw new Error();
          }
        }, false);
      }
    }
    return value;
  }
  function updateComputation(node) {
    if (!node.fn) return;
    cleanNode(node);
    const time = ExecCount;
    runComputation(
      node,
      Transition && Transition.running && Transition.sources.has(node) ? node.tValue : node.value,
      time
    );
    if (Transition && !Transition.running && Transition.sources.has(node)) {
      queueMicrotask(() => {
        runUpdates(() => {
          Transition && (Transition.running = true);
          Listener = Owner = node;
          runComputation(node, node.tValue, time);
          Listener = Owner = null;
        }, false);
      });
    }
  }
  function runComputation(node, value, time) {
    let nextValue;
    const owner = Owner, listener = Listener;
    Listener = Owner = node;
    try {
      nextValue = node.fn(value);
    } catch (err) {
      if (node.pure) {
        if (Transition && Transition.running) {
          node.tState = STALE;
          node.tOwned && node.tOwned.forEach(cleanNode);
          node.tOwned = void 0;
        } else {
          node.state = STALE;
          node.owned && node.owned.forEach(cleanNode);
          node.owned = null;
        }
      }
      node.updatedAt = time + 1;
      return handleError(err);
    } finally {
      Listener = listener;
      Owner = owner;
    }
    if (!node.updatedAt || node.updatedAt <= time) {
      if (node.updatedAt != null && "observers" in node) {
        writeSignal(node, nextValue, true);
      } else if (Transition && Transition.running && node.pure) {
        Transition.sources.add(node);
        node.tValue = nextValue;
      } else node.value = nextValue;
      node.updatedAt = time;
    }
  }
  function createComputation(fn, init, pure, state2 = STALE, options) {
    const c3 = {
      fn,
      state: state2,
      updatedAt: null,
      owned: null,
      sources: null,
      sourceSlots: null,
      cleanups: null,
      value: init,
      owner: Owner,
      context: Owner ? Owner.context : null,
      pure
    };
    if (Transition && Transition.running) {
      c3.state = 0;
      c3.tState = state2;
    }
    if (Owner === null) ;
    else if (Owner !== UNOWNED) {
      if (Transition && Transition.running && Owner.pure) {
        if (!Owner.tOwned) Owner.tOwned = [c3];
        else Owner.tOwned.push(c3);
      } else {
        if (!Owner.owned) Owner.owned = [c3];
        else Owner.owned.push(c3);
      }
    }
    if (ExternalSourceConfig && c3.fn) {
      const [track, trigger] = createSignal(void 0, {
        equals: false
      });
      const ordinary = ExternalSourceConfig.factory(c3.fn, trigger);
      onCleanup(() => ordinary.dispose());
      const triggerInTransition = () => startTransition(trigger).then(() => inTransition.dispose());
      const inTransition = ExternalSourceConfig.factory(c3.fn, triggerInTransition);
      c3.fn = (x) => {
        track();
        return Transition && Transition.running ? inTransition.track(x) : ordinary.track(x);
      };
    }
    return c3;
  }
  function runTop(node) {
    const runningTransition = Transition && Transition.running;
    if ((runningTransition ? node.tState : node.state) === 0) return;
    if ((runningTransition ? node.tState : node.state) === PENDING) return lookUpstream(node);
    if (node.suspense && untrack(node.suspense.inFallback)) return node.suspense.effects.push(node);
    const ancestors = [node];
    while ((node = node.owner) && (!node.updatedAt || node.updatedAt < ExecCount)) {
      if (runningTransition && Transition.disposed.has(node)) return;
      if (runningTransition ? node.tState : node.state) ancestors.push(node);
    }
    for (let i2 = ancestors.length - 1; i2 >= 0; i2--) {
      node = ancestors[i2];
      if (runningTransition) {
        let top = node, prev = ancestors[i2 + 1];
        while ((top = top.owner) && top !== prev) {
          if (Transition.disposed.has(top)) return;
        }
      }
      if ((runningTransition ? node.tState : node.state) === STALE) {
        updateComputation(node);
      } else if ((runningTransition ? node.tState : node.state) === PENDING) {
        const updates = Updates;
        Updates = null;
        runUpdates(() => lookUpstream(node, ancestors[0]), false);
        Updates = updates;
      }
    }
  }
  function runUpdates(fn, init) {
    if (Updates) return fn();
    let wait = false;
    if (!init) Updates = [];
    if (Effects) wait = true;
    else Effects = [];
    ExecCount++;
    try {
      const res = fn();
      completeUpdates(wait);
      return res;
    } catch (err) {
      if (!wait) Effects = null;
      Updates = null;
      handleError(err);
    }
  }
  function completeUpdates(wait) {
    if (Updates) {
      if (Scheduler && Transition && Transition.running) scheduleQueue(Updates);
      else runQueue(Updates);
      Updates = null;
    }
    if (wait) return;
    let res;
    if (Transition) {
      if (!Transition.promises.size && !Transition.queue.size) {
        const sources = Transition.sources;
        const disposed = Transition.disposed;
        Effects.push.apply(Effects, Transition.effects);
        res = Transition.resolve;
        for (const e3 of Effects) {
          "tState" in e3 && (e3.state = e3.tState);
          delete e3.tState;
        }
        Transition = null;
        runUpdates(() => {
          for (const d2 of disposed) cleanNode(d2);
          for (const v of sources) {
            v.value = v.tValue;
            if (v.owned) {
              for (let i2 = 0, len = v.owned.length; i2 < len; i2++) cleanNode(v.owned[i2]);
            }
            if (v.tOwned) v.owned = v.tOwned;
            delete v.tValue;
            delete v.tOwned;
            v.tState = 0;
          }
          setTransPending(false);
        }, false);
      } else if (Transition.running) {
        Transition.running = false;
        Transition.effects.push.apply(Transition.effects, Effects);
        Effects = null;
        setTransPending(true);
        return;
      }
    }
    const e2 = Effects;
    Effects = null;
    if (e2.length) runUpdates(() => runEffects(e2), false);
    if (res) res();
  }
  function runQueue(queue) {
    for (let i2 = 0; i2 < queue.length; i2++) runTop(queue[i2]);
  }
  function scheduleQueue(queue) {
    for (let i2 = 0; i2 < queue.length; i2++) {
      const item = queue[i2];
      const tasks = Transition.queue;
      if (!tasks.has(item)) {
        tasks.add(item);
        Scheduler(() => {
          tasks.delete(item);
          runUpdates(() => {
            Transition.running = true;
            runTop(item);
          }, false);
          Transition && (Transition.running = false);
        });
      }
    }
  }
  function lookUpstream(node, ignore) {
    const runningTransition = Transition && Transition.running;
    if (runningTransition) node.tState = 0;
    else node.state = 0;
    for (let i2 = 0; i2 < node.sources.length; i2 += 1) {
      const source = node.sources[i2];
      if (source.sources) {
        const state2 = runningTransition ? source.tState : source.state;
        if (state2 === STALE) {
          if (source !== ignore && (!source.updatedAt || source.updatedAt < ExecCount))
            runTop(source);
        } else if (state2 === PENDING) lookUpstream(source, ignore);
      }
    }
  }
  function markDownstream(node) {
    const runningTransition = Transition && Transition.running;
    for (let i2 = 0; i2 < node.observers.length; i2 += 1) {
      const o = node.observers[i2];
      if (runningTransition ? !o.tState : !o.state) {
        if (runningTransition) o.tState = PENDING;
        else o.state = PENDING;
        if (o.pure) Updates.push(o);
        else Effects.push(o);
        o.observers && markDownstream(o);
      }
    }
  }
  function cleanNode(node) {
    let i2;
    if (node.sources) {
      while (node.sources.length) {
        const source = node.sources.pop(), index = node.sourceSlots.pop(), obs = source.observers;
        if (obs && obs.length) {
          const n2 = obs.pop(), s3 = source.observerSlots.pop();
          if (index < obs.length) {
            n2.sourceSlots[s3] = index;
            obs[index] = n2;
            source.observerSlots[index] = s3;
          }
        }
      }
    }
    if (node.tOwned) {
      for (i2 = node.tOwned.length - 1; i2 >= 0; i2--) cleanNode(node.tOwned[i2]);
      delete node.tOwned;
    }
    if (Transition && Transition.running && node.pure) {
      reset(node, true);
    } else if (node.owned) {
      for (i2 = node.owned.length - 1; i2 >= 0; i2--) cleanNode(node.owned[i2]);
      node.owned = null;
    }
    if (node.cleanups) {
      for (i2 = node.cleanups.length - 1; i2 >= 0; i2--) node.cleanups[i2]();
      node.cleanups = null;
    }
    if (Transition && Transition.running) node.tState = 0;
    else node.state = 0;
  }
  function reset(node, top) {
    if (!top) {
      node.tState = 0;
      Transition.disposed.add(node);
    }
    if (node.owned) {
      for (let i2 = 0; i2 < node.owned.length; i2++) reset(node.owned[i2]);
    }
  }
  function castError(err) {
    if (err instanceof Error) return err;
    return new Error(typeof err === "string" ? err : "Unknown error", {
      cause: err
    });
  }
  function runErrors(err, fns, owner) {
    try {
      for (const f of fns) f(err);
    } catch (e2) {
      handleError(e2, owner && owner.owner || null);
    }
  }
  function handleError(err, owner = Owner) {
    const fns = ERROR && owner && owner.context && owner.context[ERROR];
    const error = castError(err);
    if (!fns) throw error;
    if (Effects)
      Effects.push({
        fn() {
          runErrors(error, fns, owner);
        },
        state: STALE
      });
    else runErrors(error, fns, owner);
  }
  function resolveChildren(children2) {
    if (typeof children2 === "function" && !children2.length) return resolveChildren(children2());
    if (Array.isArray(children2)) {
      const results = [];
      for (let i2 = 0; i2 < children2.length; i2++) {
        const result = resolveChildren(children2[i2]);
        Array.isArray(result) ? results.push.apply(results, result) : results.push(result);
      }
      return results;
    }
    return children2;
  }
  function createProvider(id, options) {
    return function provider(props) {
      let res;
      createRenderEffect(
        () => res = untrack(() => {
          Owner.context = {
            ...Owner.context,
            [id]: props.value
          };
          return children(() => props.children);
        }),
        void 0
      );
      return res;
    };
  }
  var FALLBACK = Symbol("fallback");
  var DEV = void 0;

  // tools/node_modules/solid-js/web/dist/web.js
  var booleans = [
    "allowfullscreen",
    "async",
    "autofocus",
    "autoplay",
    "checked",
    "controls",
    "default",
    "disabled",
    "formnovalidate",
    "hidden",
    "indeterminate",
    "inert",
    "ismap",
    "loop",
    "multiple",
    "muted",
    "nomodule",
    "novalidate",
    "open",
    "playsinline",
    "readonly",
    "required",
    "reversed",
    "seamless",
    "selected"
  ];
  var Properties = /* @__PURE__ */ new Set([
    "className",
    "value",
    "readOnly",
    "formNoValidate",
    "isMap",
    "noModule",
    "playsInline",
    ...booleans
  ]);
  var RequestContext = Symbol();
  var isServer = false;

  // tools/node_modules/@solid-primitives/trigger/dist/index.js
  var triggerOptions = !isServer && DEV ? { equals: false, name: "trigger" } : { equals: false };
  var triggerCacheOptions = !isServer && DEV ? { equals: false, internal: true } : triggerOptions;
  var _map;
  var TriggerCache = class {
    constructor(mapConstructor = Map) {
      __privateAdd(this, _map);
      __privateSet(this, _map, new mapConstructor());
    }
    dirty(key) {
      if (isServer)
        return;
      __privateGet(this, _map).get(key)?.$$();
    }
    dirtyAll() {
      if (isServer)
        return;
      for (const trigger of __privateGet(this, _map).values())
        trigger.$$();
    }
    track(key) {
      if (!getListener())
        return;
      let trigger = __privateGet(this, _map).get(key);
      if (!trigger) {
        const [$, $$] = createSignal(void 0, triggerCacheOptions);
        __privateGet(this, _map).set(key, trigger = { $, $$, n: 1 });
      } else
        trigger.n++;
      onCleanup(() => {
        if (--trigger.n === 0)
          queueMicrotask(() => trigger.n === 0 && __privateGet(this, _map).delete(key));
      });
      trigger.$();
    }
  };
  _map = new WeakMap();

  // tools/node_modules/@solid-primitives/map/dist/index.js
  var $OBJECT = Symbol("track-object");
  var _keyTriggers, _valueTriggers;
  var ReactiveMap = class extends Map {
    constructor(entries) {
      super();
      __privateAdd(this, _keyTriggers, new TriggerCache());
      __privateAdd(this, _valueTriggers, new TriggerCache());
      if (entries)
        for (const entry of entries)
          super.set(...entry);
    }
    [Symbol.iterator]() {
      return this.entries();
    }
    get size() {
      __privateGet(this, _keyTriggers).track($OBJECT);
      return super.size;
    }
    *keys() {
      __privateGet(this, _keyTriggers).track($OBJECT);
      for (const key of super.keys()) {
        yield key;
      }
    }
    *values() {
      __privateGet(this, _valueTriggers).track($OBJECT);
      for (const value of super.values()) {
        yield value;
      }
    }
    *entries() {
      __privateGet(this, _keyTriggers).track($OBJECT);
      __privateGet(this, _valueTriggers).track($OBJECT);
      for (const entry of super.entries()) {
        yield entry;
      }
    }
    forEach(callbackfn, thisArg) {
      __privateGet(this, _keyTriggers).track($OBJECT);
      __privateGet(this, _valueTriggers).track($OBJECT);
      super.forEach(callbackfn, thisArg);
    }
    has(key) {
      __privateGet(this, _keyTriggers).track(key);
      return super.has(key);
    }
    get(key) {
      __privateGet(this, _valueTriggers).track(key);
      return super.get(key);
    }
    set(key, value) {
      const hadNoKey = !super.has(key);
      const hasChanged = super.get(key) !== value;
      const result = super.set(key, value);
      if (hasChanged || hadNoKey) {
        batch(() => {
          if (hadNoKey) {
            __privateGet(this, _keyTriggers).dirty($OBJECT);
            __privateGet(this, _keyTriggers).dirty(key);
          }
          if (hasChanged) {
            __privateGet(this, _valueTriggers).dirty($OBJECT);
            __privateGet(this, _valueTriggers).dirty(key);
          }
        });
      }
      return result;
    }
    delete(key) {
      const isDefined = super.get(key) !== void 0;
      const result = super.delete(key);
      if (result) {
        batch(() => {
          __privateGet(this, _keyTriggers).dirty($OBJECT);
          __privateGet(this, _valueTriggers).dirty($OBJECT);
          __privateGet(this, _keyTriggers).dirty(key);
          if (isDefined) {
            __privateGet(this, _valueTriggers).dirty(key);
          }
        });
      }
      return result;
    }
    clear() {
      if (super.size) {
        super.clear();
        batch(() => {
          __privateGet(this, _keyTriggers).dirtyAll();
          __privateGet(this, _valueTriggers).dirtyAll();
        });
      }
    }
  };
  _keyTriggers = new WeakMap();
  _valueTriggers = new WeakMap();

  // tools/node_modules/@solid-primitives/set/dist/index.js
  var $KEYS = Symbol("track-keys");
  var _triggers;
  var ReactiveSet = class extends Set {
    constructor(values) {
      super();
      __privateAdd(this, _triggers, new TriggerCache());
      if (values)
        for (const value of values)
          super.add(value);
    }
    [Symbol.iterator]() {
      return this.values();
    }
    get size() {
      __privateGet(this, _triggers).track($KEYS);
      return super.size;
    }
    has(value) {
      __privateGet(this, _triggers).track(value);
      return super.has(value);
    }
    keys() {
      return this.values();
    }
    *values() {
      __privateGet(this, _triggers).track($KEYS);
      for (const value of super.values()) {
        yield value;
      }
    }
    *entries() {
      __privateGet(this, _triggers).track($KEYS);
      for (const entry of super.entries()) {
        yield entry;
      }
    }
    forEach(callbackfn, thisArg) {
      __privateGet(this, _triggers).track($KEYS);
      super.forEach(callbackfn, thisArg);
    }
    add(value) {
      if (!super.has(value)) {
        super.add(value);
        batch(() => {
          __privateGet(this, _triggers).dirty(value);
          __privateGet(this, _triggers).dirty($KEYS);
        });
      }
      return this;
    }
    delete(value) {
      const result = super.delete(value);
      if (result) {
        batch(() => {
          __privateGet(this, _triggers).dirty(value);
          __privateGet(this, _triggers).dirty($KEYS);
        });
      }
      return result;
    }
    clear() {
      if (super.size) {
        super.clear();
        batch(() => {
          __privateGet(this, _triggers).dirtyAll();
        });
      }
    }
  };
  _triggers = new WeakMap();

  // tools/node_modules/solid-js/store/dist/store.js
  var $RAW = Symbol("store-raw");
  var $NODE = Symbol("store-node");
  var $HAS = Symbol("store-has");
  var $SELF = Symbol("store-self");
  function wrap$1(value) {
    let p = value[$PROXY];
    if (!p) {
      Object.defineProperty(value, $PROXY, {
        value: p = new Proxy(value, proxyTraps$1)
      });
      if (!Array.isArray(value)) {
        const keys = Object.keys(value), desc = Object.getOwnPropertyDescriptors(value);
        for (let i2 = 0, l = keys.length; i2 < l; i2++) {
          const prop = keys[i2];
          if (desc[prop].get) {
            Object.defineProperty(value, prop, {
              enumerable: desc[prop].enumerable,
              get: desc[prop].get.bind(p)
            });
          }
        }
      }
    }
    return p;
  }
  function isWrappable(obj) {
    let proto;
    return obj != null && typeof obj === "object" && (obj[$PROXY] || !(proto = Object.getPrototypeOf(obj)) || proto === Object.prototype || Array.isArray(obj));
  }
  function unwrap(item, set = /* @__PURE__ */ new Set()) {
    let result, unwrapped, v, prop;
    if (result = item != null && item[$RAW]) return result;
    if (!isWrappable(item) || set.has(item)) return item;
    if (Array.isArray(item)) {
      if (Object.isFrozen(item)) item = item.slice(0);
      else set.add(item);
      for (let i2 = 0, l = item.length; i2 < l; i2++) {
        v = item[i2];
        if ((unwrapped = unwrap(v, set)) !== v) item[i2] = unwrapped;
      }
    } else {
      if (Object.isFrozen(item)) item = Object.assign({}, item);
      else set.add(item);
      const keys = Object.keys(item), desc = Object.getOwnPropertyDescriptors(item);
      for (let i2 = 0, l = keys.length; i2 < l; i2++) {
        prop = keys[i2];
        if (desc[prop].get) continue;
        v = item[prop];
        if ((unwrapped = unwrap(v, set)) !== v) item[prop] = unwrapped;
      }
    }
    return item;
  }
  function getNodes(target, symbol) {
    let nodes = target[symbol];
    if (!nodes)
      Object.defineProperty(target, symbol, {
        value: nodes = /* @__PURE__ */ Object.create(null)
      });
    return nodes;
  }
  function getNode(nodes, property, value) {
    if (nodes[property]) return nodes[property];
    const [s3, set] = createSignal(value, {
      equals: false,
      internal: true
    });
    s3.$ = set;
    return nodes[property] = s3;
  }
  function proxyDescriptor$1(target, property) {
    const desc = Reflect.getOwnPropertyDescriptor(target, property);
    if (!desc || desc.get || !desc.configurable || property === $PROXY || property === $NODE)
      return desc;
    delete desc.value;
    delete desc.writable;
    desc.get = () => target[$PROXY][property];
    return desc;
  }
  function trackSelf(target) {
    getListener() && getNode(getNodes(target, $NODE), $SELF)();
  }
  function ownKeys(target) {
    trackSelf(target);
    return Reflect.ownKeys(target);
  }
  var proxyTraps$1 = {
    get(target, property, receiver) {
      if (property === $RAW) return target;
      if (property === $PROXY) return receiver;
      if (property === $TRACK) {
        trackSelf(target);
        return receiver;
      }
      const nodes = getNodes(target, $NODE);
      const tracked = nodes[property];
      let value = tracked ? tracked() : target[property];
      if (property === $NODE || property === $HAS || property === "__proto__") return value;
      if (!tracked) {
        const desc = Object.getOwnPropertyDescriptor(target, property);
        if (getListener() && (typeof value !== "function" || target.hasOwnProperty(property)) && !(desc && desc.get))
          value = getNode(nodes, property, value)();
      }
      return isWrappable(value) ? wrap$1(value) : value;
    },
    has(target, property) {
      if (property === $RAW || property === $PROXY || property === $TRACK || property === $NODE || property === $HAS || property === "__proto__")
        return true;
      getListener() && getNode(getNodes(target, $HAS), property)();
      return property in target;
    },
    set() {
      return true;
    },
    deleteProperty() {
      return true;
    },
    ownKeys,
    getOwnPropertyDescriptor: proxyDescriptor$1
  };
  function setProperty(state2, property, value, deleting = false) {
    if (!deleting && state2[property] === value) return;
    const prev = state2[property], len = state2.length;
    if (value === void 0) {
      delete state2[property];
      if (state2[$HAS] && state2[$HAS][property] && prev !== void 0) state2[$HAS][property].$();
    } else {
      state2[property] = value;
      if (state2[$HAS] && state2[$HAS][property] && prev === void 0) state2[$HAS][property].$();
    }
    let nodes = getNodes(state2, $NODE), node;
    if (node = getNode(nodes, property, prev)) node.$(() => value);
    if (Array.isArray(state2) && state2.length !== len) {
      for (let i2 = state2.length; i2 < len; i2++) (node = nodes[i2]) && node.$();
      (node = getNode(nodes, "length", len)) && node.$(state2.length);
    }
    (node = nodes[$SELF]) && node.$();
  }
  function mergeStoreNode(state2, value) {
    const keys = Object.keys(value);
    for (let i2 = 0; i2 < keys.length; i2 += 1) {
      const key = keys[i2];
      setProperty(state2, key, value[key]);
    }
  }
  function updateArray(current, next) {
    if (typeof next === "function") next = next(current);
    next = unwrap(next);
    if (Array.isArray(next)) {
      if (current === next) return;
      let i2 = 0, len = next.length;
      for (; i2 < len; i2++) {
        const value = next[i2];
        if (current[i2] !== value) setProperty(current, i2, value);
      }
      setProperty(current, "length", len);
    } else mergeStoreNode(current, next);
  }
  function updatePath(current, path, traversed = []) {
    let part, prev = current;
    if (path.length > 1) {
      part = path.shift();
      const partType = typeof part, isArray = Array.isArray(current);
      if (Array.isArray(part)) {
        for (let i2 = 0; i2 < part.length; i2++) {
          updatePath(current, [part[i2]].concat(path), traversed);
        }
        return;
      } else if (isArray && partType === "function") {
        for (let i2 = 0; i2 < current.length; i2++) {
          if (part(current[i2], i2)) updatePath(current, [i2].concat(path), traversed);
        }
        return;
      } else if (isArray && partType === "object") {
        const { from = 0, to = current.length - 1, by = 1 } = part;
        for (let i2 = from; i2 <= to; i2 += by) {
          updatePath(current, [i2].concat(path), traversed);
        }
        return;
      } else if (path.length > 1) {
        updatePath(current[part], path, [part].concat(traversed));
        return;
      }
      prev = current[part];
      traversed = [part].concat(traversed);
    }
    let value = path[0];
    if (typeof value === "function") {
      value = value(prev, traversed);
      if (value === prev) return;
    }
    if (part === void 0 && value == void 0) return;
    value = unwrap(value);
    if (part === void 0 || isWrappable(prev) && isWrappable(value) && !Array.isArray(value)) {
      mergeStoreNode(prev, value);
    } else setProperty(current, part, value);
  }
  function createStore(...[store, options]) {
    const unwrappedStore = unwrap(store || {});
    const isArray = Array.isArray(unwrappedStore);
    const wrappedStore = wrap$1(unwrappedStore);
    function setStore(...args) {
      batch(() => {
        isArray && args.length === 1 ? updateArray(unwrappedStore, args[0]) : updatePath(unwrappedStore, args);
      });
    }
    return [wrappedStore, setStore];
  }
  var $ROOT = Symbol("store-root");
  function applyState(target, parent, property, merge, key) {
    const previous = parent[property];
    if (target === previous) return;
    const isArray = Array.isArray(target);
    if (property !== $ROOT && (!isWrappable(target) || !isWrappable(previous) || isArray !== Array.isArray(previous) || key && target[key] !== previous[key])) {
      setProperty(parent, property, target);
      return;
    }
    if (isArray) {
      if (target.length && previous.length && (!merge || key && target[0] && target[0][key] != null)) {
        let i2, j, start, end, newEnd, item, newIndicesNext, keyVal;
        for (start = 0, end = Math.min(previous.length, target.length); start < end && (previous[start] === target[start] || key && previous[start] && target[start] && previous[start][key] === target[start][key]); start++) {
          applyState(target[start], previous, start, merge, key);
        }
        const temp = new Array(target.length), newIndices = /* @__PURE__ */ new Map();
        for (end = previous.length - 1, newEnd = target.length - 1; end >= start && newEnd >= start && (previous[end] === target[newEnd] || key && previous[end] && target[newEnd] && previous[end][key] === target[newEnd][key]); end--, newEnd--) {
          temp[newEnd] = previous[end];
        }
        if (start > newEnd || start > end) {
          for (j = start; j <= newEnd; j++) setProperty(previous, j, target[j]);
          for (; j < target.length; j++) {
            setProperty(previous, j, temp[j]);
            applyState(target[j], previous, j, merge, key);
          }
          if (previous.length > target.length) setProperty(previous, "length", target.length);
          return;
        }
        newIndicesNext = new Array(newEnd + 1);
        for (j = newEnd; j >= start; j--) {
          item = target[j];
          keyVal = key && item ? item[key] : item;
          i2 = newIndices.get(keyVal);
          newIndicesNext[j] = i2 === void 0 ? -1 : i2;
          newIndices.set(keyVal, j);
        }
        for (i2 = start; i2 <= end; i2++) {
          item = previous[i2];
          keyVal = key && item ? item[key] : item;
          j = newIndices.get(keyVal);
          if (j !== void 0 && j !== -1) {
            temp[j] = previous[i2];
            j = newIndicesNext[j];
            newIndices.set(keyVal, j);
          }
        }
        for (j = start; j < target.length; j++) {
          if (j in temp) {
            setProperty(previous, j, temp[j]);
            applyState(target[j], previous, j, merge, key);
          } else setProperty(previous, j, target[j]);
        }
      } else {
        for (let i2 = 0, len = target.length; i2 < len; i2++) {
          applyState(target[i2], previous, i2, merge, key);
        }
      }
      if (previous.length > target.length) setProperty(previous, "length", target.length);
      return;
    }
    const targetKeys = Object.keys(target);
    for (let i2 = 0, len = targetKeys.length; i2 < len; i2++) {
      applyState(target[targetKeys[i2]], previous, targetKeys[i2], merge, key);
    }
    const previousKeys = Object.keys(previous);
    for (let i2 = 0, len = previousKeys.length; i2 < len; i2++) {
      if (target[previousKeys[i2]] === void 0) setProperty(previous, previousKeys[i2], void 0);
    }
  }
  function reconcile(value, options = {}) {
    const { merge, key = "id" } = options, v = unwrap(value);
    return (state2) => {
      if (!isWrappable(state2) || !isWrappable(v)) return v;
      const res = applyState(
        v,
        {
          [$ROOT]: state2
        },
        $ROOT,
        merge,
        key
      );
      return res === void 0 ? state2 : res;
    };
  }
  var producers = /* @__PURE__ */ new WeakMap();
  var setterTraps = {
    get(target, property) {
      if (property === $RAW) return target;
      const value = target[property];
      let proxy;
      return isWrappable(value) ? producers.get(value) || (producers.set(value, proxy = new Proxy(value, setterTraps)), proxy) : value;
    },
    set(target, property, value) {
      setProperty(target, property, unwrap(value));
      return true;
    },
    deleteProperty(target, property) {
      setProperty(target, property, void 0, true);
      return true;
    }
  };
  function produce(fn) {
    return (state2) => {
      if (isWrappable(state2)) {
        let proxy;
        if (!(proxy = producers.get(state2))) {
          producers.set(state2, proxy = new Proxy(state2, setterTraps));
        }
        fn(proxy);
      }
      return state2;
    };
  }

  // vendor/lib/setMethods.ts
  function difference(a3, b) {
    return new Set([...a3].filter((item) => !b.has(item)));
  }
  function intersection(a3, b) {
    return new Set([...a3].filter((item) => b.has(item)));
  }

  // vendor/lib/in-memoriam.ts
  var Database = class {
    constructor(config, values) {
      __publicField(this, "byId");
      __publicField(this, "setById");
      __publicField(this, "indexes");
      __publicField(this, "config");
      ;
      [this.byId, this.setById] = createStore({});
      this.config = config;
      this.indexes = new Map(Object.keys(config).map((i2) => [i2, new ReactiveMap()]));
      if (values) {
        for (const value of values) {
          this.set(value.id, value);
        }
      }
    }
    update(values) {
      batch(() => {
        const keysToDelete = difference(
          new Set(Object.keys(this.byId)),
          new Set(Object.keys(values))
        );
        for (const id of keysToDelete) {
          this.del(id);
        }
        for (const id in values) {
          const entity = values[id];
          this.set(id, entity);
        }
      });
    }
    del(id) {
      const entity = this.get(id);
      if (!entity) return;
      batch(() => {
        this.setById(
          produce((byId) => {
            delete byId[id];
          })
        );
        for (const indexName of this.indexes.keys()) {
          const index = this.indexes.get(indexName);
          const value = this.config[indexName](entity);
          index.get(value).delete(id);
        }
      });
    }
    set(id, newEntity) {
      batch(() => {
        if (!newEntity) {
          this.del(id);
          return;
        }
        const oldEntity = this.get(id);
        for (const indexName of this.indexes.keys()) {
          const index = this.indexes.get(indexName);
          const newValue = this.config[indexName](newEntity);
          if (oldEntity) {
            const oldValue = this.config[indexName](oldEntity);
            if (oldValue !== newValue) {
              index.get(oldValue).delete(id);
            }
          }
          const entities = index.get(newValue);
          if (entities) {
            entities.add(id);
          } else {
            index.set(newValue, new ReactiveSet([id]));
          }
        }
        this.setById(id, reconcile(newEntity));
      });
    }
    filterBy(query) {
      let entities;
      const values = Object.entries(query);
      for (const [attr, value] of values) {
        const entitySet = this.indexes.get(attr).get(value);
        if (!entitySet || entitySet.size === 0) return [];
        entities = entities ? intersection(entities, entitySet) : entitySet;
      }
      if (!entities) return [];
      return [...entities].map((id) => this.get(id));
    }
    findBy(query) {
      return this.filterBy(query)[0] || null;
    }
    get all() {
      return Object.values(this.byId);
    }
    get(id) {
      if (!id) return null;
      return this.byId[id] || null;
    }
    get size() {
      return Object.keys(this.byId).length;
    }
  };

  // platform/audio.ts
  function play(_name) {
  }

  // platform/animation.ts
  var DELETED_DURATION = 300;
  function animate(_event) {
  }

  // tools/node_modules/remeda/dist/chunk-ANXBDSUI.js
  var s = { done: false, hasNext: false };

  // tools/node_modules/remeda/dist/chunk-A3ZTVFWP.js
  function C(t3, ...o) {
    let n2 = t3, u3 = o.map((e2) => "lazy" in e2 ? A(e2) : void 0), p = 0;
    for (; p < o.length; ) {
      if (u3[p] === void 0 || !B(n2)) {
        let i2 = o[p];
        n2 = i2(n2), p += 1;
        continue;
      }
      let r2 = [];
      for (let i2 = p; i2 < o.length; i2++) {
        let l = u3[i2];
        if (l === void 0 || (r2.push(l), l.isSingle)) break;
      }
      let a3 = [];
      for (let i2 of n2) if (y(i2, a3, r2)) break;
      let { isSingle: s3 } = r2.at(-1);
      n2 = s3 ? a3[0] : a3, p += r2.length;
    }
    return n2;
  }
  function y(t3, o, n2) {
    if (n2.length === 0) return o.push(t3), false;
    let u3 = t3, p = s, e2 = false;
    for (let [r2, a3] of n2.entries()) {
      let { index: s3, items: i2 } = a3;
      if (i2.push(u3), p = a3(u3, s3, i2), a3.index += 1, p.hasNext) {
        if (p.hasMany ?? false) {
          for (let l of p.next) if (y(l, o, n2.slice(r2 + 1))) return true;
          return e2;
        }
        u3 = p.next;
      }
      if (!p.hasNext) break;
      p.done && (e2 = true);
    }
    return p.hasNext && o.push(u3), e2;
  }
  function A(t3) {
    let { lazy: o, lazyArgs: n2 } = t3, u3 = o(...n2);
    return Object.assign(u3, { isSingle: o.single ?? false, index: 0, items: [] });
  }
  function B(t3) {
    return typeof t3 == "string" || typeof t3 == "object" && t3 !== null && Symbol.iterator in t3;
  }

  // tools/node_modules/remeda/dist/chunk-BCPWYWXN.js
  function y2(t3, i2) {
    let a3 = i2.length - t3.length;
    if (a3 === 1) {
      let [n2, ...r2] = i2;
      return C(n2, { lazy: t3, lazyArgs: r2 });
    }
    if (a3 === 0) {
      let n2 = { lazy: t3, lazyArgs: i2 };
      return Object.assign((e2) => C(e2, n2), n2);
    }
    throw new Error("Wrong number of arguments");
  }

  // tools/node_modules/remeda/dist/chunk-IXASGXCR.js
  function T(...e2) {
    return y2(y3, e2);
  }
  function y3(e2) {
    let u3 = e2, n2 = /* @__PURE__ */ new Set();
    return (t3, i2, d2) => {
      let r2 = u3(t3, i2, d2);
      return n2.has(r2) ? s : (n2.add(r2), { done: false, hasNext: true, next: t3 });
    };
  }

  // tools/node_modules/remeda/dist/chunk-D6FCK2GA.js
  function u(o, n2, a3) {
    let t3 = (r2) => o(r2, ...n2);
    return a3 === void 0 ? t3 : Object.assign(t3, { lazy: a3, lazyArgs: n2 });
  }

  // tools/node_modules/remeda/dist/chunk-WIMGWYZL.js
  function u2(r2, n2, o) {
    let a3 = r2.length - n2.length;
    if (a3 === 0) return r2(...n2);
    if (a3 === 1) return u(r2, n2, o);
    throw new Error("Wrong number of arguments");
  }

  // tools/node_modules/remeda/dist/chunk-U753ZCO5.js
  function a(e2, n2) {
    if (n2 === void 0) {
      let t3 = new Set(e2);
      return (r2) => t3.has(r2);
    }
    return n2.includes(e2);
  }

  // tools/node_modules/remeda/dist/chunk-B6PG574O.js
  function c(...e2) {
    return u2(y4, e2);
  }
  function y4(e2, o) {
    let r2 = {};
    for (let [a3, n2] of e2.entries()) {
      let d2 = o(n2, a3, e2);
      r2[d2] = n2;
    }
    return r2;
  }

  // tools/node_modules/remeda/dist/chunk-VVM5DH6Z.js
  function t(...r2) {
    return u2(Object.entries, r2);
  }

  // tools/node_modules/remeda/dist/chunk-SBKJLULJ.js
  function c2(...e2) {
    return u2(d, e2);
  }
  var d = (e2, o) => {
    let n2 = {};
    for (let [a3, y5] of e2.entries()) {
      let r2 = o(y5, a3, e2);
      r2 !== void 0 && (n2[r2] ?? (n2[r2] = 0), n2[r2] += 1);
    }
    return n2;
  };

  // vendor/lib/maps/responsive.ts
  var n = null;
  var RESPONSIVE_MAP = [
    // level 0
    [
      // 1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  8
      [113, 113, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 114, 114],
      // 1
      [113, 113, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 114, 114],
      // 1
      [n, n, 132, 132, 69, 69, 7, 7, 8, 8, 70, 70, 133, 133, n, n],
      // 2
      [n, n, 132, 132, 69, 69, 7, 7, 8, 8, 70, 70, 133, 133, n, n],
      // 2
      [n, n, 140, 140, 9, 9, 10, 10, 11, 11, 12, 12, 141, 141, n, n],
      // 3
      [n, n, 140, 140, 9, 9, 10, 10, 11, 11, 12, 12, 141, 141, n, n],
      // 3
      [n, n, 13, 13, 14, 14, 81, 81, 82, 82, 15, 15, 16, 16, n, n],
      // 4
      [118, 118, 13, 13, 14, 14, 81, 81, 82, 82, 15, 15, 16, 16, 117, 117],
      // 4
      [118, 118, 17, 17, 18, 18, 83, 83, 84, 84, 19, 19, 20, 20, 117, 117],
      // 5
      [n, n, 17, 17, 18, 18, 83, 83, 84, 84, 19, 19, 20, 20, n, n],
      // 5
      [n, n, 142, 142, 21, 21, 22, 22, 23, 23, 24, 24, 143, 143, n, n],
      // 6
      [n, n, 142, 142, 21, 21, 22, 22, 23, 23, 24, 24, 143, 143, n, n],
      // 6
      [n, n, 134, 134, 71, 71, 25, 25, 26, 26, 72, 72, 135, 135, n, n],
      // 7
      [n, n, 134, 134, 71, 71, 25, 25, 26, 26, 72, 72, 135, 135, n, n],
      // 7
      [115, 115, 27, 27, 28, 28, 29, 29, 30, 30, 31, 31, 32, 32, 116, 116],
      // 8
      [115, 115, 27, 27, 28, 28, 29, 29, 30, 30, 31, 31, 32, 32, 116, 116]
      // 8
    ],
    // level 1
    [
      // 1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  8
      [n, n, 119, 119, 73, 73, 33, 33, 52, 52, 74, 74, 120, 120, n, n],
      // 1
      [n, n, 119, 119, 73, 73, 33, 33, 52, 52, 74, 74, 120, 120, n, n],
      // 1
      [n, n, 146, 146, 136, 136, 48, 48, 49, 49, 137, 137, 147, 147, n, n],
      // 2
      [n, n, 146, 146, 136, 136, 48, 48, 49, 49, 137, 137, 147, 147, n, n],
      // 2
      [n, n, n, n, 34, 34, 35, 35, 36, 36, 37, 37, n, n, n, n],
      // 3
      [n, n, n, n, 34, 34, 35, 35, 36, 36, 37, 37, n, n, n, n],
      // 3
      [n, n, 46, 46, 38, 38, 97, 97, 98, 98, 39, 39, 67, 67, n, n],
      // 4
      [n, n, 46, 46, 38, 38, 97, 97, 98, 98, 39, 39, 67, 67, n, n],
      // 4
      [n, n, 47, 47, 40, 40, 99, 99, 100, 100, 41, 41, 68, 68, n, n],
      // 5
      [n, n, 47, 47, 40, 40, 99, 99, 100, 100, 41, 41, 68, 68, n, n],
      // 5
      [n, n, n, n, 42, 42, 43, 43, 44, 44, 45, 45, n, n, n, n],
      // 6
      [n, n, n, n, 42, 42, 43, 43, 44, 44, 45, 45, n, n, n, n],
      // 6
      [n, n, 148, 148, 138, 138, 50, 50, 51, 51, 139, 139, 149, 149, n, n],
      // 7
      [n, n, 148, 148, 138, 138, 50, 50, 51, 51, 139, 139, 149, 149, n, n],
      // 7
      [n, n, 121, 121, 75, 75, 53, 53, 54, 54, 76, 76, 122, 122, n, n],
      // 8
      [n, n, 121, 121, 75, 75, 53, 53, 54, 54, 76, 76, 122, 122, n, n]
      // 8
    ],
    // level 2
    [
      // 1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  8
      [n, n, 156, 156, 101, 101, 85, 85, 86, 86, 102, 102, 157, 157, n, n],
      // 1
      [n, n, 156, 156, 101, 101, 85, 85, 86, 86, 102, 102, 157, 157, n, n],
      // 1
      [n, n, n, n, 150, 150, 77, 77, 78, 78, 151, 151, n, n, n, n],
      // 2
      [n, n, n, n, 150, 150, 77, 77, 78, 78, 151, 151, n, n, n, n],
      // 2
      [n, n, n, n, 55, 55, 56, 56, 57, 57, 58, 58, n, n, n, n],
      // 3
      [n, n, n, n, 55, 55, 56, 56, 57, 57, 58, 58, n, n, n, n],
      // 3
      [n, n, 105, 105, 59, 59, 109, 109, 110, 110, 60, 60, 107, 107, n, n],
      // 4
      [n, n, 105, 105, 59, 59, 109, 109, 110, 110, 60, 60, 107, 107, n, n],
      // 4
      [n, n, 106, 106, 61, 61, 111, 111, 112, 112, 62, 62, 108, 108, n, n],
      // 5
      [n, n, 106, 106, 61, 61, 111, 111, 112, 112, 62, 62, 108, 108, n, n],
      // 5
      [n, n, n, n, 63, 63, 64, 64, 65, 65, 66, 66, n, n, n, n],
      // 6
      [n, n, n, n, 63, 63, 64, 64, 65, 65, 66, 66, n, n, n, n],
      // 6
      [n, n, n, n, 152, 152, 79, 79, 80, 80, 153, 153, n, n, n, n],
      // 7
      [n, n, n, n, 152, 152, 79, 79, 80, 80, 153, 153, n, n, n, n],
      // 7
      [n, n, 155, 155, 103, 103, 87, 87, 88, 88, 104, 104, 154, 154, n, n],
      // 8
      [n, n, 155, 155, 103, 103, 87, 87, 88, 88, 104, 104, 154, 154, n, n]
      // 8
    ],
    // level 3
    [
      // 1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  8
      [n, n, n, n, n, n, 127, 127, 128, 128, n, n, n, n, n, n],
      // 1
      [n, n, n, n, n, n, 127, 127, 128, 128, n, n, n, n, n, n],
      // 1
      [n, n, n, n, n, n, 158, 158, 159, 159, n, n, n, n, n, n],
      // 2
      [n, n, n, n, n, n, 158, 158, 159, 159, n, n, n, n, n, n],
      // 2
      [n, n, n, n, n, n, 89, 89, 90, 90, n, n, n, n, n, n],
      // 3
      [n, n, n, n, n, n, 89, 89, 90, 90, n, n, n, n, n, n],
      // 3
      [n, n, n, n, 95, 95, 123, 123, 124, 124, 93, 93, n, n, n, n],
      // 4
      [n, n, n, n, 95, 95, 123, 123, 124, 124, 93, 93, n, n, n, n],
      // 4
      [n, n, n, n, 96, 96, 125, 125, 126, 126, 94, 94, n, n, n, n],
      // 5
      [n, n, n, n, 96, 96, 125, 125, 126, 126, 94, 94, n, n, n, n],
      // 5
      [n, n, n, n, n, n, 91, 91, 92, 92, n, n, n, n, n, n],
      // 6
      [n, n, n, n, n, n, 91, 91, 92, 92, n, n, n, n, n, n],
      // 6
      [n, n, n, n, n, n, 160, 160, 161, 161, n, n, n, n, n, n],
      // 7
      [n, n, n, n, n, n, 160, 160, 161, 161, n, n, n, n, n, n],
      // 7
      [n, n, n, n, n, n, 129, 129, 130, 130, n, n, n, n, n, n],
      // 8
      [n, n, n, n, n, n, 129, 129, 130, 130, n, n, n, n, n, n]
      // 8
    ],
    // level 4
    [
      // 1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  8
      [n, n, n, n, n, n, n, 144, 144, n, n, n, n, n, n, n],
      // 1
      [n, n, n, n, n, n, n, 144, 144, n, n, n, n, n, n, n],
      // 1
      [n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n],
      // 2
      [n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n],
      // 2
      [n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n],
      // 3
      [n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n],
      // 3
      [n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n],
      // 4
      [n, n, n, n, n, n, n, 131, 131, n, n, n, n, n, n, n],
      // 4
      [n, n, n, n, n, n, n, 131, 131, n, n, n, n, n, n, n],
      // 5
      [n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n],
      // 5
      [n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n],
      // 6
      [n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n],
      // 6
      [n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n],
      // 7
      [n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n],
      // 7
      [n, n, n, n, n, n, n, 145, 145, n, n, n, n, n, n, n],
      // 8
      [n, n, n, n, n, n, n, 145, 145, n, n, n, n, n, n, n]
      // 8
    ]
  ];

  // platform/posthog.ts
  var posthog_default = {};

  // vendor/lib/observability.ts
  function captureEvent(event, properties) {
    if (true) return;
    setTimeout(() => {
      posthog_default.capture(event, properties);
    }, 100);
  }

  // vendor/lib/resolveDragons.ts
  function resolveDragons({
    game: game2,
    tile,
    tileDb
  }) {
    const dragonRun = game2.dragonRun;
    const newCard = tile.cardId;
    const dragonCard = isDragon(newCard);
    if (!dragonRun) {
      if (dragonCard) {
        game2.dragonRun = { color: dragonCard.rank, combo: 1 };
      }
      return;
    }
    if (!cardMatchesColor(dragonRun.color, newCard, tileDb)) {
      captureEvent("dragon_run_finished", {
        color: dragonRun.color,
        combo: dragonRun.combo
      });
      game2.dragonRun = dragonCard ? { color: dragonCard.rank, combo: 1 } : void 0;
      return;
    }
    if (dragonRun.combo === 10) {
      game2.dragonRun = void 0;
    } else {
      dragonRun.combo = dragonRun.combo + 1;
    }
  }

  // vendor/lib/resolveGems.ts
  function resolveGems({ tile, game: game2 }) {
    const gemCard = isGem(tile.cardId);
    if (!gemCard) {
      game2.temporaryMaterial = void 0;
      return;
    }
    play("gemstone");
    const color = gemCard.colors[0];
    const tempMaterial = {
      r: "garnet",
      g: "jade",
      b: "topaz",
      k: "quartz"
    }[color];
    game2.temporaryMaterial = tempMaterial;
  }

  // tools/node_modules/rand-seed/dist/es/index.js
  var e = class {
    static _xfnv1a(e2) {
      let t3 = 2166136261;
      for (let i2 = 0; i2 < e2.length; i2++) t3 = Math.imul(t3 ^ e2.charCodeAt(i2), 16777619);
      return () => (t3 += t3 << 13, t3 ^= t3 >>> 7, t3 += t3 << 3, t3 ^= t3 >>> 17, (t3 += t3 << 5) >>> 0);
    }
  };
  var t2 = class _t extends e {
    constructor(e2) {
      super(), Object.defineProperty(this, "a", { enumerable: true, configurable: true, writable: true, value: void 0 }), this.a = _t._xfnv1a(e2)();
    }
    next() {
      let e2 = this.a += 1831565813;
      return e2 = Math.imul(e2 ^ e2 >>> 15, 1 | e2), e2 ^= e2 + Math.imul(e2 ^ e2 >>> 7, 61 | e2), ((e2 ^ e2 >>> 14) >>> 0) / 4294967296;
    }
  };
  var i = class _i extends e {
    constructor(e2) {
      super(), Object.defineProperty(this, "a", { enumerable: true, configurable: true, writable: true, value: void 0 }), Object.defineProperty(this, "b", { enumerable: true, configurable: true, writable: true, value: void 0 }), Object.defineProperty(this, "c", { enumerable: true, configurable: true, writable: true, value: void 0 }), Object.defineProperty(this, "d", { enumerable: true, configurable: true, writable: true, value: void 0 });
      const t3 = _i._xfnv1a(e2);
      this.a = t3(), this.b = t3(), this.c = t3(), this.d = t3();
    }
    next() {
      this.a >>>= 0, this.b >>>= 0, this.c >>>= 0, this.d >>>= 0;
      let e2 = this.a + this.b | 0;
      return this.a = this.b ^ this.b >>> 9, this.b = this.c + (this.c << 3) | 0, this.c = this.c << 21 | this.c >>> 11, this.d = this.d + 1 | 0, e2 = e2 + this.d | 0, this.c = this.c + e2 | 0, (e2 >>> 0) / 4294967296;
    }
  };
  var r = class _r extends e {
    constructor(e2) {
      super(), Object.defineProperty(this, "a", { enumerable: true, configurable: true, writable: true, value: void 0 }), Object.defineProperty(this, "b", { enumerable: true, configurable: true, writable: true, value: void 0 }), Object.defineProperty(this, "c", { enumerable: true, configurable: true, writable: true, value: void 0 }), Object.defineProperty(this, "d", { enumerable: true, configurable: true, writable: true, value: void 0 });
      const t3 = _r._xfnv1a(e2);
      this.a = t3(), this.b = t3(), this.c = t3(), this.d = t3();
    }
    next() {
      const e2 = this.b << 9;
      let t3 = 5 * this.b;
      return t3 = 9 * (t3 << 7 | t3 >>> 25), this.c ^= this.a, this.d ^= this.b, this.b ^= this.c, this.a ^= this.d, this.c ^= e2, this.d = this.d << 11 | this.d >>> 21, (t3 >>> 0) / 4294967296;
    }
  };
  var s2;
  !(function(e2) {
    e2.sfc32 = "sfc32", e2.mulberry32 = "mulberry32", e2.xoshiro128ss = "xoshiro128ss";
  })(s2 || (s2 = {}));
  var a2 = class {
    constructor(e2, t3 = s2.sfc32) {
      Object.defineProperty(this, "str", { enumerable: true, configurable: true, writable: true, value: void 0 }), Object.defineProperty(this, "prng", { enumerable: true, configurable: true, writable: true, value: void 0 }), Object.defineProperty(this, "generator", { enumerable: true, configurable: true, writable: true, value: void 0 }), this.str = e2, this.prng = t3, this.generator = this._initializeGenerator();
    }
    next() {
      return this.generator.next();
    }
    _initializeGenerator() {
      if (/* @__PURE__ */ ((e3) => null === e3)(e2 = this.str) || /* @__PURE__ */ ((e3) => void 0 === e3)(e2)) return this.wrap();
      var e2;
      switch (this.prng) {
        case "sfc32":
          return new i(this.str);
        case "mulberry32":
          return new t2(this.str);
        case "xoshiro128ss":
          return new r(this.str);
        default:
          return this.wrap();
      }
    }
    wrap() {
      return { next: () => Math.random() };
    }
  };

  // vendor/lib/rand.ts
  function shuffle(array, rng) {
    const shuffled = [...array];
    for (let i2 = shuffled.length - 1; i2 > 0; i2--) {
      const j = Math.floor(rng.next() * (i2 + 1));
      [shuffled[i2], shuffled[j]] = [shuffled[j], shuffled[i2]];
    }
    return shuffled;
  }

  // vendor/lib/resolveJokers.ts
  function resolveJokers({
    tileDb,
    tile
  }) {
    const jokerCard = isJoker(tile.cardId);
    if (!jokerCard) return;
    play("joker");
    batch(() => {
      for (const tile2 of tileDb.filterBy({ deleted: false })) {
        animate({ id: tile2.id, name: "deleted" });
      }
    });
    setTimeout(() => {
      batch(() => {
        for (const tile2 of tileDb.all) {
          animate({ id: tile2.id, name: "fall" });
        }
        const rng = new a2();
        shuffleTiles({ rng, tileDb });
        play("tiles");
      });
    }, DELETED_DURATION + 100);
  }
  function shuffleTiles({ rng, tileDb }) {
    const newTileDb = initTileDb({});
    const currentTiles = tileDb.all.filter((tile) => !tile.deleted);
    for (const tile of currentTiles) {
      newTileDb.set(tile.id, {
        cardId: "bam1",
        material: "bone",
        id: tile.id,
        x: tile.x,
        y: tile.y,
        z: tile.z,
        deleted: false,
        selected: false
      });
    }
    const pickOrder = [];
    while (true) {
      const freeTiles = getFreeTiles(newTileDb);
      if (freeTiles.length <= 1) break;
      while (freeTiles.length > 1) {
        const idx1 = Math.floor(rng.next() * freeTiles.length);
        const tile1 = freeTiles[idx1];
        freeTiles.splice(idx1, 1);
        const idx2 = Math.floor(rng.next() * freeTiles.length);
        const tile2 = freeTiles[idx2];
        freeTiles.splice(idx2, 1);
        newTileDb.del(tile1.id);
        newTileDb.del(tile2.id);
        pickOrder.push(tile1, tile2);
      }
    }
    if (newTileDb.size > 0) {
      return shuffleTiles({ rng, tileDb });
    }
    const pairs = [];
    const usedTileIds = /* @__PURE__ */ new Set();
    for (let i2 = 0; i2 < currentTiles.length; i2++) {
      const tile1 = currentTiles[i2];
      if (usedTileIds.has(tile1.id)) continue;
      for (let j = i2 + 1; j < currentTiles.length; j++) {
        const tile2 = currentTiles[j];
        if (usedTileIds.has(tile2.id)) continue;
        if (cardsMatch(tile1.cardId, tile2.cardId)) {
          pairs.push([tile1, tile2]);
          usedTileIds.add(tile1.id);
          usedTileIds.add(tile2.id);
          break;
        }
      }
    }
    const shuffledPairs = shuffle(pairs, rng);
    for (const tile of tileDb.all) {
      tileDb.del(tile.id);
    }
    for (let i2 = 0; i2 < pickOrder.length; i2 += 2) {
      const tile1 = pickOrder[pickOrder.length - 1 - i2];
      const tile2 = pickOrder[pickOrder.length - 2 - i2];
      const pair = shuffledPairs[i2 / 2];
      const [deckTile1, deckTile2] = pair;
      const id1 = tile1.id;
      const id2 = tile2.id;
      tileDb.set(id1, {
        id: id1,
        cardId: deckTile1.cardId,
        material: deckTile1.material,
        x: tile1.x,
        y: tile1.y,
        z: tile1.z,
        deleted: false,
        selected: false
      });
      tileDb.set(id2, {
        id: id2,
        cardId: deckTile2.cardId,
        material: deckTile2.material,
        x: tile2.x,
        y: tile2.y,
        z: tile2.z,
        deleted: false,
        selected: false
      });
    }
    return tileDb.byId;
  }

  // vendor/lib/resolveMutations.ts
  var MUTATION_RANKS = {
    mutation1: ["crack", "bam"],
    mutation2: ["dot", "crack"],
    mutation3: ["bam", "dot"]
  };
  var MUTATION_MATERIALS = {
    r: ["bone", "garnet", "ruby"],
    g: ["bone", "jade", "emerald"],
    b: ["bone", "topaz", "sapphire"]
  };
  function changeSuits(tileDb, tiles2, fromSuit, toSuit) {
    for (const tile of tiles2) {
      const newCardId = tile.cardId.replace(fromSuit, toSuit);
      const material = tile.material;
      const color = isSuit(tile.cardId).colors[0];
      const materialIndex = MUTATION_MATERIALS[color].indexOf(material);
      const newColor = isSuit(newCardId).colors[0];
      const newMaterial = MUTATION_MATERIALS[newColor][materialIndex];
      tileDb.set(tile.id, { ...tile, cardId: newCardId, material: newMaterial });
      animate({ id: tile.id, name: "mutate" });
    }
  }
  function resolveMutations({
    tileDb,
    tile
  }) {
    const mutationCard = isMutation(tile.cardId);
    if (!mutationCard) return;
    play("mutation");
    const id = mutationCard.id;
    const [aSuit, bSuit] = MUTATION_RANKS[id];
    const aTiles = tileDb.all.filter(
      (tile2) => getCard(tile2.cardId).suit === aSuit
    );
    const bTiles = tileDb.all.filter(
      (tile2) => getCard(tile2.cardId).suit === bSuit
    );
    changeSuits(tileDb, aTiles, aSuit, bSuit);
    changeSuits(tileDb, bTiles, bSuit, aSuit);
  }

  // vendor/lib/resolvePhoenixes.ts
  function parseNumber(cardId) {
    const rank = Number.parseInt(getCard(cardId).rank);
    if (Number.isNaN(rank)) return null;
    return rank;
  }
  function nextNumber(number) {
    if (number === 9) return 1;
    return number + 1;
  }
  function continuesPhoenixRun(number, cardId) {
    const rank = parseNumber(cardId);
    if (rank === null) return false;
    if (number === void 0) return true;
    return rank === nextNumber(number);
  }
  function maybeStartsPhoenixRun(game2, card) {
    const canStart = card && !game2.dragonRun;
    game2.phoenixRun = canStart ? { number: void 0, combo: 1 } : void 0;
  }
  function resolvePhoenixRun({ game: game2, tile }) {
    const phoenixRun = game2.phoenixRun;
    const newCardId = tile.cardId;
    const phoenixCard = isPhoenix(newCardId);
    if (!phoenixRun) {
      maybeStartsPhoenixRun(game2, phoenixCard);
      return;
    }
    if (!continuesPhoenixRun(phoenixRun.number, newCardId)) {
      maybeStartsPhoenixRun(game2, phoenixCard);
      captureEvent("phoenix_run_finished", {
        combo: phoenixRun.combo
      });
      return;
    }
    const number = parseNumber(newCardId);
    if (number === null) return;
    if (phoenixRun.combo === 10) {
      game2.phoenixRun = void 0;
    } else {
      phoenixRun.combo = phoenixRun.combo + 1;
    }
    phoenixRun.number = number;
  }

  // vendor/lib/resolveWinds.ts
  function getNewTile(tileDb, tile, axis, bias) {
    const MAP_SIZE = {
      x: mapGetWidth(),
      y: mapGetHeight()
    };
    const value = tile[axis];
    if (bias === 0) return;
    const direction = Math.sign(bias);
    if (tile[axis] === 0 && direction === -1) return;
    if (tile[axis] === MAP_SIZE[axis] - 1 && direction === 1) return;
    for (let attempt = Math.abs(bias); attempt > 0; attempt--) {
      const displacement = attempt * direction;
      const newValue = value + displacement;
      const newTile = { ...tile, [axis]: newValue };
      if (overlaps(tileDb, newTile, 0)) continue;
      if (!fullyOverlaps(tileDb, newTile, -1)) continue;
      return newTile;
    }
    return;
  }
  var BIASES = {
    n: ["y", -2],
    s: ["y", 2],
    e: ["x", 2],
    w: ["x", -2]
  };
  function resolveWinds({ tileDb, tile }) {
    const windCard = isWind(tile.cardId);
    if (!windCard) return;
    play("wind");
    const wind = windCard.rank;
    const [axis, bias] = BIASES[wind];
    const highestLevel = tileDb.all.reduce(
      (max, tile2) => Math.max(max, tile2.z),
      0
    );
    for (let z = 1; z <= highestLevel; z++) {
      const direction = Math.sign(bias);
      const zTiles = tileDb.filterBy({ z, deleted: false }).toSorted((a3, b) => direction * (b[axis] - a3[axis]));
      for (const tile2 of zTiles) {
        const newTile = getNewTile(tileDb, tile2, axis, bias);
        if (!newTile) continue;
        tileDb.set(tile2.id, { ...newTile });
        animate({ id: tile2.id, name: "wind" });
      }
    }
  }

  // vendor/lib/game.ts
  function gameOverCondition(tileDb, game2) {
    const tilesAlive = tileDb.filterBy({ deleted: false });
    if (tilesAlive.length === 0) {
      return "empty-board";
    }
    const availableTiles = getAvailablePairs(tileDb, game2);
    if (availableTiles.length === 0) {
      return "no-pairs";
    }
    return null;
  }
  function getAvailablePairs(tileDb, game2) {
    const tiles2 = getFreeTiles(tileDb, game2);
    const pairs = [];
    for (let i2 = 0; i2 < tiles2.length; i2++) {
      for (let j = i2 + 1; j < tiles2.length; j++) {
        const tile1 = tiles2[i2];
        const tile2 = tiles2[j];
        if (!cardsMatch(tile1.cardId, tile2.cardId)) continue;
        pairs.push([tile1, tile2]);
      }
    }
    return pairs;
  }
  function getMaterialPoints(material) {
    switch (material) {
      case "topaz":
      case "quartz":
      case "garnet":
        return 1;
      case "jade":
        return 2;
      case "sapphire":
      case "obsidian":
      case "ruby":
        return 24;
      case "emerald":
        return 48;
      default:
        return 0;
    }
  }
  function getElements(cardId, tileDb) {
    const colors = getColors(cardId, tileDb);
    const elements2 = colors.flatMap(
      (rank) => tileDb.filterBy({ deleted: false, cardId: `element${rank}` })
    );
    return {
      visibleElements: elements2.filter((tile) => !isCovered(tileDb, tile)).length,
      freeElements: elements2.filter((tile) => isFree(tileDb, tile)).length
    };
  }
  function getTaijituMultiplier(tiles2) {
    const [tile1, tile2] = tiles2;
    if (tile1.z !== tile2.z) return 1;
    if (!tiles2.every((tile) => isTaijitu(tile.cardId))) return 1;
    const xDistance = Math.abs(tile1.x - tile2.x);
    const yDistance = Math.abs(tile1.y - tile2.y);
    const isAdjacent = xDistance === 2 && yDistance < 1 || xDistance < 1 && yDistance === 2;
    return isAdjacent ? 3 : 1;
  }
  function getActiveShadows(tileDb) {
    const cards = tileDb.filterBy({ deleted: false }).flatMap((tile) => {
      const shadowCard = isShadow(tile.cardId);
      return shadowCard && isFree(tileDb, tile) ? shadowCard : [];
    });
    const suits = [
      ["bam", "g"],
      ["crack", "r"],
      ["dot", "b"]
    ];
    const result = /* @__PURE__ */ new Set([]);
    for (const [suit, color] of suits) {
      if (!cards.some((card) => card.rank === color)) continue;
      result.add(suit);
    }
    return result;
  }
  function getPoints({
    game: game2,
    tile,
    tileDb
  }) {
    const rawPoints = getRawPoints({
      cardId: tile.cardId,
      material: getMaterial({ tile, game: game2, tileDb })
    });
    const dragonRunMultiplier = getDragonMultiplier(game2);
    const phoenixRunMultiplier = getPhoenixRunMultiplier(game2);
    const { visibleElements, freeElements } = getElements(tile.cardId, tileDb);
    return (rawPoints + visibleElements) * Math.max(1, dragonRunMultiplier + phoenixRunMultiplier + freeElements);
  }
  function getMaterialCoins(material) {
    switch (material) {
      case "garnet":
        return 1;
      case "ruby":
        return 3;
      default:
        return 0;
    }
  }
  function getCoins({
    tile,
    game: game2,
    tileDb
  }) {
    const materialCoins = getMaterialCoins(getMaterial({ tile, game: game2, tileDb }));
    const rabbitCoins = isRabbit(tile.cardId) ? 1 : 0;
    return materialCoins + rabbitCoins;
  }
  function getRawPoints({
    cardId,
    material
  }) {
    const cardPoints = getCard(cardId).points;
    const materialPoints = getMaterialPoints(material);
    return cardPoints + materialPoints;
  }
  function getFreeTiles(tileDb, game2) {
    return tileDb.filterBy({ deleted: false }).filter((tile) => isFree(tileDb, tile, game2));
  }
  function selectTile({
    tileDb,
    game: game2,
    tileId
  }) {
    batch(() => {
      const tile = tileDb.get(tileId);
      if (!tile) throw new Error("Tile not found");
      const firstTile = tileDb.findBy({ selected: true });
      if (!firstTile) {
        tileDb.set(tileId, { ...tile, selected: true });
        return;
      }
      if (firstTile.id === tileId) {
        tileDb.set(tileId, { ...tile, selected: false });
        animate({ id: tileId, name: "shake" });
        play("shake");
        return;
      }
      tileDb.set(firstTile.id, { ...firstTile, selected: false });
      const tiles2 = [firstTile, tile];
      if (cardsMatch(firstTile.cardId, tile.cardId)) {
        deleteTiles(tileDb, tiles2);
        resolveDragons({ game: game2, tile, tileDb });
        resolvePhoenixRun({ game: game2, tile });
        resolveMutations({ tileDb, tile });
        resolveBlackMaterials({ tile, game: game2, tileDb });
        const taijituMultiplier = getTaijituMultiplier(tiles2);
        if (taijituMultiplier > 1) play("gong2");
        for (const tile2 of tiles2) {
          const newPoints = getPoints({ game: game2, tile: tile2, tileDb }) * taijituMultiplier;
          const newCoins = getCoins({ tile: tile2, game: game2, tileDb });
          if (newCoins) play("coin");
          tileDb.set(tile2.id, { ...tile2, points: newPoints, coins: newCoins });
          game2.points += newPoints;
          game2.coins += newCoins;
        }
        resolveGems({ tile, game: game2 });
        resolveJokers({ tileDb, tile, game: game2 });
        resolveWinds({ tileDb, tile });
        if (!isMutation(tile.cardId) && !isDragon(tile.cardId) && !isPhoenix(tile.cardId) && !isGem(tile.cardId) && !isJoker(tile.cardId) && !isWind(tile.cardId)) {
          play("ding");
        }
      } else {
        if (!resolveJumpingTiles({ tiles: tiles2, tileDb })) {
          for (const tile2 of tiles2) {
            animate({ id: tile2.id, name: "shake" });
          }
          play("shake");
        }
      }
      const condition = gameOverCondition(tileDb, game2);
      if (condition) {
        game2.pause = true;
        game2.endCondition = condition;
        return;
      }
    });
  }
  function frogMatchesLotus(cardIds) {
    const [cardId1, cardId2] = cardIds;
    const frog = isFrog(cardId1);
    const lotus = isLotus(cardId2);
    if (!frog || !lotus) return false;
    return frog.rank === lotus.rank;
  }
  function cardsMatch(cardId1, cardId2) {
    if (isFlower(cardId1) && isFlower(cardId2)) {
      return true;
    }
    if (frogMatchesLotus([cardId1, cardId2]) || frogMatchesLotus([cardId2, cardId1])) {
      return true;
    }
    return cardId1 === cardId2;
  }
  function deleteTiles(tileDb, tiles2) {
    for (const tile of tiles2) {
      tileDb.set(tile.id, { ...tile, deleted: true, selected: false });
      animate({ id: tile.id, name: "deleted" });
    }
  }
  var deckTileIndexes = {
    cardId: (tile) => tile.cardId,
    material: (tile) => tile.material
  };
  function getInitialPairs() {
    const pairs = [];
    const regularTiles = [...bams, ...cracks, ...dots];
    for (const tile of regularTiles) {
      pairs.push([tile, tile]);
    }
    return pairs;
  }
  function checkSuit(cardId, suit) {
    const card = getCard(cardId);
    return card.suit === suit ? card : null;
  }
  function isDragon(cardId) {
    return checkSuit(cardId, "dragon");
  }
  function isMutation(cardId) {
    return checkSuit(cardId, "mutation");
  }
  function isFlower(cardId) {
    return checkSuit(cardId, "flower");
  }
  function isWind(cardId) {
    return checkSuit(cardId, "wind");
  }
  function isSuit(cardId) {
    return isBam(cardId) || isCrack(cardId) || isDot(cardId);
  }
  function isBam(cardId) {
    return checkSuit(cardId, "bam");
  }
  function isCrack(cardId) {
    return checkSuit(cardId, "crack");
  }
  function isDot(cardId) {
    return checkSuit(cardId, "dot");
  }
  function isRabbit(cardId) {
    return checkSuit(cardId, "rabbit");
  }
  function isJoker(cardId) {
    return checkSuit(cardId, "joker");
  }
  function isPhoenix(cardId) {
    return checkSuit(cardId, "phoenix");
  }
  function isFrog(cardId) {
    return checkSuit(cardId, "frog");
  }
  function isGem(cardId) {
    return checkSuit(cardId, "gem");
  }
  function isSparrow(cardId) {
    return checkSuit(cardId, "sparrow");
  }
  function isLotus(cardId) {
    return checkSuit(cardId, "lotus");
  }
  function isTaijitu(cardId) {
    return checkSuit(cardId, "taijitu");
  }
  function isShadow(cardId) {
    return checkSuit(cardId, "shadow");
  }
  function coord(position) {
    return `${position.x},${position.y},${position.z}`;
  }
  var tileIndexes = {
    x: (tile) => tile.x,
    y: (tile) => tile.y,
    z: (tile) => tile.z,
    deleted: (tile) => tile.deleted,
    selected: (tile) => tile.selected,
    coord,
    cardId: (tile) => tile.cardId
  };
  function initTileDb(tiles2) {
    const db = new Database(tileIndexes);
    db.update(tiles2);
    return db;
  }
  function fullyOverlaps(tileDb, position, z) {
    const find = getFinder(tileDb, position);
    const left = find(-1, 0, z);
    const right = find(1, 0, z);
    const top = find(0, -1, z);
    const bottom = find(0, 1, z);
    const topLeft = find(-1, -1, z);
    const topRight = find(1, -1, z);
    const bottomLeft = find(-1, 1, z);
    const bottomRight = find(1, 1, z);
    const center = find(0, 0, z);
    return !!(center || left && right || top && bottom || topLeft && bottomRight || topRight && bottomLeft || topLeft && topRight && bottomLeft || topLeft && topRight && bottomRight || topLeft && bottomLeft && bottomRight || topRight && bottomLeft && bottomRight || topLeft && topRight && bottomLeft && bottomRight);
  }
  function overlaps(tileDb, position, z) {
    const find = getFinder(tileDb, position);
    const area = [-1, 0, 1];
    for (const x of area) {
      for (const y5 of area) {
        const tile = find(x, y5, z);
        if (tile) return tile;
      }
    }
    return null;
  }
  function getFreedoms(tileDb, position) {
    const find = getFinder(tileDb, position);
    const hasLeftTile = find(-2, -1) || find(-2, 0) || find(-2, 1);
    const hasRightTile = find(2, -1) || find(2, 0) || find(2, 1);
    const hasTopTile = find(-1, -2) || find(0, -2) || find(1, -2);
    const hasBottomTile = find(-1, 2) || find(0, 2) || find(1, 2);
    return {
      left: !hasLeftTile,
      right: !hasRightTile,
      top: !hasTopTile,
      bottom: !hasBottomTile
    };
  }
  function getFinder(tileDb, position) {
    return (x = 0, y5 = 0, z = 0) => {
      const tile = tileDb.filterBy({
        coord: coord({
          x: position.x + x,
          y: position.y + y5,
          z: position.z + z
        })
      }).find((t3) => !t3.deleted);
      if (!tile) return null;
      return tile;
    };
  }
  function isCovered(tileDb, tile) {
    return overlaps(tileDb, tile, 1);
  }
  function isFree(tileDb, tile, game2) {
    if (tile.deleted) return false;
    if (isCovered(tileDb, tile)) return false;
    const material = getMaterial({ tile, game: game2, tileDb });
    if (material === "topaz" || material === "sapphire") return true;
    const freedoms = getFreedoms(tileDb, tile);
    return freedoms.left || freedoms.right;
  }
  function getShadowMaterial(cardId, material, tileDb) {
    const suitCard = isSuit(cardId);
    if (!suitCard) return material;
    if (material === "bone") return material;
    const activeShadows = getActiveShadows(tileDb);
    if (activeShadows.has(suitCard.suit)) {
      return isShiny(material) ? "obsidian" : "quartz";
    }
    return material;
  }
  function getMaterial({
    tile,
    tileDb,
    game: game2
  }) {
    const temporaryMaterial = game2?.temporaryMaterial;
    if (temporaryMaterial) {
      const materials = {
        topaz: { color: "b", evolution: "sapphire" },
        garnet: { color: "r", evolution: "ruby" },
        jade: { color: "g", evolution: "emerald" },
        quartz: { color: "k", evolution: "obsidian" }
      };
      const { color, evolution } = materials[temporaryMaterial];
      const card = getCard(tile.cardId);
      let material = temporaryMaterial;
      if (tile.material !== "bone") {
        material = evolution;
      }
      if (new Set(card.colors).has(color)) {
        return getShadowMaterial(tile.cardId, material, tileDb);
      }
    }
    return getShadowMaterial(tile.cardId, tile.material, tileDb);
  }
  function getDragonMultiplier(game2) {
    return game2.dragonRun?.combo ?? 0;
  }
  function getPhoenixRunMultiplier(game2) {
    return (game2.phoenixRun?.combo ?? 0) * 2;
  }
  function resolveBlackMaterials({
    tile,
    game: game2,
    tileDb
  }) {
    game2.pause = getMaterial({ tile, game: game2, tileDb }) === "obsidian" || getMaterial({ tile, game: game2, tileDb }) === "quartz";
  }
  function getColors(cardId, tileDb) {
    const card = getCard(cardId);
    const colors = card.colors;
    if (!isSuit(cardId)) {
      return colors;
    }
    const activeShadows = getActiveShadows(tileDb);
    if (activeShadows.has(card.suit)) {
      return ["k"];
    }
    return colors;
  }
  function cardMatchesColor(color, cardId, tileDb) {
    return a(color, getColors(cardId, tileDb));
  }
  function jump({
    fromTile,
    toPosition,
    tileDb
  }) {
    const newTile = { ...fromTile, ...toPosition };
    tileDb.set(fromTile.id, { ...newTile });
    animate({ id: fromTile.id, name: "jump" });
  }
  function resolveJumpingTiles({
    tiles: tiles2,
    tileDb
  }) {
    const [firstTile, tile] = tiles2;
    const frogCard = isFrog(firstTile.cardId);
    function color(card, tile2) {
      return cardMatchesColor(card.rank, tile2.cardId, tileDb);
    }
    if (frogCard && color(frogCard, tile)) {
      const toPosition = { x: tile.x, y: tile.y, z: tile.z + 1 };
      jump({ fromTile: firstTile, toPosition, tileDb });
      play("frog");
      return true;
    }
    const sparrowCard = isSparrow(firstTile.cardId);
    if (sparrowCard && color(sparrowCard, tile) || isSparrow(tile.cardId)) {
      const fromPosition = { x: firstTile.x, y: firstTile.y, z: firstTile.z };
      const toPosition = { x: tile.x, y: tile.y, z: tile.z };
      jump({ fromTile: firstTile, toPosition, tileDb });
      jump({ fromTile: tile, toPosition: fromPosition, tileDb });
      play("sparrow");
      return true;
    }
    const lotusCard = isLotus(tile.cardId);
    if (lotusCard && color(lotusCard, firstTile)) {
      const toPosition = { x: tile.x, y: tile.y, z: tile.z + 1 };
      jump({ fromTile: firstTile, toPosition, tileDb });
      play("lotus");
      return true;
    }
    return false;
  }
  function mapGet(map, x, y5, z) {
    if (x < 0 || y5 < 0 || z < 0) return null;
    return map[z]?.[y5]?.[x]?.toString() ?? null;
  }
  function mapGetWidth() {
    return RESPONSIVE_MAP[0][0].length;
  }
  function mapGetHeight() {
    return RESPONSIVE_MAP[0].length;
  }
  function isShiny(material) {
    return material === "obsidian" || material === "sapphire" || material === "emerald" || material === "ruby";
  }
  function getMap(tiles2) {
    const limitedMap = JSON.parse(JSON.stringify(RESPONSIVE_MAP));
    return limitedMap.map(
      (level) => level.map(
        (row) => row.map(
          (tile) => tile !== null && tile > tiles2 ? null : tile
        )
      )
    );
  }
  var bams = [
    { id: "bam1", suit: "bam", rank: "1", colors: ["g"], points: 1 },
    { id: "bam2", suit: "bam", rank: "2", colors: ["g"], points: 1 },
    { id: "bam3", suit: "bam", rank: "3", colors: ["g"], points: 1 },
    { id: "bam4", suit: "bam", rank: "4", colors: ["g"], points: 1 },
    { id: "bam5", suit: "bam", rank: "5", colors: ["g"], points: 1 },
    { id: "bam6", suit: "bam", rank: "6", colors: ["g"], points: 1 },
    { id: "bam7", suit: "bam", rank: "7", colors: ["g"], points: 1 },
    { id: "bam8", suit: "bam", rank: "8", colors: ["g"], points: 1 },
    { id: "bam9", suit: "bam", rank: "9", colors: ["g"], points: 1 }
  ];
  var cracks = [
    { id: "crack1", suit: "crack", rank: "1", colors: ["r"], points: 1 },
    { id: "crack2", suit: "crack", rank: "2", colors: ["r"], points: 1 },
    { id: "crack3", suit: "crack", rank: "3", colors: ["r"], points: 1 },
    { id: "crack4", suit: "crack", rank: "4", colors: ["r"], points: 1 },
    { id: "crack5", suit: "crack", rank: "5", colors: ["r"], points: 1 },
    { id: "crack6", suit: "crack", rank: "6", colors: ["r"], points: 1 },
    { id: "crack7", suit: "crack", rank: "7", colors: ["r"], points: 1 },
    { id: "crack8", suit: "crack", rank: "8", colors: ["r"], points: 1 },
    { id: "crack9", suit: "crack", rank: "9", colors: ["r"], points: 1 }
  ];
  var dots = [
    { id: "dot1", suit: "dot", rank: "1", colors: ["b"], points: 1 },
    { id: "dot2", suit: "dot", rank: "2", colors: ["b"], points: 1 },
    { id: "dot3", suit: "dot", rank: "3", colors: ["b"], points: 1 },
    { id: "dot4", suit: "dot", rank: "4", colors: ["b"], points: 1 },
    { id: "dot5", suit: "dot", rank: "5", colors: ["b"], points: 1 },
    { id: "dot6", suit: "dot", rank: "6", colors: ["b"], points: 1 },
    { id: "dot7", suit: "dot", rank: "7", colors: ["b"], points: 1 },
    { id: "dot8", suit: "dot", rank: "8", colors: ["b"], points: 1 },
    { id: "dot9", suit: "dot", rank: "9", colors: ["b"], points: 1 }
  ];
  var winds = [
    { id: "windn", suit: "wind", rank: "n", colors: ["k"], points: 3 },
    { id: "windw", suit: "wind", rank: "w", colors: ["k"], points: 3 },
    { id: "winds", suit: "wind", rank: "s", colors: ["k"], points: 3 },
    { id: "winde", suit: "wind", rank: "e", colors: ["k"], points: 3 }
  ];
  var dragons = [
    { id: "dragonr", suit: "dragon", rank: "r", colors: ["r"], points: 2 },
    { id: "dragong", suit: "dragon", rank: "g", colors: ["g"], points: 2 },
    { id: "dragonb", suit: "dragon", rank: "b", colors: ["b"], points: 2 },
    { id: "dragonk", suit: "dragon", rank: "k", colors: ["k"], points: 2 }
  ];
  var rabbits = [
    { id: "rabbitr", suit: "rabbit", rank: "r", colors: ["r"], points: 2 },
    { id: "rabbitg", suit: "rabbit", rank: "g", colors: ["g"], points: 2 },
    { id: "rabbitb", suit: "rabbit", rank: "b", colors: ["b"], points: 2 }
  ];
  var frogs = [
    { id: "frogr", suit: "frog", rank: "r", colors: ["r"], points: 2 },
    { id: "frogb", suit: "frog", rank: "b", colors: ["b"], points: 2 },
    { id: "frogg", suit: "frog", rank: "g", colors: ["g"], points: 2 }
  ];
  var lotuses = [
    { id: "lotusr", suit: "lotus", rank: "r", colors: ["r"], points: 2 },
    { id: "lotusb", suit: "lotus", rank: "b", colors: ["b"], points: 2 },
    { id: "lotusg", suit: "lotus", rank: "g", colors: ["g"], points: 2 }
  ];
  var shadows = [
    { id: "shadowr", suit: "shadow", rank: "r", colors: ["r", "k"], points: 10 },
    { id: "shadowb", suit: "shadow", rank: "b", colors: ["b", "k"], points: 10 },
    { id: "shadowg", suit: "shadow", rank: "g", colors: ["g", "k"], points: 10 }
  ];
  var sparrows = [
    { id: "sparrowr", suit: "sparrow", rank: "r", colors: ["r"], points: 2 },
    { id: "sparrowb", suit: "sparrow", rank: "b", colors: ["b"], points: 2 },
    { id: "sparrowg", suit: "sparrow", rank: "g", colors: ["g"], points: 2 }
  ];
  var phoenixes = [
    { id: "phoenix", suit: "phoenix", rank: "", colors: ["k"], points: 2 }
  ];
  var taijitu = [
    { id: "taijitur", suit: "taijitu", rank: "r", colors: ["r"], points: 8 },
    { id: "taijitug", suit: "taijitu", rank: "g", colors: ["g"], points: 8 },
    { id: "taijitub", suit: "taijitu", rank: "b", colors: ["b"], points: 8 }
  ];
  var mutations = [
    { id: "mutation1", suit: "mutation", rank: "", colors: ["r", "g"], points: 4 },
    { id: "mutation2", suit: "mutation", rank: "", colors: ["b", "r"], points: 4 },
    { id: "mutation3", suit: "mutation", rank: "", colors: ["g", "b"], points: 4 }
  ];
  var flowers = [
    { id: "flower1", suit: "flower", rank: "", colors: ["r", "g", "b"], points: 4 },
    { id: "flower2", suit: "flower", rank: "", colors: ["r", "g", "b"], points: 4 },
    { id: "flower3", suit: "flower", rank: "", colors: ["r", "g", "b"], points: 4 }
  ];
  var elements = [
    { id: "elementr", suit: "element", rank: "r", colors: ["r"], points: 5 },
    { id: "elementg", suit: "element", rank: "g", colors: ["g"], points: 5 },
    { id: "elementb", suit: "element", rank: "b", colors: ["b"], points: 5 },
    { id: "elementk", suit: "element", rank: "k", colors: ["k"], points: 5 }
  ];
  var gems = [
    { id: "gemr", suit: "gem", rank: "r", colors: ["r"], points: 6 },
    { id: "gemg", suit: "gem", rank: "g", colors: ["g"], points: 6 },
    { id: "gemb", suit: "gem", rank: "b", colors: ["b"], points: 6 },
    { id: "gemk", suit: "gem", rank: "k", colors: ["k"], points: 6 }
  ];
  var jokers = [
    { id: "joker", suit: "joker", rank: "", colors: ["g", "r", "b", "k"], points: 8 }
  ];
  var CARDS_BY_ID = c(
    [
      ...bams,
      ...cracks,
      ...dots,
      ...winds,
      ...dragons,
      ...flowers,
      ...jokers,
      ...frogs,
      ...lotuses,
      ...sparrows,
      ...rabbits,
      ...phoenixes,
      ...elements,
      ...mutations,
      ...taijitu,
      ...gems,
      ...shadows
    ],
    (c3) => c3.id
  );
  function getCard(card) {
    return CARDS_BY_ID[card];
  }
  function getPenalty(time, round2) {
    return Math.floor(time * round2.timerPoints);
  }

  // vendor/state/deckState.tsx
  function initializeDeckState(deck2) {
    batch(() => {
      deck2.update({});
      const pairs = getInitialPairs();
      for (const [i2, cards] of pairs.entries()) {
        const id = i2.toString();
        deck2.set(id, {
          id,
          cardId: cards[0].id,
          material: "bone"
        });
      }
    });
  }
  var DeckStateContext = createContext();

  // platform/observability.ts
  function captureEvent2(_name, _event) {
  }

  // tools/node_modules/nanoid/url-alphabet/index.js
  var urlAlphabet = "useandom-26T198340PX75pxJACKVERYMINDBUSHWOLF_GQZbfghjklqvwyzrict";

  // tools/node_modules/nanoid/index.browser.js
  var nanoid = (size = 21) => {
    let id = "";
    let bytes = crypto.getRandomValues(new Uint8Array(size |= 0));
    while (size--) {
      id += urlAlphabet[bytes[size] & 63];
    }
    return id;
  };

  // vendor/lib/setupTiles.ts
  function setupTiles({ rng, deck: deck2 }) {
    const tileDb = initTileDb({});
    const map = getMap(deck2.length * 2);
    for (const z of map.keys()) {
      for (const y5 of map[z].keys()) {
        for (const x of map[z][y5].keys()) {
          const id = mapGet(map, x, y5, z);
          const prev = mapGet(map, x - 1, y5, z);
          const above = mapGet(map, x, y5 - 1, z);
          const sameAsPrev = prev ? prev === id : false;
          const sameAsAbove = above ? above === id : false;
          if (id !== null && !sameAsPrev && !sameAsAbove) {
            tileDb.set(id, {
              cardId: "bam1",
              material: "bone",
              id,
              x,
              y: y5,
              z,
              deleted: false,
              selected: false
            });
          }
        }
      }
    }
    const pickOrder = [];
    while (true) {
      const freeTiles = getFreeTiles(tileDb);
      if (freeTiles.length <= 1) break;
      while (freeTiles.length > 1) {
        const idx1 = Math.floor(rng.next() * freeTiles.length);
        const tile1 = freeTiles[idx1];
        freeTiles.splice(idx1, 1);
        const idx2 = Math.floor(rng.next() * freeTiles.length);
        const tile2 = freeTiles[idx2];
        freeTiles.splice(idx2, 1);
        tileDb.del(tile1.id);
        tileDb.del(tile2.id);
        pickOrder.push(tile1, tile2);
      }
    }
    if (tileDb.size > 0) {
      return setupTiles({ rng, deck: deck2 });
    }
    const pairs = [];
    for (const deckTile of deck2) {
      pairs.push([deckTile, deckTile]);
    }
    const shuffledPairs = shuffle(pairs, rng);
    for (let i2 = 0; i2 < pickOrder.length; i2 += 2) {
      const tile1 = pickOrder[pickOrder.length - 1 - i2];
      const tile2 = pickOrder[pickOrder.length - 2 - i2];
      const pair = shuffledPairs[i2 / 2];
      const [deckTile1, deckTile2] = pair;
      const id1 = mapGet(map, tile1.x, tile1.y, tile1.z);
      const id2 = mapGet(map, tile2.x, tile2.y, tile2.z);
      tileDb.set(id1, {
        id: id1,
        cardId: deckTile1.cardId,
        material: deckTile1.material,
        x: tile1.x,
        y: tile1.y,
        z: tile1.z,
        deleted: false,
        selected: false
      });
      tileDb.set(id2, {
        id: id2,
        cardId: deckTile2.cardId,
        material: deckTile2.material,
        x: tile2.x,
        y: tile2.y,
        z: tile2.z,
        deleted: false,
        selected: false
      });
    }
    return tileDb.byId;
  }

  // vendor/state/tileState.tsx
  var TileStateContext = createContext();
  function initializeTileState(id, deck2, db) {
    const rng = new a2(id);
    const newTiles = setupTiles({ rng, deck: deck2 });
    batch(() => {
      db.update({});
      for (const tile of Object.values(newTiles)) {
        db.set(tile.id, tile);
      }
    });
  }

  // vendor/state/runState.tsx
  var TUTORIAL_SEED = "tutorial-seed";
  var ITEM_COST = 3;
  var ITEM_POOL_SIZE = 9;
  var REROLL_COST = 1;
  var ITEM_COUNT = 5;
  var PATHS = {
    r: ["garnet", "ruby"],
    g: ["jade", "emerald"],
    b: ["topaz", "sapphire"],
    k: ["quartz", "obsidian"]
  };
  var RunStateContext = createContext();
  var RoundContext = createContext();
  var LevelsContext = createContext();
  function initialRunState(id) {
    let difficulty;
    if (id === TUTORIAL_SEED) {
      difficulty = "easy";
    } else {
      const letter = id[0];
      difficulty = {
        E: "easy",
        M: "medium",
        H: "hard"
      }[letter] ?? "easy";
    }
    return {
      runId: id,
      money: 0,
      round: 1,
      stage: id === TUTORIAL_SEED ? "intro" : "game",
      retries: 0,
      attempts: 0,
      difficulty,
      totalPoints: 0,
      createdAt: Date.now(),
      items: [],
      reroll: 0,
      tutorialStep: id === TUTORIAL_SEED ? 1 : null
    };
  }
  var DIFFICULTY = {
    easy: {
      timer: { exp: 1.02, lin: 0.3 },
      point: { initial: 40, exp: 2, lin: 5 }
    },
    medium: {
      timer: { exp: 1.05, lin: 0.8 },
      point: { initial: 40, exp: 2.1, lin: 10 }
    },
    hard: {
      timer: { exp: 1.1, lin: 1 },
      point: { initial: 40, exp: 2.2, lin: 15 }
    }
  };
  function generateRound(id, run2) {
    const runId = run2.runId;
    const rng = new a2(`round-${runId}-${id}`);
    const { timer, point } = DIFFICULTY[run2.difficulty];
    function variation() {
      const rand = rng.next();
      return 1 + (rand * 2 - 1) * 0.1;
    }
    const var1 = variation();
    const var2 = variation();
    const level = id - 1;
    const timerPoints = (level * timer.lin + timer.exp ** level) / 20 * var1;
    const pointObjective = Math.round(
      (point.initial + level * point.lin + level ** point.exp) * var2
    );
    const round2 = {
      id,
      timerPoints,
      pointObjective
    };
    return round2;
  }
  function calculateIncome(run2) {
    return Math.round(4 * Math.sqrt(run2.round));
  }
  function createLevels(info) {
    return info.map(([rewards2, collection], level) => ({
      level,
      rewards: rewards2,
      tileItems: collection.flatMap(
        (cards, i2) => cards.flatMap(
          (card, j) => Array.from({ length: ITEM_POOL_SIZE }, (_, k) => ({
            id: `${level}-${i2}-${j}-${k}`,
            cardId: card.id,
            type: "tile",
            cost: ITEM_COST + Math.floor((isSuit(card.id) ? 1 : level - 1) / 2)
          }))
        )
      )
    }));
  }
  function getLevels(runId) {
    const rnd = new a2(runId);
    const nonBlackDragons = dragons.filter((c3) => c3.rank !== "k");
    const blackDragons = dragons.filter((c3) => c3.rank === "k");
    const blueShadow = shadows.find((c3) => c3.rank === "b");
    const redShadow = shadows.find((c3) => c3.rank === "r");
    const greenShadow = shadows.find((c3) => c3.rank === "g");
    const [first1, first2, first3, first4] = shuffle(
      [rabbits, frogs, lotuses, sparrows],
      rnd
    );
    const [second1, second2, second3, second4] = shuffle(
      [flowers, mutations, taijitu, phoenixes],
      rnd
    );
    const [third1, third2, third3, third4] = shuffle(
      [elements, gems, jokers, blackDragons],
      rnd
    );
    const [extra1, extra2, extra3] = shuffle([bams, cracks, dots], rnd);
    return createLevels([
      [0, [bams, cracks, dots]],
      [winds.length, [winds]],
      [1, [nonBlackDragons]],
      [0, [extra1]],
      [1, [first1]],
      [1, [first2]],
      [0, []],
      [1, [first3]],
      [1, [first4]],
      [1, [[redShadow]]],
      [0, [extra2]],
      [1, [second1]],
      [1, [second2]],
      [1, [second3]],
      [0, []],
      [1, [second4]],
      [1, [[blueShadow]]],
      [1, [third1]],
      [0, [extra3]],
      [1, [third2]],
      [1, [third3]],
      [1, [third4]],
      [0, []],
      [1, [[greenShadow]]]
    ]);
  }
  function generateItems(run2, levels2) {
    const runId = run2.runId;
    const round2 = run2.freeze?.round || run2.round;
    const rng = new a2(`items-${runId}-${round2}`);
    const itemIds = new Set(run2.items.map((i2) => i2.id));
    const initialPool = levels2.filter((l) => l.level <= round2).flatMap((l) => l.tileItems);
    const poolSize = initialPool.length;
    const reroll2 = run2.freeze?.reroll || run2.reroll;
    const start = ITEM_COUNT * reroll2 % poolSize;
    const shuffled = shuffle(initialPool, rng).filter(
      (item) => !itemIds.has(item.id)
    );
    const items = [];
    for (let i2 = 0; i2 < ITEM_COUNT; i2++) {
      const index = (start + i2) % shuffled.length;
      items.push(shuffled[index]);
    }
    return items;
  }
  function mergeCounts(count, path) {
    const order = ["bone", ...PATHS[path]];
    const newCount = { ...count };
    for (const [i2, material] of order.entries()) {
      const next = order[i2 + 1];
      if (!next) continue;
      if (!newCount[material]) continue;
      while (newCount[material] >= 3) {
        newCount[material] = (newCount[material] || 0) - 3;
        newCount[next] = (newCount[next] || 0) + 1;
        if (newCount[material] === 0) {
          delete newCount[material];
        }
      }
    }
    return newCount;
  }
  function getNextMaterials(tiles2, path) {
    const count = c2(tiles2, (tile) => tile.material);
    count.bone = (count.bone || 0) + 1;
    const newCount = mergeCounts(count, path);
    const result = [];
    for (const [material, count2] of t(newCount)) {
      for (let i2 = 0; i2 < count2; i2++) {
        result.push(material);
      }
    }
    return result;
  }
  function getNextMaterial(tiles2, path) {
    const materials = getNextMaterials(tiles2, path);
    return materials[materials.length - 1];
  }
  function getTransformation(tiles2, path) {
    const nextMaterials = getNextMaterials(tiles2, path);
    const currentMaterials = tiles2.map((tile) => tile.material);
    const updates = {};
    const removes = [];
    let adds = false;
    for (const [index, tile] of tiles2.entries()) {
      const mat = nextMaterials[index];
      if (!mat) {
        removes.push(tile.id);
        continue;
      }
      if (mat !== tile.material) {
        updates[tile.id] = mat;
      }
    }
    for (const [index, _] of nextMaterials.entries()) {
      if (!currentMaterials[index]) adds = true;
    }
    return { adds, updates, removes };
  }
  function buyTile({
    run: run2,
    item,
    deck: deck2,
    reward = false
  }) {
    const cost = reward ? 0 : item.cost;
    const money = run2.money;
    if (cost > money) throw Error("You don't have enough money");
    batch(() => {
      run2.money = money - cost;
      run2.items.push(item);
      const id = nanoid();
      deck2.set(id, { id, material: "bone", cardId: item.cardId });
    });
    if (!reward) {
      play("coin2");
      captureEvent2("tile_bought", { cardId: item.cardId });
    }
  }
  function upgradeTile({
    run: run2,
    item,
    deck: deck2,
    path
  }) {
    const cost = item.cost;
    const money = run2.money;
    if (cost > money) throw Error("You don't have enough money");
    batch(() => {
      run2.money = money - cost;
      run2.items.push(item);
      const { updates, removes } = getTransformation(
        deck2.filterBy({ cardId: item.cardId }),
        path
      );
      for (const [id, material] of t(updates)) {
        deck2.set(id, { id, material, cardId: item.cardId });
      }
      for (const id of removes) {
        deck2.del(id);
      }
    });
    play("coin2");
    captureEvent2("tile_upgraded", { cardId: item.cardId, path });
  }

  // vendor/state/gameState.tsx
  function initialGameState(id) {
    return {
      points: 0,
      coins: 0,
      time: 0,
      pause: false,
      tutorialStep: id === `${TUTORIAL_SEED}-1` ? 1 : void 0
    };
  }
  var GameStateContext = createContext();

  // platform/game.ts
  var WIDTH = 800;
  var HEIGHT = 600;
  var run;
  var game;
  var deck;
  var tiles;
  var levels;
  var round;
  var screen = "menu";
  var modal = null;
  var elapsed = 0;
  var lastTick = 0;
  var rewards = [];
  var message = "";
  var dirty = true;
  var buttons = [];
  var xy = (t3) => ({ x: 32 + t3.x * 38 + t3.z * 5, y: 94 + t3.y * 26 - t3.z * 4 });
  function button(x, y5, w, label, fn, enabled = true) {
    pxaRect(x, y5, w, 36, enabled ? 6 : 4);
    pxaText(x + 8, y5 + 11, label, enabled ? 1 : 3, 1);
    if (enabled) buttons.push({ x, y: y5, w, h: 36, fn });
  }
  function title(text) {
    pxaText(28, 23, text, 7, 2);
  }
  function mainMenu() {
    screen = "menu";
    elapsed = 0;
  }
  function newRun(difficulty) {
    run = initialRunState(difficulty + "-pxa-" + Math.floor(pxaRandom() * 1e9));
    deck = new Database(deckTileIndexes);
    initializeDeckState(deck);
    levels = getLevels(run.runId);
    startRound();
    screen = "run";
    message = "";
  }
  function startRound() {
    round = generateRound(run.round, run);
    game = initialGameState(run.runId + "-" + run.round);
    tiles = new Database(tileIndexes);
    initializeTileState(run.runId + "-" + run.round, deck.all, tiles);
    elapsed = 0;
    run.stage = "game";
    modal = null;
  }
  function win() {
    return game.endCondition === "empty-board" && game.points - getPenalty(game.time, round) >= round.pointObjective;
  }
  function nextStage() {
    if (!win()) return;
    const total = game.points - getPenalty(game.time, round);
    const extra = Math.min(Math.floor(Math.max(0, total / round.pointObjective - 1) * 2), 12);
    run.money += calculateIncome(run) + game.coins + extra;
    run.totalPoints += total;
    const level = levels.find((l) => l.level === run.round);
    run.stage = !level ? "end" : level.rewards > 0 ? "reward" : "shop";
    if (run.stage === "reward") {
      rewards = shuffle(T(level.tileItems, (i2) => i2.cardId), new a2(run.runId + "-" + run.round)).slice(0, level.rewards);
    }
    enterShop();
  }
  function enterShop() {
    if (run.stage === "shop" && run.freeze?.active) {
      const capturedFreeze = run.freeze;
      setTimeout(() => {
        capturedFreeze.active = false;
        dirty = true;
      }, 2e3);
    }
  }
  function receiveRewards() {
    for (const item of rewards) buyTile({ run, item, deck, reward: true });
    rewards = [];
    run.stage = "shop";
    enterShop();
  }
  function continueRound() {
    run.round++;
    startRound();
  }
  function purchase(item) {
    if (run.freeze?.active || item.cost > run.money || deck.size >= 163) return;
    const same = deck.filterBy({ cardId: item.cardId });
    if (getCard(item.cardId).colors.some((color) => getNextMaterial(same, color) !== "bone")) modal = item;
    else buyTile({ run, item, deck });
  }
  function upgrade(path) {
    if (modal) upgradeTile({ run, item: modal, deck, path });
    modal = null;
  }
  function reroll() {
    if (run.money < REROLL_COST) return;
    if (run.freeze && !run.freeze.active) run.freeze = void 0;
    run.money -= REROLL_COST;
    run.reroll++;
  }
  function freeze() {
    if (run.freeze) run.freeze.active = !run.freeze.active;
    else run.freeze = { round: run.round, reroll: run.reroll, active: true };
  }
  function drawTile(t3) {
    const p = xy(t3), free = isFree(tiles, t3, game);
    pxaRect(p.x + 3, p.y + 4, 70, 50, 4);
    pxaRect(p.x, p.y, 70, 50, t3.selected ? 7 : free ? 8 : 9);
    const card = getCard(t3.cardId), suit = card.suit === "bam" ? 10 : card.suit === "crack" ? 11 : card.suit === "dot" ? 12 : 1;
    pxaText(p.x + 4, p.y + 5, t3.cardId.slice(0, 8), suit, 1);
    pxaText(p.x + 4, p.y + 27, t3.material === "bone" ? "PAIR" : t3.material.slice(0, 8), free ? 1 : 3, 1);
  }
  function state() {
    const pairs = screen === "run" && run.stage === "game" ? getAvailablePairs(tiles, game).sort((a3, b) => {
      const aw = isWind(a3[0].cardId), bw = isWind(b[0].cardId);
      return aw && !bw ? 1 : !aw && bw ? -1 : b[0].z + b[1].z - a3[0].z - a3[1].z;
    }) : [];
    const pair = pairs[0]?.map((t3) => {
      const p = xy(t3);
      return { id: t3.id, card: t3.cardId, x: p.x + 35, y: p.y + 20 };
    }) || [];
    return {
      screen,
      stage: run?.stage || "menu",
      seed: run?.runId,
      round: run?.round || 0,
      points: game?.points || 0,
      time: game?.time || 0,
      penalty: game ? getPenalty(game.time, round) : 0,
      objective: round?.pointObjective || 0,
      money: run?.money || 0,
      deck: deck?.size || 0,
      freeze: run?.freeze ? { ...run.freeze } : null,
      reroll: run?.reroll || 0,
      retries: run?.retries || 0,
      remaining: tiles?.filterBy({ deleted: false }).length || 0,
      pairs: pairs.length,
      pair,
      condition: game?.endCondition || null,
      win: game ? win() : false,
      rewards: rewards.map((i2) => i2.cardId),
      items: run?.stage === "shop" ? generateItems(run, levels).map((i2) => ({ card: i2.cardId, cost: i2.cost, id: i2.id })) : [],
      upgraded: deck?.all.filter((t3) => t3.material !== "bone").map((t3) => ({ card: t3.cardId, material: t3.material })) || []
    };
  }
  function trace() {
    pxaTrace("WHATAJONG STATE " + JSON.stringify(state()));
  }
  function render() {
    buttons = [];
    pxaRect(0, 0, WIDTH, HEIGHT, 0);
    if (screen === "menu") {
      title("WHATAJONG");
      pxaText(30, 84, "Original 24-round mahjong roguelike", 8, 2);
      pxaText(30, 124, "Match free pairs. Beat each score objective.", 8, 1);
      pxaText(30, 148, "Earn coins, collect special tiles, upgrade materials.", 8, 1);
      button(42, 214, 210, "NEW EASY RUN", () => newRun("E"));
      button(42, 266, 210, "NEW MEDIUM RUN", () => newRun("M"));
      button(42, 318, 210, "NEW HARD RUN", () => newRun("H"));
      if (run) button(42, 370, 210, "RESUME", () => {
        screen = "run";
        enterShop();
      });
      button(42, 438, 210, "HOW TO PLAY", () => screen = "help");
      pxaText(30, 534, "MIT rules / maps by Pao Ramon. PXA text frontend.", 3, 1);
      pxaText(30, 555, "Touch controls. Silent. Save state lasts this session.", 3, 1);
    } else if (screen === "help") {
      title("HOW TO PLAY");
      const lines = [
        "Tap a free tile, then a matching free tile.",
        "A tile is free if uncovered and open on its left or right.",
        "Topaz / sapphire can ignore the side constraint.",
        "Clear every tile AND reach the round point objective.",
        "Time subtracts points; dark materials pause the timer.",
        "Coins and round rewards buy pairs or material upgrades.",
        "Special cards preserve original wind / dragon / gem rules.",
        "BAM = bamboo, CRACK = characters, DOT = circles.",
        "Materials / special cards use their original identifiers.",
        "The shop has reroll ($1), freeze, and all color upgrades.",
        "MENU keeps the run; NEW RUN resets it. HOME exits."
      ];
      lines.forEach((line, i2) => pxaText(30, 90 + i2 * 30, line, 8, 1));
      button(30, 500, 170, "BACK", () => screen = "menu");
    } else {
      title("ROUND " + run.round + " / 24  " + run.stage.toUpperCase());
      button(672, 18, 100, "MENU", mainMenu);
      pxaText(30, 66, "Points " + game.points + "  Penalty " + getPenalty(game.time, round) + "  Goal " + round.pointObjective + "  $" + run.money, 8, 1);
      if (run.stage === "game") {
        for (const t3 of tiles.filterBy({ deleted: false }).sort((a3, b) => a3.z - b.z || a3.y - b.y)) drawTile(t3);
        if (game.endCondition) {
          pxaRect(135, 188, 540, 183, 2);
          pxaText(162, 217, win() ? "ROUND WON" : "ROUND LOST", 7, 2);
          pxaText(162, 254, "Final score " + (game.points - getPenalty(game.time, round)) + " / " + round.pointObjective, 8, 1);
          pxaText(162, 278, "Result: " + game.endCondition, 8, 1);
          button(162, 312, 160, win() ? "CONTINUE" : "RETRY", () => {
            if (win()) nextStage();
            else {
              run.retries++;
              startRound();
            }
          });
        } else {
          pxaText(30, 537, "Available pairs " + getAvailablePairs(tiles, game).length + "  Time " + game.time + "s", 8, 1);
          pxaText(530, 541, game.pause ? "TIMER PAUSED BY MATERIAL" : "TIMER RUNNING", 8, 1);
        }
      } else if (run.stage === "reward") {
        title("ROUND " + run.round + " REWARD");
        pxaText(40, 118, "Free original special tile pairs:", 8, 1);
        rewards.forEach((i2, n2) => pxaText(45, 166 + n2 * 42, i2.cardId, 7, 2));
        button(40, 420, 250, "COLLECT / GO TO SHOP", receiveRewards);
      } else if (run.stage === "shop") {
        pxaText(30, 111, "Deck pairs " + deck.size + " / 163. Tap item to buy or upgrade.", 8, 1);
        generateItems(run, levels).forEach((item, i2) => {
          pxaText(30, 163 + i2 * 55, item.cardId + "  $" + item.cost, 8, 1);
          button(260, 151 + i2 * 55, 230, "BUY / UPGRADE", () => purchase(item), !run.freeze?.active && item.cost <= run.money && deck.size < 163);
        });
        button(530, 151, 230, "REROLL $1", reroll, run.money >= REROLL_COST);
        button(530, 207, 230, run.freeze?.active ? "UNFREEZE" : "FREEZE SHOP", freeze);
        button(530, 431, 230, "NEXT ROUND", continueRound);
        pxaText(30, 482, "Upgrades merge 3 same materials into the next tier.", 3, 1);
        if (modal) {
          buttons = [];
          pxaRect(120, 172, 560, 258, 2);
          pxaText(145, 192, "CHOOSE MATERIAL PATH", 7, 2);
          const card = getCard(modal.cardId);
          card.colors.forEach((color, i2) => button(145, 231 + i2 * 40, 420, color + " -> " + getNextMaterial(deck.filterBy({ cardId: modal.cardId }), color), () => upgrade(color)));
          button(145, 386, 160, "CANCEL", () => modal = null);
        }
      } else {
        pxaText(80, 170, "RUN COMPLETE! Total points " + run.totalPoints, 7, 2);
        button(80, 280, 200, "MAIN MENU", mainMenu);
      }
      if (message) pxaText(30, 580, message.slice(0, 90), 11, 1);
    }
    dirty = false;
  }
  function touch(x, y5) {
    const hit = [...buttons].reverse().find((b) => x >= b.x && x < b.x + b.w && y5 >= b.y && y5 < b.y + b.h);
    try {
      if (hit) hit.fn();
      else if (screen === "run" && run.stage === "game" && !game.endCondition) {
        const tile = tiles.filterBy({ deleted: false }).sort((a3, b) => b.z - a3.z || b.y - a3.y).find((t3) => {
          const p = xy(t3);
          return x >= p.x && x < p.x + 70 && y5 >= p.y && y5 < p.y + 50;
        });
        if (tile && isFree(tiles, tile, game)) selectTile({ tileDb: tiles, game, tileId: tile.id });
      }
      message = "";
    } catch (error) {
      message = String(error);
      pxaTrace("WHATAJONG ERROR " + message);
    }
    dirty = true;
    render();
    trace();
  }
  function tick(ms) {
    if (__pxaAdvanceTimers(ms)) dirty = true;
    const delta = lastTick ? Math.max(0, ms - lastTick) : 0;
    lastTick = ms;
    if (screen === "run" && run.stage === "game" && !game.pause && !game.endCondition) {
      elapsed += delta;
      if (elapsed >= 1e3) {
        game.time += Math.floor(elapsed / 1e3);
        elapsed %= 1e3;
        dirty = true;
      }
    }
    if (dirty) render();
  }
  Object.assign(globalThis, { whatajongTouch: touch, whatajongTick: tick, whatajongRender: render });
  render();
  trace();
})();

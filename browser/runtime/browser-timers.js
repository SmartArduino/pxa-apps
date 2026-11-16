// App-owned timer queue, instantiated only on first registration.
(function(core) {
    'use strict';
    const {root,native,apply:timerApply}=core;
    let nextTimer=1,activeInterval=null;const timers=[];
    // Keep at most eight records ordered by due time. Updating the queue does
    // not allocate map/filter/spread/splice result arrays on every timer turn.
    function schedule() { native.wake(timers.length ? timers[0].at : -1); }
    function timerIndex(id) {
        for(let i=0;i<timers.length;++i)if(timers[i].id===id)return i;
        return -1;
    }
    function insertTimer(timer) {
        let at=timers.length;
        while(at && timers[at-1].at>timer.at){timers[at]=timers[at-1];--at;}
        timers[at]=timer;
    }
    function removeTimer(at) {
        for(let i=at+1;i<timers.length;++i)timers[i-1]=timers[i];
        --timers.length;
    }
    function createTimer(fn, delay, args, repeat) {
        if (typeof fn !== 'function') throw new TypeError('function timer required');
        // Web IDL long conversion, once, before observing the queue: valueOf
        // may register other timers and must not bypass the existing quota.
        let timeout=delay|0;if(timeout<0)timeout=0;
        if (timers.length + (activeInterval && activeInterval.interval ? 1 : 0) >= 8) throw new RangeError('timer limit');
        let id;
        do {id=nextTimer++;if(nextTimer>2147483647)nextTimer=1;}
        while(timerIndex(id)>=0 || activeInterval && activeInterval.id===id);
        insertTimer({id,fn,args,at:native.now()+timeout,interval:repeat?(timeout<20?20:timeout):0});
        schedule();return id;
    }
    function cancel(id) {
        const at=timerIndex(id);
        if(at>=0)removeTimer(at);
        if(activeInterval && activeInterval.id===id)activeInterval.interval=0;
        schedule();
    }
    function pump() {
        if (timers.length && timers[0].at<=native.now()) {
            const timer=timers[0];removeTimer(0);
            if(timer.interval)activeInterval=timer;
            schedule();
            try {timerApply(timer.fn,root,timer.args);}
            finally {
                if(activeInterval===timer) {
                    activeInterval=null;
                    if(timer.interval){timer.at=native.now()+timer.interval;insertTimer(timer);}
                    schedule();
                }
            }
        }
    }
    return {create:createTimer,cancel,pump};
})

import itertools

from common import *

name = "PropagateStable"
n_states = 8
n_neighbors = 8

OFF = 0
ON = 1
UNKNOWN = 2
OFFSIGNALOFF = 3
ONSIGNALOFF = 4
OFFSIGNALON = 5
ONSIGNALON = 6
ABORT = 7

def underlying(cell):
    if cell == OFF or cell == OFFSIGNALOFF or cell == OFFSIGNALON:
        return OFF
    if cell == ON or cell == ONSIGNALOFF or cell == ONSIGNALON:
        return ON
    return cell

def life_stable(center, count):
    if center == ON:
        return count == 2 or count == 3
    if center == OFF:
        return not count == 3

def propagate_function(center, oncount, unkcount):
    lower = oncount
    upper = oncount + unkcount + 1

    r = range(lower, upper)
    o = []
    for i in r:
        this_on = False
        this_off = False
        if underlying(center) == ON or center == UNKNOWN:
            if life_stable(ON, i):
                this_on = True
        if underlying(center) == OFF or center == UNKNOWN:
            if life_stable(OFF, i):
                this_off = True
        if this_on and this_off:
            o.append(UNKNOWN)
        if this_on and not this_off:
            o.append(ON)
        if not this_on and this_off:
            o.append(OFF)
        if not this_on and not this_off:
            o.append(ABORT)

    maybe_on = any([c == ON or c == UNKNOWN for c in o])
    maybe_off = any([c == OFF or c == UNKNOWN for c in o])

    if center == UNKNOWN:
        if maybe_on and not maybe_off:
            return ON
        if not maybe_on and maybe_off:
            return OFF

    if underlying(center) == ON and not maybe_on:
        return ABORT
    if underlying(center) == OFF and not maybe_off:
        return ABORT

    if unkcount > 0:
        if center == ON and o[-1] == ON and all([c == OFF or c == ABORT for c in o[0:-1]]):
            return ONSIGNALON
        if center == OFF and o[-1] == OFF and all([c == ON or c == ABORT for c in o[0:-1]]):
            return OFFSIGNALON
        if center == ON and o[0] == ON and all([c == OFF or c == ABORT for c in o[1:]]):
            return ONSIGNALOFF
        if center == OFF and o[0] == OFF and all([c == ON or c == ABORT for c in o[1:]]):
            return OFFSIGNALOFF

    if unkcount == 0:
        return underlying(center)

    return center

def stateresult2string(state, result):
    if state == UNKNOWN and result == OFF:
        return "10000"
    if state == UNKNOWN and result == ON:
        return "01000"
    if state == UNKNOWN:
        return "00000"
    if result == OFFSIGNALOFF:
        return "--100"
    if result == ONSIGNALOFF:
        return "--100"
    if result == OFFSIGNALON:
        return "--010"
    if result == ONSIGNALON:
        return "--010"
    if result == ABORT:
        return "----1"
    return "--000"

def emit_boolean(state, live_count, unknown_count, result, rdigs=5):
    inputs = int2bin(state, 2) + \
        int2bin(live_count, 3) + int2bin(unknown_count, 3)
    outputs = stateresult2string(state, result)

    return f"{inputs} {outputs}\n"

def emit_rule(live_count, unknown_count):
    if live_count > 7 or unknown_count > 3: return ""
    result = ""

    result += emit_boolean(OFF, live_count, unknown_count,
                           propagate_function(OFF, live_count, unknown_count))
    if live_count > 0:
        result += emit_boolean(ON, live_count, unknown_count,
                               propagate_function(ON, live_count-1, unknown_count))

    if unknown_count > 0:
        result += emit_boolean(UNKNOWN, live_count, unknown_count,
                               propagate_function(UNKNOWN, live_count, unknown_count-1))

    return result

def make_propagate_rule():
    data = """.i 8
.o 5
.type fr
"""
    for live_count in range(0,10):
        for unknown_count in range(0,10-live_count):
            data += emit_rule(live_count, unknown_count)

    innames = ["state1", "state0", "on2", "on1", "on0", "unk2", "unk1", "unk0"]
    outnames = ["set_off", "set_on", "signal_off", "signal_on", "abort"]

    run_espresso(data, innames, outnames)

make_propagate_rule()

# For golly
def transition_function(s):
    if any([c == ABORT for c in s]):
        return ABORT

    center = s[8]

    oncount = 0
    unkcount = 0
    signalon = False
    signaloff = False
    for c in s[0:8]:
        if underlying(c) == ON:
            oncount += 1
        if c == UNKNOWN:
            unkcount += 1
        if c == OFFSIGNALON or c == ONSIGNALON:
            signalon = True
        if c == OFFSIGNALOFF or c == ONSIGNALOFF:
            signaloff = True

    if center == UNKNOWN:
        if signalon and signaloff: return ABORT
        if signalon: return ON
        if signaloff: return OFF

    return propagate_function(center, oncount, unkcount)

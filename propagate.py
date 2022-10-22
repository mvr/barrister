# from golchemy.basics import *

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

# @COLORS
# 0 0 0 0
# 1 255 255 255
# 2 60 60 60
# 3 80 0 0
# 4 255 200 200
# 5 0 0 80
# 6 200 200 255
# 7 255 0 0

def underlying(cell):
    if cell == OFF or cell == OFFSIGNALOFF or cell == OFFSIGNALON:
        return OFF
    if cell == ON or cell == ONSIGNALOFF or cell == ONSIGNALON:
        return ON
    return cell

# def bits_to_num(state, background, active, known):
#     x = 0
#     if state: x += 1
#     if background: x += 2
#     if active: x += 4
#     if known: x += 8
#     return x

# def num_to_bits(n):
#     return (n >> 0) % 2 == 1, (n >> 1) % 2 == 1, (n >> 2) % 2 == 1, (n >> 3) % 2 == 1

def life_rule(center, count):
    if center == ON:
        return count == 2 or count == 3
    if center == OFF:
        return count == 3

def life_stable(center, count):
    if center == ON:
        return count == 2 or count == 3
    if center == OFF:
        return not count == 3


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

    # statecount = 0
    # bgcount = 0
    # activecount = 0
    # knowncount = 0
    # for c in s[0:8]:
    #     state, bg, active, known = num_to_bits(c)
    #     if state: statecount += 1
    #     if bg: bgcount += 1
    #     if active: activecount += 1
    #     if known: knowncount += 1

    # if center == 0 and statecount == 3: return 1
    # if center == 0: return 0

    # if center == 1 and (statecount == 3 or statecount == 2): return 1
    # if center == 1: return 0
    # return 0

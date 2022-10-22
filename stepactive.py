# from golchemy.basics import *

name = "StepActive"
n_states = 6
n_neighbors = 8

# active by default
OFFSTABLE = 0
ONSTABLE = 1
UNKNOWNSTABLE = 2
OFF = 3
ON = 4
UNKNOWN = 5

# @COLORS
# 0 0 0 0
# 1 255 255 255
# 2 60 60 60
# 3 0 0 50
# 4 200 200 255
# 5 60 60 100

def underlying(cell):
    if cell == OFFSTABLE or cell == OFF:
        return OFF
    if cell == ONSTABLE or cell == ON:
        return ON
    if cell == UNKNOWN or cell == UNKNOWNSTABLE:
        return UNKNOWN
    return cell

def life_rule(center, count):
    if center == ON:
        if count == 2 or count == 3: return ON
        else: return OFF
    if center == OFF:
        if count == 3: return ON
        else: return OFF

def life_stable(center, count):
    if center == ON:
        return count == 2 or count == 3
    if center == OFF:
        return not count == 3

def transition_function(s):
    center = s[8]
    u = underlying(center)

    oncount = 0
    unkcount = 0
    stablecount = 0
    for c in s[0:8]:
        if underlying(c) == ON:
            oncount += 1
        if c == UNKNOWN or c == UNKNOWNSTABLE:
            unkcount += 1
        if c == OFFSTABLE or c == ONSTABLE or c == UNKNOWNSTABLE:
            stablecount += 1

    if stablecount == 8:
        return center

    lower = oncount
    upper = oncount + unkcount + 1

    maybe_on = False
    maybe_off = False
    stable_on = True
    stable_off = True

    r = range(lower, upper)
    for i in r:
        if u == ON or u == UNKNOWN:
            if life_rule(ON, i) == ON:
                maybe_on = True
            if life_rule(ON, i) == OFF:
                maybe_off = True
            if not life_stable(ON, i):
                stable_on = False
        if u == OFF or u == UNKNOWN:
            if life_rule(OFF, i) == ON:
                maybe_on = True
            if life_rule(OFF, i) == OFF:
                maybe_off = True
            if not life_stable(OFF, i):
                stable_off = False

    next = None
    if maybe_on and maybe_off:
        next = UNKNOWN
    if not maybe_on and maybe_off:
        next = OFF
    if maybe_on and not maybe_off:
        next = ON

    if next is None:
        raise Exception(s)

    if center == ONSTABLE and next == ON:
        return ONSTABLE
    if center == OFFSTABLE and next == OFF:
        return OFFSTABLE
    if center == UNKNOWNSTABLE and stable_on and stable_off:
        return UNKNOWNSTABLE

    return next

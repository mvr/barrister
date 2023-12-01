from common import *

def stepactive_function(center, oncount, unkcount):
    if center == UNKNOWN: return UNKNOWN

    u = center

    lower = oncount
    upper = oncount + unkcount

    maybe_on = False
    maybe_off = False

    r = range(lower, upper + 1)
    for i in r:
        if u == ON or u == UNKNOWN:
            if life_rule(ON, i) == ON:
                maybe_on = True
            if life_rule(ON, i) == OFF:
                maybe_off = True
        if u == OFF or u == UNKNOWN:
            if life_rule(OFF, i) == ON:
                maybe_on = True
            if life_rule(OFF, i) == OFF:
                maybe_off = True

    next = None
    if maybe_on and maybe_off:
        next = UNKNOWN
    if not maybe_on and maybe_off:
        next = OFF
    if maybe_on and not maybe_off:
        next = ON

    if next is None:
        raise Exception(center,oncount,unkcount)

    return next

def emit_boolean(state, live_count, unknown_count, result):
    inputs = int2bin(state, 2) + \
        int2bin(live_count, 4) + \
        int2bin(unknown_count, 4)
    outputs = int2bin(result, 2)

    return f"{inputs} {outputs}\n"

def remove(state, live_count, unknown_count):
    if state == OFF:
        return live_count, unknown_count
    if state == ON:
        return live_count - 1, unknown_count
    if state == UNKNOWN:
        return live_count, unknown_count - 1

innames = ["current_unknown", "current_on",
           "on3", "on2", "on1", "on0",
           "unk3", "unk2", "unk1", "unk0"]
outnames = ["naive_next_unknown", "naive_next_on"]
data = ""

for c in [OFF, ON, UNKNOWN]:
    for live_count in range(0,10):
        if c == ON and live_count == 0: continue
        for unknown_count in range(0, 10-live_count):
            if c == UNKNOWN and unknown_count == 0: continue

            live_neighbours = live_count
            unknown_neighbours = unknown_count
            if c == ON: live_neighbours -= 1
            if c == UNKNOWN: unknown_neighbours -= 1

            result = stepactive_function(c, live_neighbours, unknown_neighbours)
            data += emit_boolean(c, live_count, unknown_count, result)

run_espresso(data, innames, outnames)

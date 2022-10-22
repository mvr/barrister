import itertools
import subprocess

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

def int2bin(n, count=24):
    """returns the binary of integer n, using count number of digits"""
    return "".join([str((n >> y) & 1) for y in range(count-1, -1, -1)])

def stateresult2string(state, result):
    if state == UNKNOWN and result == OFF:
        return "10000"
    if state == UNKNOWN and result == ON:
        return "01000"
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
    return "-----"

def emit_boolean(state, u_on, c_on, l_on, u_unk, c_unk, l_unk, result, rdigs=5):
    inputs = int2bin(state, 2) + \
        int2bin(u_on, 2) + int2bin(c_on, 2) + int2bin(l_on, 2) + \
        int2bin(u_unk, 2) + int2bin(c_unk, 2) + int2bin(l_unk, 2)
    outputs = stateresult2string(state, result)

    return f"{inputs} {outputs}\n"

def emit_rule(u_on, c_on, l_on, u_unk, c_unk, l_unk):
    result = ""

    # this is the whole 3x3 square, so we have to -1 below
    live_count = u_on + c_on + l_on
    unknown_count = u_unk + c_unk + l_unk

    if c_on < 3:
        result += emit_boolean(OFF, u_on, c_on, l_on, u_unk, c_unk, l_unk,
                               propagate_function(OFF, live_count, unknown_count))

    if c_on > 0:
        result += emit_boolean(ON, u_on, c_on, l_on, u_unk, c_unk, l_unk,
                               propagate_function(ON, live_count-1, unknown_count))

    if c_unk > 0:
        result += emit_boolean(UNKNOWN, u_on, c_on, l_on, u_unk, c_unk, l_unk,
                               propagate_function(UNKNOWN, live_count, unknown_count-1))

    return result

def make_propagate_rule():
    data = """.i 14
.o 5
.type fr
"""
    for u_on, c_on, l_on, u_unk, c_unk, l_unk in itertools.product(range(0,4), repeat=6):
        data += emit_rule(u_on, c_on, l_on, u_unk, c_unk, l_unk)

    innames = ["state1", "state0", "u_on1", "u_on0", "c_on1", "c_on0", "l_on1", "l_on0", "u_unk1", "u_unk0", "c_unk1", "c_unk0", "l_unk1", "l_unk0"]
    outnames = ["set_off", "set_on", "signal_off", "signal_on", "abort"]

    run_espresso(data, innames, outnames)

def run_espresso(data, innames, outnames):
    print(data)
    p = subprocess.run(["./espresso", "-Dexact"],
                       text = True,
                       input = data,
                       capture_output = True)
    out = p.stdout
    print(out)
    lines = out.split("\n")
    lines = [l for l in lines if len(l) > 0 and l[0] != '.']

    print_output(lines, innames, outnames)

def print_output(lines, innames, outnames):
    for term in lines:
        ins, outs = term.split(" ")
        code = []
        for val, name in zip(ins, innames):
            if val == "0":
                code.append("(~" + name + ")")
            elif val == "1":
                code.append(name)
        code = " & ".join(code)
        for val, name in zip(outs, outnames):
            if val == "1":
                print(name, "|=", code, ";")

make_propagate_rule()

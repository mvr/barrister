from common import *

def new_signal_function(o, n):
    if n.unknown == 0: return "--"

    o2 = o.restrict_to(n)
    if o2 == impossible: return "--"

    n2 = n.restrict_to(o2)
    if n2 is None: return "--"

    if n2.unknown == 0:
        if n2.count == n.count:
            return "01"
        if n2.count == n.count + n.unknown:
            return "10"

    return "00"

def emit_boolean(center, options, live_count, unknown_count, result):
    if result == ABORT: return ""
    inputs = int2bin(center, 2) + \
             options.espresso_str() + \
             int2bin(live_count, 3) + \
             int2bin(live_count + unknown_count, 4)
    outputs = result

    return f"{inputs} {outputs}\n"

innames = ["stateunk", "stateon", "l2", "l3", "d0", "d1", "d2", "d4", "d5", "d6",
           "s2", "s1", "s0",
           # "unk3", "unk2", "unk1", "unk0",
           "m3", "m2", "m1", "m0",
           ]
outnames = ["signalon", "signaloff"]
data = ""

impossible = StableOptions(True, True, True, True, True, True, True, True)

for c in [OFF, ON, UNKNOWN]:
    for unknown_count in range(0, 10):
        if c == UNKNOWN and unknown_count == 0: continue
        for stab_count in range(0, 10 - unknown_count):
            if c == ON and stab_count == 0: continue

            stab_neighbours = stab_count
            unknown_neighbours = unknown_count
            if c == ON: stab_neighbours -= 1
            if c == UNKNOWN: unknown_neighbours -= 1

            for o in StableOptions.all_possible():
                n = CellUnknownNeighbourhood(c, stab_neighbours, unknown_neighbours)

                result = new_signal_function(o, n)
                data += emit_boolean(c, o, stab_count, unknown_count, result)

run_espresso(data, innames, outnames)

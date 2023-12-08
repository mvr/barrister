from common import *

impossible = StableOptions(True, True, True, True, True, True, True, True)

def is_forced(o, n):
    center_unknown = n.center == UNKNOWN

    o2 = o.restrict_to(n)
    if o2 == impossible: return None

    n2 = n.restrict_to(o2)
    if n2 is None: return None

    return n2.unknown == 0 or (center_unknown and n2.center != UNKNOWN)

def vulnerable_function(o, n):
    if n.unknown <= 1: return "0"

    n_on = CellUnknownNeighbourhood(n.center, n.count+1, n.unknown-1)
    n_off = CellUnknownNeighbourhood(n.center, n.count, n.unknown-1)

    forced_on  = is_forced(o, n_on)
    forced_off = is_forced(o, n_off)

    if forced_on is None: return "1"
    if forced_off is None: return "1"
    if forced_on and forced_off: return "1"

    return "0"

def vulnerable_center_function(o, n):
    if n.unknown == 0: return "0"
    if n.center != UNKNOWN: return "-"

    n_on = CellUnknownNeighbourhood(ON, n.count, n.unknown)
    n_off = CellUnknownNeighbourhood(OFF, n.count, n.unknown)

    forced_on  = is_forced(o, n_on)
    forced_off = is_forced(o, n_off)

    if forced_on is None: return "1"
    if forced_off is None: return "1"
    if forced_on and forced_off: return "1"

    return "0"

def emit_boolean(options, live_count, unknown_count, result):
    if result == ABORT: return ""
    inputs = options.espresso_str() + \
             int2bin(live_count, 3) + \
             int2bin(unknown_count, 4) + \
             ""
             # espresso_char(options.single_off()) + espresso_char(options.single_on()) + \
    outputs = result

    return f"{inputs} {outputs}\n"

innames = ["l2", "l3", "d0", "d1", "d2", "d4", "d5", "d6",
           "s2", "s1", "s0",
           "unk3", "unk2", "unk1", "unk0",
           # "single_off", "single_on",
           ]
outnames = ["vulnerable", "vulnerable_center"]
data = ""


for c in [OFF, ON, UNKNOWN]:
    for unknown_count in range(0, 10):
        if c == UNKNOWN and unknown_count == 0: continue
        for stab_count in range(0, 10 - unknown_count):
            if c == ON and stab_count == 0: continue

            stab_neighbours = stab_count
            unknown_neighbours = unknown_count
            if c == ON: stab_neighbours -= 1
            if c == UNKNOWN: unknown_neighbours -= 1

            n = CellUnknownNeighbourhood(c, stab_neighbours, unknown_neighbours)

            for o in StableOptions.compatible_options(n):
                result = vulnerable_function(o, n) + vulnerable_center_function(o, n)
                data += emit_boolean(o, stab_count, unknown_count, result)

run_espresso(data, innames, outnames)

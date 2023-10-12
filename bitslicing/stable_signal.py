from common import *

def signal_neighbourhood(center, stable_count, unknown_count):
    if unknown_count == 0: return "--"
    if center.neighbours == stable_count: return "01"
    if center.neighbours == stable_count + unknown_count: return "10"
    return "00"

def signal_function(center_options, stable_count, unknown_count):
    if unknown_count == 0: return "--"

    signals = []
    for n in center_options.possible_neighbourhoods():
        signals.append(signal_neighbourhood(n, stable_count, unknown_count))

    if len(signals) == 0: return "--"

    if signals.count(signals[0]) == len(signals):
        return signals[0]

    return "00"

def emit_boolean(options, live_count, unknown_count, result):
    if result == ABORT: return ""
    inputs = options.espresso_str() + \
        int2bin(live_count, 3) + int2bin(unknown_count, 4)
    outputs = result

    return f"{inputs} {outputs}\n"

innames = ["l2", "l3", "d0", "d1", "d2", "d4", "d5", "d6",
           "s2", "s1", "s0",
           "unk3", "unk2", "unk1", "unk0"]
outnames = ["signalon", "signaloff"]
data = f""".i {len(innames)}
.o {len(outnames)}
.type fr
"""

for c in [OFF, ON, UNKNOWN]:
    for unknown_count in range(0, 9):
        if c == UNKNOWN and unknown_count == 0: continue
        for stab_count in range(0, 10 - unknown_count):
            if c == ON and stab_count == 0: continue

            stab_neighbours = stab_count
            unknown_neighbours = unknown_count
            if c == ON: stab_neighbours -= 1
            if c == UNKNOWN: unknown_neighbours -= 1

            for o in StableOptions.compatible_options(c, stab_neighbours, unknown_neighbours):
                result = signal_function(o, stab_neighbours, unknown_neighbours)
                data += emit_boolean(o, stab_count, unknown_count, result)

run_espresso(data, innames, outnames)

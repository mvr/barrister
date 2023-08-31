# If there is an active unknown unknown in the neighbourhood, things
# get much harder. To keep things feasible, let us assume all unknowns
# are stable unknowns.

# Think of this as a more intelligent correction step after the normal
# StepUnknown. The result will only be applied to cells that go from
# ON/OFF to UNKNOWN

from common import *

def step_neighbourhood(center, stable_neighbours, live_neighbours, unknown_neighbours):
    unknown_ons = center.neighbours - stable_neighbours
    current_ons = live_neighbours + unknown_ons
    return life_rule(center.center, current_ons)

def keep_stable_function(center_options, stable_neighbours, live_neighbours, unknown_neighbours):
    # center_state = center_options.to_three_state()
    # if center_state == UNKNOWN:
    #     raise Exception("can't do unknown")

    for n in center_options.possible_neighbourhoods():
        if step_neighbourhood(n, stable_neighbours, live_neighbours, unknown_neighbours) != n.center:
            return False

    return True

def emit_boolean(state, stable_count, live_count, unknown_count, result):
    inputs = state.espresso_str() \
        + int2bin(stable_count, 3) \
        + int2bin(live_count, 4) \
        + int2bin(unknown_count, 4)
    outputs = espresso_char(result)

    return f"{inputs} {outputs}\n"

innames = ["l2", "l3", "d0", "d1", "d2", "d4", "d5", "d6",
           "s2", "s1", "s0",
           "on3", "on2", "on1", "on0",
           "unk3", "unk2", "unk1", "unk0"]
outnames = ["keep_stable"]
data = f""".i {len(innames)}
.o {len(outnames)}
.type fr
"""

for c in [OFF, ON, UNKNOWN]:
# for c in [OFF, ON]:
    for unknown_count in range(0, 9+1):
        if c == UNKNOWN and unknown_count == 0: continue
        for stab_count in range(0, 9+1 - unknown_count):
            if c == ON and stab_count == 0: continue
            for live_count in range(0, 9+1 - unknown_count):

                stab_neighbours = stab_count
                live_neighbours = live_count
                unknown_neighbours = unknown_count

                if c == ON:
                    stab_neighbours -= 1
                    live_neighbours -= 1
                if c == UNKNOWN:
                    unknown_neighbours -= 1

                for o in StableOptions.compatible_options(c, stab_neighbours, unknown_neighbours):
                    result = keep_stable_function(o, stab_neighbours, live_neighbours, unknown_neighbours)
                    data += emit_boolean(o, stab_count, live_count, unknown_count, result)

run_espresso(data, innames, outnames)

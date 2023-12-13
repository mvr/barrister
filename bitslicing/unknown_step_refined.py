# If there is an active unknown unknown in the neighbourhood, things
# get much harder. To keep things feasible, let us assume all unknowns
# are stable unknowns.

from common import *

def naive_stepactive_function(center, oncount, unkcount):
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

def step_neighbourhood(stable_neighbourhood, current_center, stable_neighbours, live_neighbours, unknown_neighbours):
    unknown_ons = stable_neighbourhood.count - stable_neighbours
    current_ons = live_neighbours + unknown_ons

    return life_rule(current_center, current_ons)

def unknown_step_function(stable_options, current_center, stable_neighbours, live_neighbours, unknown_neighbours):
    maybe_on = False
    maybe_off = False
    maybe_unstable = False

    for n in stable_options.possible_neighbourhoods():
        actual_current_center = current_center
        if current_center == UNKNOWN:
            actual_current_center = n.center

        stepped = step_neighbourhood(n, actual_current_center, stable_neighbours, live_neighbours, unknown_neighbours)
        if stepped == ON:
            maybe_on = True
        if stepped == OFF:
            maybe_off = True
        if stepped != n.center:
            maybe_unstable = True

    # Unknown but stable
    # We don't ever want an unknown cell to become known
    if stable_options.to_three_state() == UNKNOWN:
        if maybe_unstable:
            return DONTCARE, DONTCARE, False
        else:
            return DONTCARE, DONTCARE, True

    if maybe_on and not maybe_off:
        return True, False, DONTCARE

    if not maybe_on and maybe_off:
        return False, False, DONTCARE

    if maybe_on and maybe_off:
        return DONTCARE, True, DONTCARE

    print("shouldn't happen")
    exit(0)

# print(unknown_step_function(StableOptions(False, True, True, True, False, True, True, False), UNKNOWN, 2, 0, None))
# exit(0)

def emit_boolean(state, current_center, stable_center, stable_count, live_count, unknown_count, result):
    if result[0] == DONTCARE and result[1] == DONTCARE and result[2] == DONTCARE: return ""
    diff = stable_count - live_count
    inputs = ""
    inputs += state.espresso_str()
    inputs += int2bin(current_center, 2)
    # inputs += int2bin(stable_center, 2)
    inputs += int2bin(stable_count, 3)
    # inputs += int2bin(live_count, 4)
    inputs += int2twos(diff, 4)
    # inputs += espresso_char(state.single_off()) + espresso_char(state.single_on())
    # inputs += int2bin(unknown_count, 4)
    outputs = espresso_char(result[0]) + espresso_char(result[1]) + espresso_char(result[2])
    # outputs = espresso_char(result[0]) + espresso_char(result[2])

    return f"{inputs} {outputs}\n"

innames = ["l2", "l3", "d0", "d1", "d2", "d4", "d5", "d6",
           "current_unknown", "current_on", 
           # "stable_unknown", "stable_on",
           "s2", "s1", "s0",
           # "on3", "on2", "on1", "on0",
           "m3", "m2", "m1", "m0",
           # "singleoff", "singleon",
           ]
outnames = ["next_on",
            "next_unknown",
            "next_unknown_stable"]
data = ""

for stable_center in [OFF, ON, UNKNOWN]:
    for current_center in [OFF, ON, UNKNOWN]:
        if stable_center == UNKNOWN and current_center != UNKNOWN: continue
        if stable_center != UNKNOWN and current_center == UNKNOWN: continue
        for unknown_count in range(0, 9+1):
            if stable_center == UNKNOWN and unknown_count == 0: continue
            for stab_count in range(0, 9+1 - unknown_count):
                for live_count in range(0, 9+1 - unknown_count):
                    if stable_center == ON and stab_count == 0: continue
                    if current_center == ON and live_count == 0: continue

                    stab_neighbours = stab_count
                    live_neighbours = live_count
                    unknown_neighbours = unknown_count

                    if current_center == ON:
                        live_neighbours -= 1

                    if stable_center == ON:
                        stab_neighbours -= 1

                    if stable_center == UNKNOWN or current_center == UNKNOWN:
                        unknown_neighbours -= 1

                    if current_center == stable_center and live_neighbours == stab_neighbours: continue
                    if naive_stepactive_function(current_center, live_neighbours, unknown_neighbours) != UNKNOWN: continue

                    n = CellUnknownNeighbourhood(stable_center, stab_neighbours, unknown_neighbours)

                    for o in StableOptions.compatible_options(n):
                        result = unknown_step_function(o, current_center, stab_neighbours, live_neighbours, unknown_neighbours)
                        data += emit_boolean(o, current_center, stable_center, stab_count, live_count, unknown_count, result)

run_espresso(data, innames, outnames)

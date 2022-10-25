import subprocess, StringIO

OFF = 0
ON = 1
UNKNOWN = 2
UNKNOWN_STABLE = 3
ABORT = 4

def life_rule(state, live_count):
    if live_count == 3:
        return ON
    if state == ON and live_count == 2:
        return ON
    return OFF

def life_rule_3state(state, live_count, unknown_count):
    off_ok = False
    on_ok = False
    for i in range(live_count, live_count + unknown_count + 1):
        if state != OFF:
            value = life_rule(ON, i)
            if value == OFF: off_ok = True
            if value == ON: on_ok = True
        if state != ON:
            value = life_rule(OFF, i)
            if value == OFF: off_ok = True
            if value == ON: on_ok = True
    if off_ok and on_ok:
        return UNKNOWN
    elif on_ok:
        return ON
    else:
        return OFF

def int2bin(n, count=24):
    """returns the binary of integer n, using count number of digits"""
    return "".join([str((n >> y) & 1) for y in range(count-1, -1, -1)])

def emit_boolean(data, state, live_count, unknown_count, result, rdigs=2):

    inputs = int2bin(state, 2) + int2bin(live_count, 4) + int2bin(unknown_count, 4)
    outputs = int2bin(result, rdigs)

    print >> data, inputs, outputs

def emit_rule(data, live_count, unknown_count):
    emit_boolean(data, OFF, live_count, unknown_count,
                 life_rule_3state(OFF, live_count, unknown_count))

    emit_boolean(data, ON, live_count + 1, unknown_count,
                 life_rule_3state(ON, live_count, unknown_count))

    emit_boolean(data, UNKNOWN, live_count, unknown_count + 1,
                 life_rule_3state(UNKNOWN, live_count, unknown_count))

def make_3state_rule():
    data = StringIO.StringIO()

    print >> data, ".i 10"
    print >> data, ".o 2"
    print >> data, ".type fr"

    for live_count in range(0, 9):
        for unknown_count in range(0, 9 - live_count):
            emit_rule(data, live_count, unknown_count)

    innames = ["mid_unk", "mid",
               "neigh_total3", "neigh_total2", "neigh_total1", "neigh_total0",
               "neigh_unk_total3", "neigh_unk_total2", "neigh_unk_total1", "neigh_unk_total0" ]
    outnames = ["is_unk", "is_live"]

    run_espresso(data.getvalue(), innames, outnames)

def emit_stabilise_rule(data, live_count, unknown_count):
    # Which values would be stable here?

    off_stable = life_rule_3state(OFF, live_count, unknown_count) != ON
    on_stable = life_rule_3state(ON, live_count, unknown_count) != OFF

    if off_stable and on_stable:
        emit_boolean(data,     OFF, live_count, unknown_count, OFF, 3)
        emit_boolean(data,      ON, live_count + 1, unknown_count, ON, 3)
        emit_boolean(data, UNKNOWN, live_count, unknown_count + 1, UNKNOWN_STABLE, 3)

    elif off_stable:
        emit_boolean(data,     OFF, live_count, unknown_count, OFF, 3)
        emit_boolean(data,      ON, live_count + 1, unknown_count, ABORT, 3)
        emit_boolean(data, UNKNOWN, live_count, unknown_count + 1, OFF, 3)

    elif on_stable:
        emit_boolean(data,     OFF, live_count, unknown_count, ABORT, 3)
        emit_boolean(data,      ON, live_count + 1, unknown_count, ON, 3)
        emit_boolean(data, UNKNOWN, live_count, unknown_count + 1, ON, 3)

    else:
        emit_boolean(data,     OFF, live_count, unknown_count, ABORT, 3)
        emit_boolean(data,      ON, live_count + 1, unknown_count, ABORT, 3)
        emit_boolean(data, UNKNOWN, live_count, unknown_count + 1, ABORT, 3)


def make_stabilise_rule():
    data = StringIO.StringIO()

    print >> data, ".i 10"
    print >> data, ".o 3"
    print >> data, ".type fr"

    for live_count in range(0, 9):
        for unknown_count in range(0, 9 - live_count):
            emit_stabilise_rule(data, live_count, unknown_count)

    innames = ["mid_unk", "mid",
               "neigh_total3", "neigh_total2", "neigh_total1", "neigh_total0",
               "neigh_unk_total3", "neigh_unk_total2", "neigh_unk_total1", "neigh_unk_total0" ]
    outnames = ["abort", "is_unk", "is_live"]

    run_espresso(data.getvalue(), innames, outnames)

def run_espresso(data, innames, outnames):
    p = subprocess.Popen(["/home/mike/tmp/espresso-ab-1.0/src/espresso", "-Dexact"],
                         stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE)

    print data
    out, err = p.communicate(data)
    print out
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
                print name, "|=", code, ";"

#make_3state_rule()
make_stabilise_rule()

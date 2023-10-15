import dataclasses
from dataclasses import dataclass
import itertools
import subprocess

OFF = 0
ON = 1
UNKNOWN = 2
ABORT = 7
DONTCARE = 8

def espresso_char(v):
    if v == True:
        return "1"
    if v == False:
        return "0"
    if v == DONTCARE:
        return "-"
    return "?"

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

def int2bin(n, count=24):
    """returns the binary of integer n, using count number of digits"""
    return "".join([str((n >> y) & 1) for y in range(count-1, -1, -1)])

@dataclass
class CellNeighbourhood:
    center : int
    neighbours : int

    def life_rule(self):
        if self.center == ON:
            if self.neighbours == 2 or self.neighbours == 3: return ON
            else: return OFF
        if self.center == OFF:
            if self.neighbours == 3: return ON
            else: return OFF

    def life_stable(self):
        if self.center == ON:
            return self.neighbours == 2 or self.neighbours == 3
        if self.center == OFF:
            return not self.neighbours == 3

@dataclass
class StableOptions:
    # Recall that True means these are ruled out
    live2 : bool
    live3 : bool
    dead0 : bool
    dead1 : bool
    dead2 : bool
    dead4 : bool
    dead5 : bool
    dead6 : bool

    def copy(self):
        return dataclasses.replace(self)


    def espresso_str(self):
        result = ""
        result += espresso_char(self.live2)
        result += espresso_char(self.live3)
        result += espresso_char(self.dead0)
        result += espresso_char(self.dead1)
        result += espresso_char(self.dead2)
        result += espresso_char(self.dead4)
        result += espresso_char(self.dead5)
        result += espresso_char(self.dead6)
        return result

    def join(self, other):
        self.live2 = self.live2 and other.live2
        self.live3 = self.live3 and other.live3
        self.dead0 = self.dead0 and other.dead0
        self.dead1 = self.dead1 and other.dead1
        self.dead2 = self.dead2 and other.dead2
        self.dead4 = self.dead4 and other.dead4
        self.dead5 = self.dead5 and other.dead5
        self.dead6 = self.dead6 and other.dead6

    # I hate python
    def upperset(self):
        attrs = ["live2", "live3", "dead0", "dead1", "dead2", "dead4", "dead5", "dead6"]
        falses = [a for a in attrs if not getattr(self, a)]
        result = []
        for r in range(1, len(falses)+1):
            for combo in itertools.combinations(falses, r):
                options = StableOptions(True, True, True, True, True, True, True, True)
                for c in combo:
                    setattr(options, c, False)
                result.append(options)
        return result

    def possible_neighbourhoods(self):
        result = []
        if not self.live2: result.append(CellNeighbourhood(ON, 2))
        if not self.live3: result.append(CellNeighbourhood(ON, 3))
        if not self.dead0: result.append(CellNeighbourhood(OFF, 0))
        if not self.dead1: result.append(CellNeighbourhood(OFF, 1))
        if not self.dead2: result.append(CellNeighbourhood(OFF, 2))
        if not self.dead4: result.append(CellNeighbourhood(OFF, 4))
        if not self.dead5: result.append(CellNeighbourhood(OFF, 5))
        if not self.dead6: result.append(CellNeighbourhood(OFF, 6))
        return result

    def is_maximal(self):
        return [self.live2, self.live3, self.dead0, self.dead1, self.dead2, self.dead4, self.dead5, self.dead6].count(False) == 1

    def maybedead(self):
        return not self.dead0 or not self.dead1 or not self.dead2 or not self.dead4 or not self.dead5 or not self.dead6

    def maybelive(self):
        return not self.live2 or not self.live3

    def to_three_state(self):
        maybedead = self.maybedead()
        maybelive = self.maybelive()
        if maybelive and not maybedead:
            return ON
        if not maybelive and maybedead:
            return OFF
        return UNKNOWN

    @staticmethod
    def compatible_options(center, stable_neighbours, unknown_neighbours):
        # Counts not including the center square

        lower = stable_neighbours
        upper = stable_neighbours + unknown_neighbours
        r = range(lower, upper + 1)

        minimal = StableOptions(not 2 in r, not 3 in r, not 0 in r, not 1 in r, not 2 in r, not 4 in r, not 5 in r, not 6 in r)
        if center == ON:
            minimal.dead0 = True
            minimal.dead1 = True
            minimal.dead2 = True
            minimal.dead4 = True
            minimal.dead5 = True
            minimal.dead6 = True
        if center == OFF:
            minimal.live2 = True
            minimal.live3 = True
        upperset = minimal.upperset()

        if center == UNKNOWN:
            upperset = list(filter(lambda u: u.to_three_state() == UNKNOWN, upperset))

        return upperset

def print_output(lines, innames, outnames):
    for term in lines:
        ins, outs = term.split(" ")
        code = []
        for val, name in zip(ins, innames):
            if val == "0":
                code.append(f"(~{name})")
            elif val == "1":
                code.append(name)
        code = " & ".join(code)

        outcount = outs.count("1")
        if outcount == 1:
            for val, name in zip(outs, outnames):
                if val == "1":
                    print(f"{name} |= {code};")
        if outcount > 1:
            onoutnames = [name for val, name in zip(outs, outnames) if val == "1"]
            print("{ " + f"uint64_t temp = {code}; " + " ".join([f"{name} |= temp;" for name in onoutnames]) + " }")

def print_phase_correction(phase_line, outnames):
    bits = phase_line[8:]
    for val, name in zip(bits, outnames):
        if val == "0":
            print(f"{name} = ~{name};")

def run_espresso(data, innames, outnames):
    header = f""".i {len(innames)}
.o {len(outnames)}
.pli {" ".join(innames)}
.ob {" ".join(outnames)}
.type fr
"""
    print(header)
    print(data)
    # p = subprocess.run(["./espresso", "-Dexact", "-S1"],
    p = subprocess.run(["./espresso", "-Dopoall"],
    # p = subprocess.run(["./espresso", "-Dso_both", "-S1"],
                       text = True,
                       input = header + data,
                       capture_output = True)
    out = p.stdout
    print(out)
    lines = out.split("\n")
    phase_line = [l for l in lines if l.startswith("#.phase")][0]
    lines = [l for l in lines if len(l) > 0 and l[0] != '.' and l[0] != '#']


    print_output(lines, innames, outnames)
    print_phase_correction(phase_line, outnames)

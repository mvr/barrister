import dataclasses
from dataclasses import dataclass
import itertools
import subprocess

# TODO: make an enum
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

def int2bin(n, count):
    return "".join([str((n >> y) & 1) for y in range(count-1, -1, -1)])

# known_off, known_on
def known2bin(n):
    if n == OFF: return "10"
    if n == ON:  return "01"
    if n == UNKNOWN:  return "00"
    return "--"

def int2hot(n, count):
    result = ["0"] * count
    result[n] = "1"
    return "".join(result)

def int2twos(n, count):
    if n >= 0:
        return int2bin(n, count)
    else:
        return int2bin((1 << count) + n, count)

# Counts not including the center square
@dataclass
class CellNeighbourhood:
    center : int
    count : int

    def life_rule(self):
        if self.center == ON:
            if self.count == 2 or self.count == 3: return ON
            else: return OFF
        if self.center == OFF:
            if self.count == 3: return ON
            else: return OFF

    def life_stable(self):
        if self.center == ON:
            return self.count == 2 or self.count == 3
        if self.center == OFF:
            return not self.count == 3

#     def unknown_relaxations(self):
#         return CellUnknownNeighbourhood(0,0,0)

@dataclass
class CellUnknownNeighbourhood:
    center : int
    count : int
    unknown : int

    def meet(self, other):
        if self.center == other.center: new_center = self.center
        elif self.center == UNKNOWN: new_center = other.center
        elif other.center == UNKNOWN: new_center = self.center
        else: return None

        known_ons = max(self.count, other.count)
        known_offs = max(8 - self.unknown - self.count, 8 - other.unknown - other.count)
        remaining_unknown = 8 - known_ons - known_offs

        return CellUnknownNeighbourhood(new_center, known_ons, remaining_unknown)

    def restrict_to(self, o):
        return self.meet(o.to_unknown_neighbourhood())

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

    # There must be a nicer way to do this so I can just write StableOptions.impossible
    @staticmethod
    def impossible():
        return StableOptions(True, True, True, True, True, True, True, True)

    @staticmethod
    def unknown():
        return StableOptions(False, False, False, False, False, False, False, False)

    @staticmethod
    def on():
        return StableOptions(False, False, True, True, True, True, True, True)

    @staticmethod
    def off():
        return StableOptions(True, True, False, False, False, False, False, False)

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

    # are these names backwards?
    def join(self, other):
        return StableOptions(
            self.live2 and other.live2,
            self.live3 and other.live3,
            self.dead0 and other.dead0,
            self.dead1 and other.dead1,
            self.dead2 and other.dead2,
            self.dead4 and other.dead4,
            self.dead5 and other.dead5,
            self.dead6 and other.dead6)

    def meet(self, other):
        return StableOptions(
            self.live2 or other.live2,
            self.live3 or other.live3,
            self.dead0 or other.dead0,
            self.dead1 or other.dead1,
            self.dead2 or other.dead2,
            self.dead4 or other.dead4,
            self.dead5 or other.dead5,
            self.dead6 or other.dead6)

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

    @staticmethod
    def all_possible():
        return StableOptions.unknown().upperset();

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

    def maybe_dead(self):
        return not self.dead0 or not self.dead1 or not self.dead2 or not self.dead4 or not self.dead5 or not self.dead6

    def maybe_live(self):
        return not self.live2 or not self.live3

    def to_three_state(self):
        maybedead = self.maybe_dead()
        maybelive = self.maybe_live()
        if maybelive and not maybedead:
            return ON
        if not maybelive and maybedead:
            return OFF
        return UNKNOWN

    def single_on(self):
        count = 0
        if not self.live2: count += 1
        if not self.live3: count += 1
        return count == 1

    def single_off(self):
        count = 0
        if not self.dead0: count += 1
        if not self.dead1: count += 1
        if not self.dead2: count += 1
        if not self.dead4: count += 1
        if not self.dead5: count += 1
        if not self.dead6: count += 1
        return count == 1

    def possibilities_count(self):
        count = 0
        if not self.live2: count += 1
        if not self.live3: count += 1
        if not self.dead0: count += 1
        if not self.dead1: count += 1
        if not self.dead2: count += 1
        if not self.dead4: count += 1
        if not self.dead5: count += 1
        if not self.dead6: count += 1
        return count


    def to_unknown_neighbourhood(self):
        counts = [n.count for n in self.possible_neighbourhoods()]
        return CellUnknownNeighbourhood(self.to_three_state(), min(counts), max(counts) - min(counts))

    @staticmethod
    def maximal_options(unknown_neighbourhood):
        lower = unknown_neighbourhood.count
        upper = unknown_neighbourhood.count + unknown_neighbourhood.unknown
        r = range(lower, upper + 1)

        maximal = StableOptions(not 2 in r, not 3 in r, not 0 in r, not 1 in r, not 2 in r, not 4 in r, not 5 in r, not 6 in r)
        if unknown_neighbourhood.center == ON:
            maximal.dead0 = True
            maximal.dead1 = True
            maximal.dead2 = True
            maximal.dead4 = True
            maximal.dead5 = True
            maximal.dead6 = True
        if unknown_neighbourhood.center == OFF:
            maximal.live2 = True
            maximal.live3 = True
        return maximal

    @staticmethod
    def compatible_options(unknown_neighbourhood):
        upperset = StableOptions.maximal_options(unknown_neighbourhood).upperset()

        if unknown_neighbourhood.center == UNKNOWN:
            upperset = list(filter(lambda u: u.to_three_state() == UNKNOWN, upperset))

        return upperset

    @staticmethod
    def compatible_options_state(state):
        if state == ON: return StableOptions.on().upperset()
        if state == OFF: return StableOptions.off().upperset()
        return StableOptions.unknown().upperset()

    def restrict_to(self, unknown_neighbourhood):
        return self.meet(StableOptions.maximal_options(unknown_neighbourhood))

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
    p = subprocess.run(["./espresso", "-Dopoall", "-S1"],
    # p = subprocess.run(["./espresso", "-Dopoall"],
    # p = subprocess.run(["./espresso", "-Dso_both", "-S1"],
    # p = subprocess.run(["./espresso", "-Dso_both"],
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

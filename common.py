import subprocess


def int2bin(n, count=24):
    """returns the binary of integer n, using count number of digits"""
    return "".join([str((n >> y) & 1) for y in range(count-1, -1, -1)])

def run_espresso(data, innames, outnames):
    print(data)
    p = subprocess.run(["./espresso", "-Dexact", "-Dso_both", "-S1"],
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

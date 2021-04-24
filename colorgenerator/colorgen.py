import json
import argparse
import os
import platform
import sys

import reportgen
import blockIDs
import textureGen

# walks through the states tree and replaces all 'oldID' values with 'newID'
def mapIdIfExists(states, oldID, newID):
    for state, val in states.items():
        if isinstance(val, int):
            if val == oldID:
                states[state] = newID
        else:
            mapIdIfExists(val, oldID, newID)

def printPercent(val, maxVal):
    sys.stdout.write('\r')
    per = (val/maxVal)*100
    per = round(per, 2)
    sys.stdout.write("({}%)  ".format(per))
    sys.stdout.flush()

# checks if all leaves from a state subtree have the same stateID
def canCondenseStates(states):
    stateID = None
    for val in states.values():
        if isinstance(val, int):
            if stateID is None:
                stateID = val
            elif stateID != val:
                return (False, None)
        else:
            if not canCondenseStates(val)[0]:
                return (False, None)
    return (True, stateID)

# if all leaves from a states subtree have the same texture, condense the tree
def condenseStates(states):
    if isinstance(list(states.values())[0], int):
        return canCondenseStates(states)  # return used blockID
    for state, val in states.items():
        ret = condenseStates(val)
        if ret[0]:
            states[state] = ret[1]
    return (False, None)

# returns a list of all state ids in the given 'state' tree
def getStateIDs(states):
    ids = []
    for _, val in states.items():
        if isinstance(val, int):
            ids.append(val)
        else:
            ids.extend(getStateIDs(val))
    return ids


def main():
    parser = argparse.ArgumentParser(description='Generate colors.json')
    parser.add_argument('-g', '--gamedir', help='Path to Minecraft install folder, if not installed in default folder', required=False)
    parser.add_argument('-v', '--version', help='Minecraft version', required=True)
    parser.add_argument('-cg', '--connectedGrass', help='Makes the grass block have grass on the sides', required=False, default=False, action="store_true")
    args = parser.parse_args()

    # get gamedir if not set by user
    if args.gamedir is None:
        if platform.system() == "Windows":
            args.gamedir = os.path.join(os.getenv("APPDATA"), ".minecraft")
        else:
            args.gamedir = os.path.join(os.getenv("HOME"), ".minecraft")
    args.version = args.version.strip()

    print("Generate block report...")
    report = reportgen.genReport(args.gamedir, args.version)

    print("Generate Blockstates...")
    blocks = blockIDs.getBlockstates(report["report"])

    print("Generate colors...")
    models = textureGen.genTextureColors(os.path.join(args.gamedir, "versions", args.version, args.version + ".jar"), report["report"])

    if args.connectedGrass:
        grassBlockId = blocks["minecraft:grass_block"]["states"]["false"] #select non snowy version
        for i in range(len(models)):
            if models[i]["id"] == grassBlockId:
                models[i]["colors"][1] = models[i]["colors"][0]
                break

    # remove duplicate colors
    print("cleanup colors...")
    i = 0
    while i < len(models)-1:
        printPercent(i+1, len(models))
        firstColor = models[i]
        j = i+1
        while j < len(models):
            secondColor = models[j]
            if firstColor["colors"] == secondColor["colors"] and firstColor["drawMode"] == secondColor["drawMode"]:
                # found dublicate, remove secondColor
                for block in blocks.values():
                    mapIdIfExists(block["states"], secondColor["id"], firstColor["id"])
                del models[j]
                j -= 1
            j += 1
        i += 1
    printPercent(1, 1)
    print("")

    # removes gaps between color ids
    for i in range(0, len(models)):
        model = models[i]
        for c in model["colors"]:
            if "empty" in c:
                del c["empty"]
        if model["id"] == i+1:
            continue
        oldID = model["id"]
        model["id"] = i+1
        for block in blocks.values():
            mapIdIfExists(block["states"], oldID, i+1)

    for name, block in blocks.items():
        if "order" not in block:
            continue

        while canCondenseStates(block["states"])[0]:
            ret = condenseStates(block["states"])
            block["order"].pop()
            if ret[0]:
                assert len(block["order"]) == 0
                block["states"] = {'': ret[1]}
                break

        if len(block["order"]) == 0:
            del block["order"]

    # resolve special Blocks
    for name, values in report["tags"].items():
        ids = []
        for block in values:
            if block in blocks:
                states = blocks[block]["states"]
                ids.extend(getStateIDs(states))
        ids = sorted(list(dict.fromkeys(ids)))
        report["tags"][name] = ids

    print("write colors...")
    # dump for debug
    with open("colors.json", "w") as f:
        out = {
            "blocks": blocks,
            "models": models,
            "tags": report["tags"]
        }
        f.write(json.dumps(out))


if __name__ == "__main__":
    main()

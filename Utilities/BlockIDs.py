import json
import sys
import subprocess
import specialFuncs
import argparse

specialBlocks = {"grass_block": ["minecraft:grass_block"], "water": ["minecraft:water"], "lava": ["minecraft:lava"], "leaves": ["minecraft:oak_leaves", "minecraft:spruce_leaves", "minecraft:birch_leaves", "minecraft:jungle_leaves", "minecraft:acacia_leaves", "minecraft:dark_oak_leaves"], "torch": ["minecraft:torch", "minecraft:wall_torch"], "snow": ["minecraft:snow", "minecraft:snow_block"]}

#Parse arguments
parser = argparse.ArgumentParser(description='Update BlockID.json file')
parser.add_argument('-j', '--jar', help='Path to Minecraft jar file')
parser.add_argument('-v', '--version', help='Minecraft version')
parser.add_argument('-l', '--lib', help='Path to the Minecraft library folder, if not installed in default folder (Windows)')
args = parser.parse_args()

if args.jar and args.version:
    print("--jar and --version are mutually exclusive")
    sys.exit(1)

jar = args.jar #path to minecraft jar file

if jar is None:
    if args.version is None:
        print("Please specify either -v or -j")
        sys.exit(1)
    jar = specialFuncs.getPathFromVersion(args.version)

#generate report for all blocks in minecraft
if not specialFuncs.genReport(jar, args.lib):
    print("Error creating report")
    sys.exit(1)

blockFile = "generated/reports/blocks.json"
allBlocks = json.loads(open(blockFile).read())
outData = {"allBlocks": {}, "specialBlocks": {}}

print("Generating block id table...")
#Get all possible blockstates
for blockName, blockVals in allBlocks.items():
    ret = {}
    #check if block has multiple states
    if "properties" in blockVals:
        properties = blockVals["properties"]
        if len(properties) > 1: #check if block has more than one state description (e.g snowy, rotation)
            length = {}
            for prop, values in properties.items():
                length[prop] = len(values)
                
            #find state desc with lowest amount of values to build order
            lowest = 100
            lowestName = ""
            order = []
            while len(length) > 0:
                for k, v in length.items():
                    if v < lowest:
                        lowest = v
                        lowestName = k
                order.append(lowestName)
                del length[str(lowestName)]
                lowest = 100
                lowestName = ""

            ret["order"] = order
        else:
            #only one state description, take it
            ret["order"] = [list(properties.keys())[0]]

        #build state tree
        blockStates = blockVals["states"]
        outStates = {}
        for state in blockStates:
            stateProp = state["properties"]
            current = outStates
            for i in range(0, len(ret["order"])):
                o = ret["order"][i]
                if i+2 > len(ret["order"]):
                    assert state["id"] < 2**16
                    current[stateProp[o]] = state["id"]
                else:
                    if not stateProp[o] in current:
                        current[stateProp[o]] = {}
                current = current[stateProp[o]]

        ret["states"] = outStates
    else:
        #just take only existing state
        bID = blockVals["states"][0]["id"]

        if blockName == "minecraft:void_air" or blockName == "minecraft:cave_air":
            bID = 0

        assert bID < 2**16
                    
        outStates = {"": bID}
        ret["states"] = outStates

    outData["allBlocks"][blockName] = ret

#get block Id's for the special blocks
specialBlocksContainer = {}
for blockName, blockVals in allBlocks.items():
    found = False
    match = ""
    for blockDesc, assoBlocks in specialBlocks.items():
        if any(b == blockName for b in assoBlocks):
            found = True
            match = blockDesc
    if not found:
        continue

    minID = sys.maxsize
    maxID = 0
    states = blockVals["states"]
    for state in states:
        sID = state["id"]
        minID = min(minID, sID)
        maxID = max(maxID, sID)

    if match in specialBlocksContainer:
        specialBlocksContainer[match].append({"min": minID, "max": maxID})
    else:
        specialBlocksContainer[match] = [{"min": minID, "max": maxID}]

#simplify specialBlocksContainer
for block, values in specialBlocksContainer.items():
    outData["specialBlocks"][block] = []
    minID = values[0]["min"]
    maxID = values[0]["max"]
    if len(values) > 1:
        for i in range(1, len(values)):
            if maxID+1 == values[i]["min"]:
                maxID = values[i]["max"]
            else:
                outData["specialBlocks"][block].append({"min": minID, "max": maxID})
                minID = values[i]["min"]
                maxID = values[i]["max"]
                
    outData["specialBlocks"][block].append({"min": minID, "max": maxID})
        
#write tree to file
with open('../BlockIDs.json', 'w') as outfile:
    json.dump(outData, outfile)
    print("written all blockstates successfully")

specialFuncs.cleanReport()



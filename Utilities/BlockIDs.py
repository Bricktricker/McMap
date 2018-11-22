import json
import sys
import subprocess
import specialFuncs
import argparse

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
outData = {}

print("Generating block id table...")
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
        
        outStates = {"": bID}
        ret["states"] = outStates

    outData[blockName] = ret

#write tree to file
with open('../BlockIDs.json', 'w') as outfile:
    json.dump(outData, outfile)
    print("written all blockstates successfully")

specialFuncs.cleanReport()



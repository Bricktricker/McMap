import json
import sys

if len(sys.argv) > 1:
    p = sys.argv[1]
else:
    p = "blocks.json"

allBlocks = json.loads(open(p).read())
outData = {}

for blockName, blockVals in allBlocks.items():
    ret = {}
    if "properties" in blockVals:
        properties = blockVals["properties"]
        if len(properties) > 1:
            length = {}
            for prop, values in properties.items():
                length[prop] = len(values)
            #get lowest length
            lowest = 100
            lowestName = ""
            order = []
            #print(length)
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
            ret["order"] = [list(properties.keys())[0]]

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
        bID = blockVals["states"][0]["id"]

        if blockName == "minecraft:void_air" or blockName == "minecraft:cave_air":
            bID = 0
        
        outStates = {"": bID}
        ret["states"] = outStates

    outData[blockName] = ret


with open('../BlockIDs.json', 'w') as outfile:
    json.dump(outData, outfile)

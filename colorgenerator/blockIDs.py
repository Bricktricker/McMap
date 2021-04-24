
# initial called function to generate the blockstate lookup table
# iterates over all blocks and generates the BlockState -> ID lookup table
# report arg: the generated block report from the --reports data run 
# returns a dict of all blocks
def getBlockstates(report):
    outData = {}
    for blockName, blockVals in report.items():
        outData[blockName] = getBlockstate(blockName, blockVals)
    
    return outData

# generates the lookup table for all blockStates for the specified block
# blockName arg: the full name of the block, e.g. 'minecraft:stone'
# blockVals arg: The data from the datarun for the specified block. Its a dict with a 'states' array and maybe a 'properties' dict, see https://wiki.vg/Data_Generators#Blocks_report
# returns a dict with a 'states' key, which either has only a '""' key with an integer value, if the block only has one model,
# or it also contains an 'order' key, specifying the order of blockState properties that are used to walk the 'states' tree
def getBlockstate(blockName, blockVals):
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

    return ret
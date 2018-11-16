import json
import re
import math
import specialFuncs
import sys
import argparse
import zipfile

parser = argparse.ArgumentParser(description='Update colors.json file')
parser.add_argument('-j', '--jar', help='Path to Minecraft jar file')
parser.add_argument('-v', '--version', help='Minecraft version')
parser.add_argument('-l', '--lib', help='Path to the Minecraft library folder, if not installed in default folder (Windows)')
parser.add_argument('-s', '--special', help='File containing colors for special blocks. Default: specialBlocks.csv')
args = parser.parse_args()

if args.jar and args.version:
    print("--jar and --version are mutually exclusive")
    sys.exit(1)

jar = args.jar

if jar is None:
    if args.version is None:
        print("Please specify either -v or -j")
        sys.exit(1)
    jar = specialFuncs.getPathFromVersion(args.version)

if not specialFuncs.genReport(jar, args.lib):
    print("Error creating report")
    sys.exit(1)

blockFile = "generated/reports/blocks.json"
csvPath = "specialBlocks.csv"
if not args.special is None:
    csvPath = args.special

allBlocks = json.loads(open(blockFile).read())

outData = []

def loadFileJSON(file):
    zf = zipfile.ZipFile(jar, 'r')
    content = None
    try:
        iFile = zf.open("assets/minecraft/{}".format(file))
        content = json.loads(iFile.read())
        iFile.close()
    finally:
        zf.close()
    return content

def printPercent(val, maxVal):
    sys.stdout.write('\r')
    per = (val/maxVal)*100
    per = round(per, 2)
    sys.stdout.write("({}%)  ".format(per))
    sys.stdout.flush()
    

def isTinted(data, selection):
    if "elements" not in data:
        if "parent" in data:
            return isTinted(loadFileJSON("models/{}.json".format(data["parent"])), selection)
        return False
    
    elements = data["elements"]
    for elem in elements:
        faces = elem["faces"]
        for faceName, face in faces.items():
            if "texture" in face:
                if face["texture"] == ("#" + selection):
                    if "tintindex" in face:
                        return True

    if "parent" in data:
        return isTinted(loadFileJSON("models/{}.json".format(data["parent"])), selection)
    else:
        return False

def parseStr(inStr):
    d = re.findall(r'[a-zA-Z_-]+=[A-Za-z0-9_-]+', inStr)
    out = {}
    for entry in d:
        s = entry.split('=')
        out[s[0]] = s[1]
    return out

def handleSpecialBlocks(model):
    if model == "block/grass_block":
        return ("top", 7)
    if model.endswith("_slab"): #slab bottom
        return ("top", 9)
    if model.endswith("_slab_top"): #slab top
        return ("top", 10)
    if model.endswith("_slab_double"): #slab double/full block
        return ("end", 0)
    if "fence" in model:
        return ("texture", 4)
    if model.endswith("_carpet"):
        return ("wool", 1)
    if "height" in model:
        return ("texture", 1)
    if "trapdoor" in model:
        return ("texture", 1)
    if model.startswith("block/fire") and "coral" not in model:
        return ("fire", 8)
    if model == "block/seagrass":
        return ("seagrass", 3)
    if "vine" in model:
        return ("vine", 4)
        
    return None

def getTextureFromModel(model):
    modelData = loadFileJSON("models/{}.json".format(model))
    textures = modelData["textures"]
    texture = ""
    blockType = 0 #0 = SOLID, 1 = FLAT (Snow/Trapdor/Carpet), 2 = TORCH, 3 = FLOWER/PLANT, 4 = FENCE, 5 = WIRE, 6 = RAIL, 7 = GRASS, 8 = FIRE, 9 = SLAP bottom, 10 = SLAP top
    special = handleSpecialBlocks(model)

    selection = ""
    if special is not None:
        texture = textures[special[0]]
        blockType = special[1]
        selection = special[0]
    else:
        if 'all' in textures:
            texture = textures['all']
            selection = 'all'
        elif 'top' in textures:
            texture = textures['top']
            selection = 'top'
        elif 'side' in textures:
            texture = textures['side']
            selection = 'side'
        elif 'cross' in textures:
            texture = textures['cross']
            blockType = 3
            selection = 'cross'
        elif 'rail' in textures:
            texture = textures['rail']
            blockType = 6
            selection = 'rail'
        elif 'texture' in textures:
            texture = textures['texture']
            selection = 'texture'
        elif 'crop' in textures:
            texture = textures['crop']
            blockType = 3
            selection = 'crop'
        elif 'plant' in textures:
            texture = textures['plant']
            blockType = 3
            selection = 'plant'
        elif 'line' in textures:
            texture = textures['line']
            blockType = 5
            selection = 'line'
        elif 'torch' in textures:
            texture = textures['torch']
            blockType = 2
            selection = 'torch'
        elif 'particle' in textures:
            texture = textures['particle']
            selection = 'particle'
        else:
            selection = list(textures.keys())[0]
            texture = textures[selection] #simply take first texture
            #print("Take first texture for {}".format(model))

    tint = False
    tint = isTinted(modelData, selection)
    
    return (texture, blockType, tint)

def getTextureFromState(textures, propOfState): #returns texture tuple for given state
    if len(textures) == 1:
        return textures[list(textures.keys())[0]] #only one texture

    for textureConds, texture in textures.items():
        found = True
        texCondDic = parseStr(textureConds)
        for texCondKey, texCondVal in texCondDic.items():
            if texCondKey in propOfState:
                if propOfState[texCondKey] != texCondVal: #wrong texture
                    found = False
                    
        if found:
            return texture

    print("Texture not found")
    return None
                
 
def getTextures(block):
    blockStatesData = None
    try:
        blockStatesData = loadFileJSON("blockstates/{}.json".format(block))
    except Exception as e:
        print(e)
        print("Could not load:{}".format(block))
        return {}

    if 'variants' not in blockStatesData:
        #print("no variants in {}".format(block))
        if 'multipart' not in blockStatesData:
            return {}
        else:
            #print("multipart in: {}".format(block))
            apply = list(blockStatesData["multipart"])[0]["apply"] #take first mdoel in multipart
            model = ""
            if type(apply) is list:
                model = apply[0]["model"]
            else:
                model = apply["model"]
            return {"": getTextureFromModel(model)}
                

    #loading variants, no multiblock
    variants = blockStatesData["variants"]

    buffer = {}
    
    for varName, varModel in variants.items():
        model = ""
        if type(varModel) is list:
            model = varModel[0]['model'] #list of diffrent models, simply take first. They should have all the same texture
        else:
            model = varModel['model'] #only one model, take it
        modelData = getTextureFromModel(model)
        buffer[varName] = modelData

    return buffer


print("Generating colors for textures. This may take a while...")
i = -1
for blockNameL, blockData in allBlocks.items():
    i += 1
    if blockNameL == "minecraft:air" or blockNameL == "minecraft:void_air" or blockNameL == "minecraft:cave_air":
        continue

    #remove "minecraft:" from name
    blockName = blockNameL
    if blockNameL.startswith("minecraft:"):
        blockName = blockNameL[10:]

    textures = getTextures(blockName)
    
    if len(textures) == 0:
        print("No texures for {}".format(blockName))
        continue

    if 'properties' not in blockData:
        if len(textures) > 1:
            print("{} has multiple textures but only one state".format(blockName))
            break
        state = list(blockData["states"])[0]
        texture = textures[list(textures.keys())[0]]
        color = specialFuncs.calc(texture[0], jar, texture[2], csvPath) #calc rgba values for texture
        d = {"texture": texture[0], "from": state["id"], "to": state["id"], "blockType": texture[1], "color": color}
        outData.append(d)
        printPercent(i, len(allBlocks))

    else: #multiple states
        for state in blockData["states"]:
            propOfState = state["properties"]
            texture = getTextureFromState(textures, propOfState)
            color = specialFuncs.calc(texture[0], jar, texture[2], csvPath) #calc rgba values for texture
            d = {"texture": texture[0], "from": state["id"], "to": state["id"], "blockType": texture[1], "color": color}
            outData.append(d)
            printPercent(i, len(allBlocks))

#remove duplicates
print("\n")
pos = 0
while pos < len(outData)-1:
    if outData[pos]["texture"] == outData[pos+1]["texture"] and outData[pos]["blockType"] == outData[pos+1]["blockType"] and outData[pos]["color"] == outData[pos+1]["color"]:
        if outData[pos]["to"]+1 == outData[pos+1]["from"]:
            outData[pos]["to"] = outData[pos+1]["to"]
            del outData[pos+1]
            pos = max(0, pos-2)
        else:
            print("{}\n{}".format(outData[pos], outData[pos+1]))
    pos += 1
            
with open('../colors.json', 'w') as outfile:
    json.dump(outData, outfile)
    print("written {} colors successfully".format(len(outData)))

specialFuncs.cleanReport()


import zipfile
import json
import re
import sys
from PIL import Image, ImageChops

from cube import Cube, Model

def printPercent(val, maxVal):
    sys.stdout.write('\r')
    per = (val/maxVal)*100
    per = round(per, 2)
    sys.stdout.write("({}%)  ".format(per))
    sys.stdout.flush()

# initial called function to generate the color list
# iterates over all blocks and generates a color for each model
# jar arg: the path to the used [version].jar, which contains the blockstate json files, model json files and the textures
# report arg: the generated block report from the --reports data run
# returns a list of dict containing a 'colors' list of one or two colors, an 'id', a 'drawMode' and a 'solidBlock' boolean
def genTextureColors(jar, report):
    colors = []
    i = 0
    for blockName, blockVals in report.items():
        i += 1
        printPercent(i, len(report))
        if blockName == "minecraft:air" or blockName == "minecraft:void_air" or blockName == "minecraft:cave_air":
            continue

        color = genColor(jar, blockName, blockVals)
        for c in color:
            if c["solidBlock"]:
                for x in c["colors"]:
                    c["solidBlock"] &= x["a"] >= 255
        colors.extend(color)
    print("\n")
    return colors

# removes the "minecraft" namespace part from the name
def removeNamespace(name):
    if name.startswith("minecraft:"):
        return name[10:]
    return name

# generates the colors and drawMode for the given block
# jar arg: the path to the used [version].jar, which contains the blockstate json files, model json files and the textures
# blockName arg: the full name of the block, e.g. 'minecraft:stone'
# blockVals arg: The data from the datarun for the specified block. Its a dict with a 'states' array and maybe a 'properties' dict, see https://wiki.vg/Data_Generators#Blocks_report
# returns a list of generates colors for this model
def genColor(jar, blockName, blockVals):
    blockName = removeNamespace(blockName)

    blockstates = loadJarJson(jar, "blockstates/{}.json".format(blockName))

    if 'variants' not in blockstates:
        if 'multipart' not in blockstates:
            print("no variants or multipart in blockstates!")
            return []

        #multipart model
        apply = list(blockstates["multipart"])[0]["apply"] #take first model in multipart
        model = ""
        if type(apply) is list:
            model = apply[0]["model"]
        else:
            model = apply["model"]
        textures = getTexturesForModel(jar, model)
        #calculate r, g, b, a, noise for every texture
        texturesMap = {"": []}
        for tex in textures:
            t = getRGBValue(jar, tex[0], tex[1])
            t["rl"] = tex[0]
            texturesMap[""].append(t)
        drawModeMap = {"": getDrawMode(jar, model, texturesMap[""])}
    else:
        variants = blockstates["variants"]
        texturesMap = {}
        drawModeMap = {}
        for varName, varModel in variants.items():
            model = ""
            if type(varModel) is list:
                model = varModel[0]['model'] #list of different models, simply take first. They should have the same texture
            else:
                model = varModel['model']
            
            textures = getTexturesForModel(jar, model)
            #calculate r, g, b, a, noise for every texture
            texturesMap[varName] = []
            for tex in textures:
                t = getRGBValue(jar, tex[0], tex[1])
                t["rl"] = tex[0]
                texturesMap[varName].append(t)
            drawModeMap[varName] = getDrawMode(jar, model, texturesMap[varName])

    if "properties" not in blockVals:
        #simple block, no states
        assert len(texturesMap) == 1
        state = list(blockVals["states"])[0]
        key = list(texturesMap.keys())[0]
        texture = texturesMap[key]
        drawTuple = drawModeMap[key]
        return [{"colors": texture, "id": state["id"], "drawMode": drawTuple[0], "solidBlock": drawTuple[1]}]
    else:
        ret = []
        for state in blockVals["states"]:
            propOfState = state["properties"]
            texture = getTextureFromState(texturesMap, propOfState)
            drawTuple = getTextureFromState(drawModeMap, propOfState)
            ret.append({"colors": texture, "id": state["id"], "drawMode": drawTuple[0], "solidBlock": drawTuple[1]})
        
        return ret

# findes the right texture from the textureMap, that fits the given blockstate
# texturesMap arg: a dict, mapping a block state (e.g. 'snowy=false') to a texture list
# propOfState arg: a dict, describing a block state, e.g. {'snowy': 'true'}
# returns the texture list for the fitting block state
def getTextureFromState(texturesMap, propOfState):
    if len(texturesMap) == 1:
        return texturesMap[list(texturesMap.keys())[0]] #only one texture

    for textureConds, texture in texturesMap.items():
        found = True
        
        #parse the blockstate string into a map, e.g. "a=b,c=d" => {a: b, c: d}
        d = re.findall(r'[a-zA-Z_-]+=[A-Za-z0-9_-]+', textureConds)
        texCondDic = {}
        for entry in d:
            s = entry.split('=')
            texCondDic[s[0]] = s[1]

        #check if all states match the requested state
        for texCondKey, texCondVal in texCondDic.items():
            if texCondKey in propOfState:
                if propOfState[texCondKey] != texCondVal: #wrong texture
                    found = False
                    
        if found:
            return texture
    return None

# computes the used textures for a given model
# jar arg: the path to the used [version].jar, which contains the blockstate json files, model json files and the textures
# model arg: the model to use e.g. 'model/cobblestone'
# returns a list of tuples (texture, isTinted)
def getTexturesForModel(jar, model):
    modelData = loadJarJson(jar, "models/{}.json".format(removeNamespace(model)))

    if "elements" not in modelData:
        if "parent" in modelData:
            #elements not here, look in parent
            parent = modelData["parent"]
            textures = getTexturesForModel(jar, parent)
        else:
            return [(list(modelData["textures"].values())[0], False)]
    else:
        #model contains sub cubes, load them and get textures
        elements = modelData["elements"]
        topCube = None
        sideCube = None
        for elem in elements:
            cube = Cube(elem)
            up = cube.getUpVal()
            if up[0] > 0:
                if topCube is None:
                    topCube = cube
                elif up[0] > topCube.getUpVal()[0]:
                    topCube = cube
            
            side = cube.getSideVal()
            if side[0] > 0:
                if sideCube is None:
                    sideCube = cube
                elif side[0] > sideCube.getSideVal()[0]:
                    sideCube = cube
        
        textures = []
        if topCube is not None:
            val = topCube.getUpVal()
            textures.append((val[1], val[2]))
        if sideCube is not None:
            val = sideCube.getSideVal()
            textures.append((val[1], val[2]))
        assert len(textures) > 0

    #replace all placeholder with textures
    for i in range(len(textures)):
        texture = textures[i]
        if texture[0].startswith("#"):
            if "textures" in modelData and texture[0][1:] in modelData["textures"]:
                textures[i] = (modelData["textures"][texture[0][1:]], texture[1])

    #remove second texture, if its the same as the first texture
    if len(textures) == 2:
        if textures[0] == textures[1]:
            textures = [textures[0]]

    return textures

# computes the drawMode for a given model
# jar arg: the path to the used [version].jar, which contains the blockstate json files, model json files and the textures
# model arg: the model to use e.g. 'model/cobblestone'
# textures arg: list of used textures by this model
# returns a tuple (drawMode, solidBlock), where 'drawMode' is an integer indicating how this model should be drawn and 'solidBlock' is a boolean indicating if the model is a solid full block
def getDrawMode(jar, model, textures):
    assert len(textures) == 1 or len(textures) == 2
    model = removeNamespace(model)

    if model == "block/template_torch" or model == "block/torch_wall":
        return (2148007936, False) #torch model, TODO: two color drawing missing
    if model == "block/cross" or model == "block/tinted_cross" or model == "block/crop" or "seagrass" in model or model.startswith("block/stem_") or model.startswith("block/sunflower_"):
        return (1101663371264, False) #plant block, TODO: two color drawing missing
    if model.startswith("block/vine") or "fence" in model or "iron_bars" in model:
        return (2415984784, False) # fence, TODO: two color drawing missing
    if "rail" in model:
        return (4792320, False) #rail mdodel, (same as flat)
    if model.startswith("block/fire") and "coral" not in model:
        return (17628802408836, False) #fire model

    modelData = loadJarJson(jar, "models/{}.json".format(model))
    if "elements" not in modelData:
        if "parent" in modelData:
            return getDrawMode(jar, modelData["parent"], textures)
        else:
            # model has no elements and no parent, either multipart model or custom ingame renderer, fallback to full block
            if len(textures) == 1:
                return (162080096478354, True)
            else:
                return (202290807436434, True)
    
    #elements in model, compute draw mode
    elements = modelData["elements"]
    blockModel = Model(list(map(lambda elem: Cube(elem), elements)))

    if blockModel.getHeight(blockModel.getVolume() == 0) >= 0.75:
        #full cube
        if len(textures) == 1:
            return (162080096478354, True)
        else:
            return (202290807436434, True)
    if blockModel.getHeight() >= 0.4:
        #half cube / slab
        if blockModel.getLowestPoint() <= 0.1:
            #bottom slab
            if len(textures) == 1:
                return (19634135040, False)
            else:
                return (29448806400, False)
        else:
            #upper slab
            if len(textures) == 1:
                return (4793490, False)
            else:   
                return (7189650, False)
    if blockModel.getHeight() < 0.4:
        if textures[0]["empty"] > 0.8 and blockModel.getHeight() == 0:
            return (2415919104, False) #wire
        elif blockModel.getTopSurface() >= 0.76:
            return (4792320, False) #Flat block (Snow/Trapdor/Carpet/Pressure plate)

    #fallback to full block
    if len(textures) == 1:
        return (162080096478354, True)
    else:
        return (202290807436434, True)

# caches loaded json files, maps file to loaded object
fileCache = {}
# loads the given json file from the jar
# jar arg: the path to the used [version].jar, which contains the blockstate json files, model json files and the textures
# file arg: the file to load, e.g. 'blockstates/stone.json'
# returns the loaded object
def loadJarJson(jar, file):
    global fileCache
    if file in fileCache:
        return fileCache[file]

    zf = zipfile.ZipFile(jar, 'r')
    content = None
    try:
        iFile = zf.open("assets/minecraft/{}".format(file))
        content = json.loads(iFile.read())
        iFile.close()
    finally:
        zf.close()
    
    fileCache[file] = content
    return content

# caches loaded texture color, maps texture to color
colorCache = {}
# computes the average RGB value for the given texture
# jar arg: the path to the used [version].jar, which contains the blockstate json files, model json files and the textures
# texture arg: the texture to use, e.g. 'block/stone'
# tint arg: if true, then the texture gets tinted, either in blue if 'block/water_' is in texture, or red if 'block/redstone_dust_' is in texture otherwise in green
# return a dict containing the computed r, g, b, a, noise and empty pixel values
def getRGBValue(jar, texture, tint=False):
    texture = removeNamespace(texture)
    global colorCache
    if texture in colorCache:
        return colorCache[texture]

    with zipfile.ZipFile(jar, 'r') as jarFP:
        textureFP = jarFP.open("assets/minecraft/textures/{}.png".format(texture))
        img = Image.open(textureFP)
        img.load()
        img = img.convert("RGBA")
    
    #tint image
    if "block/water_" in texture:
        img = ImageChops.multiply(img, Image.new('RGBA', img.size, (40,93,255,255)))
    elif "block/redstone_dust_" in texture:
        img = ImageChops.multiply(img, Image.new('RGBA', img.size, (255,51,0,255)))
    elif tint:
        img = ImageChops.multiply(img, Image.new('RGBA', img.size, (102, 255, 76))) # tint in green for grass, vines, ...
    
    width, height = img.size
    r = 0
    g = 0
    b = 0
    a = 0
    noise = 0
    emptyPixels = 0
    minSize = min(width, height)
    n = minSize * minSize
    for x in range(0, minSize):
        for y in range(0, minSize):
            TmpR, TmpG, TmpB, TmpA = img.getpixel((x, y))
            if TmpA == 0:
                n -= 1
                emptyPixels += 1
            else:
                r += TmpR
                g += TmpG
                b += TmpB
                a += TmpA

    r /= n
    g /= n
    b /= n
    a /= (minSize * minSize)
    emptyPixels /= (minSize * minSize)
    var = 0.0
    for x in range(0, minSize):
        for y in range(0, minSize):
            TmpR, TmpG, TmpB, TmpA = img.getpixel((x, y))
            var += (pow(TmpR - r, 2.0) + pow(TmpG - b, 2.0) + pow(TmpB - g, 2.0)) / (3.0 * n)
    
    noise = int(8 * var / ( (n * n - 1) / 12))

    r = int(r)
    g = int(g)
    b = int(b)
    a = int(a)
    noise = int(noise)
        
    if noise > 255:
        noise = 255
    if a > 255:
        a = 255

    if n < minSize * minSize:
        a = max(a, 254)
    
    d = {'r': r, 'g': g, 'b': b, 'a': a, 'n': noise, 'empty': emptyPixels}
    colorCache[texture] = d
    return d
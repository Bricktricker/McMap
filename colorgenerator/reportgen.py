import os
import json
import tempfile
import subprocess

# executes a data run with the --reports and --server options,
# reads the blocks.json report, the leaves tag, the water tag, the lava tag,
# the forge:torches tag (if present) and the forge:snow tag (if present)
# gameDir arg: the path to the .minecraft folder
# version arg: the version that should be used as a String, e.g. '1.15.2', '20w17a', this version MUST exists in the .minecraft/versions folder
# returns a dict with the read data
def genReport(gameDir, version):
    libs = getLibs(gameDir, version)
    
    #build classpath
    cp = '".;'
    for lib in libs:
        libPath = os.path.join(gameDir, "libraries", lib)
        cp += str(libPath)
        cp += ";"

    versionPath = os.path.join(gameDir, "versions", version, version + ".jar")
    cp += str(versionPath)
    cp += '"'

    with tempfile.TemporaryDirectory() as tmpDir:
        fullPath = "java -cp {} net.minecraft.data.Main --output {} --reports --server".format(cp, str(tmpDir))
        subprocess.check_output(fullPath, stderr=subprocess.STDOUT, shell=True, cwd=str(tmpDir))

        # read block report
        report = os.path.join(tmpDir, "reports", "blocks.json")
        with open(report) as f:
            blocks = json.load(f)

        # read leaves tag
        leavesTagsPath = os.path.join(tmpDir, "data", "minecraft", "tags", "blocks", "leaves.json")
        with open(leavesTagsPath) as f:
            leaves = json.load(f)["values"]

        # read water tag
        waterTagsPath = os.path.join(tmpDir, "data", "minecraft", "tags", "fluids", "water.json")
        with open(waterTagsPath) as f:
            water = json.load(f)["values"]
        
        # read lava tag 
        lavaTagsPath = os.path.join(tmpDir, "data", "minecraft", "tags", "fluids", "lava.json")
        with open(lavaTagsPath) as f:
            lava = json.load(f)["values"]

        # torches tags: check if forge:torches exists, either load it or use defaults
        torchTagsPath = os.path.join(tmpDir, "data", "forge", "tags", "blocks", "torches.json")
        if os.path.exists(torchTagsPath):
            with open(torchTagsPath) as f:
                torches = json.load(f)["values"]
        else:
            torches = ["minecraft:torch", "minecraft:wall_torch"]

        # snow tags: check if forge:snow exists, either load it or use defaults TODO: check why needed?
        snowTagsPath = os.path.join(tmpDir, "data", "forge", "tags", "blocks", "snow.json")
        if os.path.exists(snowTagsPath):
            with open(snowTagsPath) as f:
                snow = json.load(f)["values"]
        else:
            snow = ["minecraft:snow", "minecraft:snow_block"]

        grass = ["minecraft:grass_block"] # TODO: why needed?

        out = {
            "report": blocks,
            "tags": {
                "leaves": leaves,
                "water": water,
                "lava": lava,
                "torches": torches,
                "snow": snow,
                "grass_block": grass
            }
        }

        return out
    

# reads the specified versions json file und .minecraft/versions/[version]/[version].json and extracts all needed libraries
# gameDir arg: the path to the .minecraft folder
# version arg: the version that should be used as a String, e.g. '1.15.2', '20w17a', this version MUST exists in the .minecraft/versions folder
# returns a list of paths to the needed libraries relative to the .minecraft/libraries folder
def getLibs(gameDir, version):
    metaDataFile = os.path.join(gameDir, "versions", version, version + ".json")
    if not os.path.exists(metaDataFile):
        raise Exception("Version does not exists")

    with open(metaDataFile) as f:
        versionData = json.load(f)

    libs = versionData["libraries"]
    libs = list(map(lambda e: e["downloads"]["artifact"]["path"], libs))

    return libs

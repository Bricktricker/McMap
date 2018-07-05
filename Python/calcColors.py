import json
from PIL import Image

data = json.loads(open("final.json").read())
path = "C:/Users/Philipp/AppData/Roaming/.minecraft/versions/1.13-pre6/minecraft/"

outData = []

def calcRGBA(img):
    mode = img.mode
    width, height = img.size
    r = 0
    g = 0
    b = 0
    a = 0
    noise = 0
    n = width * height
    if img.mode == "RGBA":
        for x in range(0, width):
            for y in range(0, height):
                TmpR, TmpG, TmpB, TmpA = img.getpixel((x, y))
                if TmpA == 0:
                    n -= 1
                else:
                    r += TmpR
                    g += TmpG
                    b += TmpB
                    a += TmpA

        r /= n
        g /= n
        b /= n
        a /= n
        var = 0.0
        for x in range(0, width):
            for y in range(0, height):
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

    return r, g, b, a, noise


for tex in data:
    fullPath = "{}textures/{}.png".format(path, tex["texture"])
    img = Image.open(fullPath)
    img.load()
    img = img.convert("RGBA")
    r, g, b, a, n = calcRGBA(img)

    col = {"r": r, "g": g, "b": b, "a": a, "n": n}
    d = {"texture": tex["texture"], "from": tex["from"], "to": tex["to"], "color": col, "blockType": tex["blockType"]}
    outData.append(d)

with open('colors.json', 'w') as outfile:
    json.dump(outData, outfile)


import json
from PIL import Image, ImageChops
import csv

def tint_image(col, tint_color):
    subR = col["r"] - 128
    subG = col["g"] - 128
    subB = col["b"] - 128

    addR = subR + tint_color[0]
    addG = subG + tint_color[1]
    addB = subB + tint_color[2]

    addR = max(0, min(addR, 255))
    addG = max(0, min(addG, 255))
    addB = max(0, min(addB, 255))

    return  {"r": addR, "g": addG, "b": addB, "a": col["a"], "n": col["n"]}
    
    #subIm = ImageChops.subtract(Image.new('RGBA', image.size, (128, 128, 128, 255)), image)
    #return ImageChops.add(subIm, Image.new('RGBA', image.size, tint_color))
    #return ImageChops.multiply(image, Image.new('RGBA', image.size, tint_color))

def calcRGBA(img):
    mode = img.mode
    width, height = img.size
    r = 0
    g = 0
    b = 0
    a = 0
    noise = 0
    minSize = min(width, height)
    n = minSize * minSize
    if img.mode == "RGBA":
        for x in range(0, minSize):
            for y in range(0, minSize):
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

    return r, g, b, a, noise


def calc(texture, path, tint, csvFile):
    if csvFile:
        f = open(csvFile, 'r')
        reader = csv.reader(f, delimiter=';')
        rownum = 0
        for row in reader:
            if rownum == 0:
                rownum += 1
                continue

            if row[0] == texture:
                return {"r": int(row[1]), "g": int(row[2]), "b": int(row[3]), "a": int(row[4]), "n": int(row[5])}
            
            rownum += 1
        
    
    fullPath = "{}textures/{}.png".format(path, texture)
    img = Image.open(fullPath)
    img.load()
    img = img.convert("RGBA")
    r, g, b, a, n = calcRGBA(img)

    col = {"r": r, "g": g, "b": b, "a": a, "n": n}

    if tint:
        col = tint_image(col, (176, 246, 107)) 
    if "block/water_" in texture:
        col = tint_image(col, (40,93,255,255))

    if tint:
        pass
        #print(col, texture)
    
    if "block/water_" in texture:
        col["a"] = 36

    return col


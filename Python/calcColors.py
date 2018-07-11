import json
from PIL import Image, ImageChops

def tint_image(image, tint_color):
    return ImageChops.multiply(image, Image.new('RGBA', image.size, tint_color))

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

    return r, g, b, a, noise


def calc(texture, path, tint):
    fullPath = "{}textures/{}.png".format(path, texture)
    img = Image.open(fullPath)
    img.load()
    img = img.convert("RGBA")
    if tint:
        img = tint_image(img, (92,170,71,255))
    if "block/water_" in texture:
        img = tint_image(img, (40,93,255,255))
    r, g, b, a, n = calcRGBA(img)

    col = {"r": r, "g": g, "b": b, "a": a, "n": n}
    
    if "block/water_" in texture:
        col["a"] = 36
        print(col)

    return col


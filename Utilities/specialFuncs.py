import json
from PIL import Image, ImageChops
import csv
import subprocess
import sys
import os
import platform
import shutil
import zipfile

def cleanReport():
    shutil.rmtree("generated", ignore_errors=True)
    shutil.rmtree("logs", ignore_errors=True)
    

def getPathFromVersion(ver):
    if platform.system() is "Windows":
        return os.getenv('APPDATA') + "/.minecraft/versions/" + ver + "/" + ver + ".jar"
    else:
        return os.getenv("HOME") + "/.minecraft/versions/" + ver + "/" + ver + ".jar"

def genReport(jar, lib):
    print("generating minecraft block report...")
    cleanReport()
    if lib is None:
        if platform.system() is "Windows":
            libPath = os.getenv('APPDATA') + "/.minecraft/libraries/"
        else:
            libpath = os.getenv("HOME") + "/.minecraft/libraries/"
    else:
        libPath = lib
    
    deps = []
    deps.append("net/sf/jopt-simple/jopt-simple/5.0.3/jopt-simple-5.0.3.jar")
    deps.append("org/apache/logging/log4j/log4j-core/2.8.1/log4j-core-2.8.1.jar")
    deps.append("org/apache/logging/log4j/log4j-api/2.8.1/log4j-api-2.8.1.jar")
    deps.append("com/mojang/brigadier/1.0.14/brigadier-1.0.14.jar")
    deps.append("com/google/code/gson/gson/2.8.0/gson-2.8.0.jar")
    deps.append("com/google/guava/guava/21.0/guava-21.0.jar")
    deps.append("it/unimi/dsi/fastutil/8.2.1/fastutil-8.2.1.jar")
    deps.append("org/apache/commons/commons-lang3/3.5/commons-lang3-3.5.jar")
    deps.append("io/netty/netty-all/4.1.25.Final/netty-all-4.1.25.Final.jar")
    deps.append("com/mojang/datafixerupper/1.0.21/datafixerupper-1.0.21.jar")
    #Below are deps to remove errors
    deps.append("com/mojang/authlib/1.5.25/authlib-1.5.25.jar")
    deps.append("commons-io/commons-io/2.5/commons-io-2.5.jar")

    classPath = '".;'

    for dep in deps:
        classPath += libPath + dep + ";"

    classPath += jar + '"'
    fullPath = "java -cp {} net.minecraft.data.Main --reports".format(classPath)

    result = subprocess.check_output(fullPath, stderr=subprocess.STDOUT, shell=True)
    return result


cache = {}

def tint_image(image, tint_color):
    return ImageChops.multiply(image, Image.new('RGBA', image.size, tint_color))

def calcRGBA(img, glass):
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
                if TmpA == 0 and not glass:
                    n -= 1
                else:
                    r += TmpR
                    g += TmpG
                    b += TmpB
                    a += TmpA

        r /= n
        g /= n
        b /= n
        a /= (minSize * minSize)
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


def calc(texture, jar, tint, csvFile):
    if texture in cache:
        return cache[texture]

    with zipfile.ZipFile(jar, 'r') as jarFP: 

        if csvFile:
            with open(csvFile, 'r') as f:
                reader = csv.reader(f, delimiter=';')
                rownum = 0
                for row in reader:
                    if rownum == 0:
                        rownum += 1
                        continue

                    if row[0] == texture:
                        return {"r": int(row[1]), "g": int(row[2]), "b": int(row[3]), "a": int(row[4]), "n": int(row[5])}
            
                    rownum += 1

        textureFP = jarFP.open("assets/minecraft/textures/{}.png".format(texture))
        img = Image.open(textureFP)
    
        img.load()
        img = img.convert("RGBA")
        if "block/water_" in texture:
            img = tint_image(img, (40,93,255,255))
        elif tint:
            img = tint_image(img, (102, 255, 76)) 
        
        r, g, b, a, n = calcRGBA(img, "glass" in texture)
        col = {"r": r, "g": g, "b": b, "a": a, "n": n}
    
        if "block/water_" in texture:
            col["a"] = 36

        cache[texture] = col
        textureFP.close()
        return col


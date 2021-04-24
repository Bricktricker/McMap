import math

# Class representing a simple cube from a model
class Cube:

    # constructs a Cube from an 'elements' object
    # elem arg: a dict containing a 'from' and 'to' key (a three element array), a 'faces' key for the Cube faces
    def __init__(self, elem):
        f = elem["from"]
        t = elem["to"]
        self.start = Vec3(f[0], f[1], f[2])
        self.end = Vec3(t[0], t[1], t[2])
        self.faces = {}
        for faceName, faceVal in elem["faces"].items():
            self.faces[faceName] = (faceVal["texture"], "tintindex" in faceVal)

    # returns the volume of this cube, in the range 0 to 1, where 1 is a full block
    def getVolume(self):
        return (abs(self.start.x - self.end.x) * abs(self.start.y - self.end.y) * abs(self.start.z - self.end.z)) / (16*16*16)

    # Returns a tuple (size, texture, isTinted) value for the up face,
    # where 'size' is the size of the 'up' face in the range 0 to 1, where 1 is a full block,
    # 'texture' is the specified texture for the 'up' face
    # if this cube has no 'up' face, 'size' is 0 and 'texture' is None
    def getUpVal(self):
        if "up" not in self.faces:
            return (0, None, False)
        
        xSize = self.end.x - self.start.x
        zSize = self.end.z - self.start.z
        return ((xSize*zSize) / 256, self.faces["up"][0], self.faces["up"][1])

    # Returns a tuple (size, texture, isTinted) value for the bigest side face
    # where 'size' is the size of the biggest side face in the range 0 to 1, where 1 is a full block,
    # 'texture' is the specified texture for the biggest side face
    # if this cube has no side faces, 'size' is 0 and 'texture' is None
    def getSideVal(self):
        value = 0
        face = None
        if "north" in self.faces:
            xSize = self.end.x - self.start.x
            ySize = self.end.y - self.start.y
            value = xSize * ySize
            face = "north"
        elif "south" in self.faces:
            xSize = self.end.x - self.start.x
            ySize = self.end.y - self.start.y
            value = xSize * ySize
            face = "south"
        if "west" in self.faces:
            zSize = self.end.z - self.start.z
            ySize = self.end.y - self.start.y
            v = zSize*ySize
            if v > value:
                value = v
                face = "west"
        elif "east" in self.faces:
            zSize = self.end.z - self.start.z
            ySize = self.end.y - self.start.y
            v = zSize*ySize
            if v > value:
                value = v
                face = "east"

        if value > 0:
            return (value / 256, self.faces[face][0], self.faces[face][1])
        else:
            return (0, None, False)


# Class representing a 3d vector
class Vec3:

    # constructs a 3d Vector from the given _x, _y and _z values
    def __init__(self, _x, _y, _z):
        self.x = _x
        self.y = _y
        self.z = _z

    # returns the length of this vector
    def length(self):
        return math.sqrt(self.x**2 + self.y**2 + self.z**2)

    # returns a normalized vector
    def normalize(self):
        l = self.length()
        return Vec3(self.x / l, self.y / l, self.z / l)

# Class representing a basic block model, basically a list of Cubes
class Model:
    def __init__(self, cubes):
        self.cubes = cubes

    # returns the height of this model
    # if incBillboards is false, Cubes that are flat (have no volume) are ignored
    def getHeight(self, incBillboards = True):
        minY = 16
        maxY = 0
        for c in self.cubes:
            if not incBillboards and c.getVolume() == 0:
                continue
            minY = min(minY, c.start.y)
            maxY = max(maxY, c.end.y)
        return max(maxY - minY, 0) / 16

    # returns the surface thats occupied by this model, when viewing from the top, value is in range 0 to 1
    # TODO: currently returns the value for the biggest face, FIXME
    def getTopSurface(self):
        maxSize = 0
        for c in self.cubes:
            if "up" not in c.faces:
                continue
            surfSize = abs((c.end.x - c.start.x) * (c.end.z - c.start.z))
            maxSize = max(maxSize, surfSize)
        return maxSize / (16*16)

    # returns the volume of this model in the range 0 to 1, where 1 is a full block
    def getVolume(self):
        volume = 0
        for c in self.cubes:
            volume += c.getVolume()
        return volume

    # returns the lowest point in the model in the range 0 to 1, where 1 is a full block
    def getLowestPoint(self):
        minY = 16
        for c in self.cubes:
            minY = min(minY, c.start.y)
        return minY / 16


# McMap
mcmap is a tiny map visualizer for Minecraft 1.13.1 Maps are drawn from an isometric perspective.

## About
This project is a fork of [WRIM/mcmap](https://github.com/WRIM/mcmap) beta version. [Original minecraftforum thread](https://www.minecraftforum.net/forums/mapping-and-modding-java-edition/minecraft-tools/1260548-mcmap-isometric-renders-ssp-smp-minecraft-1-3-1)

## Basic usage
- To display help run: ``` mcmap.exe```
- A simple render of your world run: ``` mcmap.exe WORLDPATH ```
- To include skylight run: ``` mcmap.exe -skylight WORLDPATH ```

## Update Blockdata
1. Download the latest server.jar from minecraft.net
2. Start it with ```java -classpath server.jar net.minecraft.data.Main --reports ```
3. Copy the ```blocks.json``` in to the *Utilities* folder of this project
4. Run the ```BlockIds.py``` script to update the ```BlockIDs.json``` file

## Update default colors
1. Download the latest server.jar from minecraft.net
2. Start it with ```java -classpath server.jar net.minecraft.data.Main --reports ```
3. Copy the ```blocks.json``` in to the *Utilities* folder of this project
4. Unpack the latest minecraft jar (usually under `%appdata%/.minecraft/versions/[latest]`)
5. Run the ```loadTexture.py PATH_TO_UNPACKED_MINECRAFT_JAR``` to update the colors.json file

You can also modify the specialBlocks.csv file to preset certain colors for special textures.

## Compatibility
This software is able to render all minecraft saves, which are in the anvil file format (MC >= 1.2.1), but maybe not all blocks will be rendered correctly. I recommend to update your minecraft save to minecraft version 1.13.1 to ensure optimal rendering.

## Dependencies
 - [zlib](https://zlib.net/)
 - [libPng](www.libpng.org)
 - [nlohman/json](https://github.com/nlohmann/json) (Already included in project)

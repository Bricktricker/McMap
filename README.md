# McMap
mcmap is a tiny map visualizer for Minecraft 1.13.2 Maps are drawn from an isometric perspective.

## About
This project is a fork of [WRIM/mcmap](https://github.com/WRIM/mcmap) beta version.

## Basic usage
- To display help run: ``` mcmap.exe```
- A simple render of your world run: ``` mcmap.exe WORLDPATH ```
- To include skylight run: ``` mcmap.exe -skylight WORLDPATH ```

## Update Blockdata
- Use the ```BlockIDs.py``` file from the ```Utilities``` folder for this.
- To get the help menu start it with ```-h```
- If you have Minecraft installed in the default directory, simply start it with ```-v VERSION```, where version is the minecraft version e.g 1.13.2
- If you have Minecraft installed in a different folder, you need to suply the ```-j``` (Path to the Minecraft jar file) and ```-l``` (Path to the Minecraft library folder)

## Update colors
- Use the ```loadTextures.py``` file from the ```Utilities``` folder for this.
- To get the help menu start it with ```-h```
- If you have Minecraft installed in the default directory, simply start it with ```-v VERSION```, where version is the minecraft version e.g 1.13.2
- If you have Minecraft installed in a different folder, you need to suply the ```-j``` (Path to the Minecraft jar file) and ```-l``` (Path to the Minecraft library folder)
- You can also modify the ```specialBlocks.csv``` file to preset certain colors for special textures or pass a new one with ```-s```

## Compatibility
This software is able to render all minecraft saves, which are in the anvil file format (MC >= 1.2.1), but maybe not all blocks will be rendered correctly. I recommend to update your minecraft save to Minecraft version 1.13 or newer to ensure optimal rendering.

## Dependencies
 - [zlib](https://zlib.net/)
 - [libPng](http://www.libpng.org)
 - [nlohman/json](https://github.com/nlohmann/json) (Already included in project)
### To update Blockdata/colors
 - [Python 3](https://www.python.org/)
 - [Pillow](https://pillow.readthedocs.io/en/5.3.x/)

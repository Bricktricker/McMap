# McMap
McMap is a tiny map visualizer for Minecraft 1.13.2+ 
Maps are drawn from an isometric perspective.

## About
This project is a fork of [WRIM/mcmap](https://github.com/WRIM/mcmap) beta version.

## Basic usage
- To display help run: `mcmap.exe`
- To get a simple render of your world run: `mcmap.exe WORLDPATH`
- To include skylight run: `mcmap.exe -skylight WORLDPATH`

## Update colors
- Run the `colorgen.py` file in the `colorgenerator` subfolder with `python colorgen.py`
- To get the help menu start it with `-h`
- If you have Minecraft installed in the default directory, simply start it with `-v VERSION`, where VERSION is the minecraft version you want to generate the color information for e.g 1.16.5
- If you have Minecraft installed in a different folder, you need to supply the path to your Minecraft installation with `-g PATH`
- copy the generated `colors.json` file next to the mcmap executable

## Dependencies
 - [zlib](https://zlib.net/)
 - [libPng](http://www.libpng.org)
 - [nlohman/json](https://github.com/nlohmann/json) (Already included in project)
### Dependencies needed to update colors
 - [Python 3](https://www.python.org/)
 - [Pillow](https://pillow.readthedocs.io/en/5.3.x/)

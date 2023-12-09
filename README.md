# StarDust-DS
 3D Cross platform game engine for DS and PC
 
 See example here:https://github.com/Yackerw/StarDust-Example

License: Just credit me if you use it. Either in a prominent place in game or where ever the game may be available, preferably.

All in one 3D game engine, should handle everything you'd need for a 3D game on the DS, with bonus PC functionality. Made entirely in C (besides the PC XAudio2 interface, made in C++)

Features:

3D Collision:

-Sphere on mesh collision with acceleration structures

-Sphere on sphere collision

-Sphere on box collision

-Ray on mesh, sphere, and box collision

Audio:

-Streamed music with loop points

-Custom SFX library

-Uses .wav files

Rendering:

-Supports both rigged & unrigged meshes with multiple materials

-Up to 30 bones can be used per mesh with hardware acceleration, 256 bones can be used per mesh at heavy cost of performance (on DS)

-Single bone weights, sorry, DS doesn't support multi-bone weights

-Supports sprites on both screens, and one 256x256 background for the bottom screen.

-Bottom screen emulation on PC

Tools:

-Custom blender exporter for models and animations (only 2.79 supported, apologies)

-Custom image converter supporting various texture, sprite, and background formats

Please view the page for tools on the wiki before using! https://github.com/Yackerw/StarDust-DS/wiki/Tools

Documentation:

Available here: https://github.com/Yackerw/StarDust-DS/wiki

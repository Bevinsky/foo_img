This foobar plugin lets you open BMP, PNG and JPEG images and play their spectrogram.

![image](http://i.imgur.com/b92pIuL.jpg)

It's cool

## How to compile

Get the foobar sdk, it should have instructions for how to set it up.

1. Create a VS project for this based on the foo_tutorial project.
2. Add all the projects of the SDK + this project to a solution.
3. Add all the SDK projects as references to this one.
4. Compile ImageMagick as StaticMT (Not DLL) for Debug and Release.
5. Add all of the IM lib files to the Linker for this project for both Debug (DB) and Release (RL).
6. Add the include path for IM for Magick++ (IM/Magick++/lib/ and IM/)
7. I think that's it?

## Settings for project maybe

Configuration Type: Dynamic Library
MFC: Standard Windows Libraries
Charset: Unicode
Runtime Library: Multi-Threaded (Debug)
Precompiled header

Original bmp2wav courtesy of ed @ Rizon. Foobar plugin and legibility by Vin @ Rizon.

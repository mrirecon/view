

Small image viewer for multi-dimensional files. Compiles
on Linux and Mac OS X. It needs some libraries from
the Berkeley Advanced Reconstruction Toolbox (BART).

https://mrirecon.github.io/bart/

![viewer](viewer.png)


Installation:

Requires BART up to commit e4df99aa75e0344931ba2e76d9ae052b02da498f

Mac OS X:

sudo port install pkgconfig
sudo port install gtk3
sudo port install adwaita-icon-theme

Linux:
sudo apt-get install libgtk-3-dev



Compile with make after setting the TOOLBOX_PATH
environment variable.



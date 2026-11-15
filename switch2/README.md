# DroidDeck for Switch 2

A native Switch 2 homebrew build of the DroidDeck launcher, made with
[ounce-devkit](https://github.com/ouncebrew/ounce-devkit).

```sh
export DEVKITPRO=/opt/devkitpro
make          # DroidDeck.nro
make host     # desktop preview in build-host/, needs SDL2, SDL2_ttf and SDL2_image
```

Copy `DroidDeck.nro` to `sdmc:/switch/` and start it from hbloader.

## What is here

- The launcher shell: the rail, the Steam page and the Setup device report, drawn with SDL 2 on OunceVK's OpenGL ES.
- `source/vkinfo.c`, a port of `app/src/main/cpp/deviceinfo/vkinfo.c` to OunceVK's Vulkan.
- A device report that checks kaiHEN, executable JIT memory (FEX needs it), the Vulkan extensions DXVK needs, SD card space and heap size.

## What is not

Steam, Proton, FEX, the desktop and the stores do not run. They are Linux programs that DroidDeck starts under proot on
Android's Linux kernel. Horizon has no Linux system calls, `fork`, `exec` or `ptrace`, so the runtime needs a Linux
system call layer for Horizon before any of it can start. The Kotlin UI (about 170,000 lines) needs Android and is not
compiled here; this shell is new C code.

The build has not been run on a console.

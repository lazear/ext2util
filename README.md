# ext2util
## Command line interface for writing and reading files for ext2 images

I've moved most of my development pipeline to the Windows Subsystem for Linux, and really like it.
However, WSL doesn't provide an ability to mount disk image files, since there is no loop device.
I needed a tool to be able to read/write/debug ext2 disk image files - so I wrote one.

Currently, there is no option parsing, but that will be added later.

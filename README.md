# ext2util 0.1
### Command line interface for writing and reading files for ext2 images

I've moved most of my development pipeline to the Windows Subsystem for Linux, and really like it.
However, WSL doesn't provide an ability to mount disk image files, since there is no loop device.
I needed a tool to be able to read/write/debug ext2 disk image files - so I wrote one.

features:
* support for variable logical block sizes
* file read/write operations (by name, or by specific inode number)
* listing files in directories
* direct display of block and inode information


usage:
<pre> 
-x [disk.img]
-w is write (-i designates inode, optional | -f designates file to write)
-r is read (non functional)
-d is dump inode information
-l is ls root directory
</pre>
options can be combined like any other getopt program, <pre>$ ./ext2util -x disk.img -wdi 5 -f stage.bin</pre>

To generate an ext2 image, execute the following commands:
<pre>
$ dd if=/dev/zero of=ext2.img bs=1k count=16k
$ mke2fs ext2.img
</pre>

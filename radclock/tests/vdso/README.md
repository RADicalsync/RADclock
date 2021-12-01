# Compiling with VDSO support
The compilation procedure is rather odd as you need to link the executable against the VDSO itself - which exists only in kernel memory and the original kernel build directory. So far I've not found another way to get hold of the VDSO.

First build `dump-vdso` a simple tool borrowed from a [Google Project](https://kernel.googlesource.com/pub/scm/linux/kernel/git/luto/misc-tests/+/5655bd41ffedc002af69e3a8d1b0a168c22f2549/dump-vdso.c):


   gcc dump-vdso.c -o dump-vdso


Running `dump-vdso` will dump out the contents of the VDSO in memory to `stdout`. This isn't terribly useful so instead dump it to a file `vdso.so`:

   ./dump-vdso > vdso.so

You can verify that this worked by doing:

   objdump -T vdso.so

Note: You need to be running a kernel with `ffclock_` VDSO support to get the symbols you need to do the next step

You can then build `test-vdso` with:

   gcc test-vdso.c -o test-vdso vdso.so

You can now run `test-vdso` with:

   ./test-vdso

You should get something like:

   result: 0
   counter: 2833691

Note: At the moment, the counter value seems nonsensical, but it *is* being set by something. So the VDSO mechanism is working even if something else isn't quite right yet


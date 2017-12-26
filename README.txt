To compile and run the code:

1. Download the zip file from Moodle.

2. Move the contents of the zip file (mem.c, testmem.c, and Makefile) onto ocelot, 
or wherever you're going to test it, and make sure they're in a newly created 
directory with nothing else in that directory.

3. Type "make" at the command line/terminal. This should compile the code. 
NOTE - 
I did not implement shared libraries due to the amount of time I had left, so the
Makefile is a simple Makefile that compiles and runs testmem.c.

4. Type "./testmem" to see the output of testmem.c, which tests the functions inside
mem.c.
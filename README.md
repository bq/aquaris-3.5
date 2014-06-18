WHAT IS THIS?
=============

Linux Kernel source code for the device bq Aquaris 3.5

BUILD INSTRUCTIONS?
===================

Specific sources are separated by branches and each version is tagged with it's corresponding number. First, you should
clone the project:

	$ git clone git@github.com:bq/aquaris-3.5.git

After it, choose the version you would like to build:

	$ cd aquaris-3.5/
	$ git checkout 1.1.0-20131206-2131


Finally, build the kernel:

	$ ./makeMtk -t eastaeon72_wet_jb3 n k

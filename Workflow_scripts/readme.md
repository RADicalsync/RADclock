Copyright (C) 2022 The RADclock Project (see AUTHORS file)
This file is part of the RADclock program.

Overview
========

This directory holds scripts to help seed the workflows for developers joining the project.
It is provided `as is', and should be used in **readonly mode** only:  required modifications to 
suit your local workflows should not be pushed upstream!
Instead, it would be best to copy these outside the repo and adjust the path variables 
in the scripts to match.  In particular secret files that the scripts refer to, but are not included 
in the repo, when created locally MUST be kept outside the repo or included in .gitignore  
at a minimum!   These are:
   VM:   id_rsa.pub  
   RPi:  id_rsa.pub   sshaccessline    [ see comment block in the latter ]

The scripts cover both the FF kernel and userland radclock requirements, as well as initial
setup of directories and scripts to support the workflow.  
They cover the needs of FreeBSD and Linux over ARM64, and Linux on ARM-RPi .

The repo is not cloned into the target machine. Instead source and/or executables are copied
from the repo into the target as required.  This is convenient, for example it allows WIP 
changes to the code, not yet committed, to be tried out just by running a script. 

For the FF patched kernel, the approach is to directly use the FF-modified "FFfiles" stored in 
OS and version specific CurrentSource directories under repo/kernel/, rather than patches.


## 
portloop               largely-automates the "port-loop" process for detailed generation of the 
 git commits supporting the porting of FFfiles from an Old OS version with an existing port,
 to a new one.  Works for Linux or FreeBSD.  Is run on the host machine (not a VM).  
 See header for usage.

## VM  
This directory holds scripts suitable for development within an amd64 Virtual Machine, though
in fact the machine could just as easily be a real one.  A key feature is that the host 
directory is mounted in the VM, so that access to the repo, and workflow scripts, is 
immediate. Another advantage is any VM snapshots will benefit from the latest generation of
scripts whenever reloaded. 
On the whole, the scripts are FreeBSD/Linux `universal', ie they detect the OS and react 
accordingly.  Some of the scripts have command line options that can be useful.

### Initial setup and testing
setupVM                setup everything from scratch, including creating and seeding useful 
                       directories, a useful .bashrc, and setting up symlinks for the scripts
fixupbash              semi-automated hack to fix a wierd problem in bashrc installation
collect_FFfiles_Linux  get the files to be FF-patched from the linux source tree
setupRAD               configure RAD build scripts for the first time, or rebuild them if needed

### Setup a new FF kernel
update_workingFFsource grab the version you ask for from the repo, or an exact match to,
                       the running system, or if not grab the latest.
move_into_srctree      copies those files into the sourcetree
quick_FF_build         compile and install (option for original unpatched kernel)

### Porting of FF code for CURRENT to a more recent main, in both host and VM repos
RADrepo:  lives on the host, holds FFfiles, editing done here
CURRrepo: lives in the VM, holds full source tree, testing done here and rebasing of main.
reBaseTest:     tracks main and tries to assess implications for FFfiles,
                including a rebase trial in a safe environment to see if a
                new port is needed. RADrepo is untouched.
seedCURRport:   following a decision to change the base on main of FFport
                branch, both repos are seeded appropriately, ready for
                the usual VM based workflow to complete and test.
                Main is not advanced.
                Is used following reBaseTest and possible manual tweaking.
commitCURRport: once tested and finalised, the WDs in each repo are
                committed and the base FFfiles recorded to support possible
                future portloop use using (O,O+) from CURRENT.
                WD's are not changed, and main is not advanced.
transferCommits:maps over the finalised patch-sequence branch from RADrepo 
                to CURRrepo, preserving commit messages.

See comment blocks of each function for many more details and descriptions of use cases.

### Creation of a new tarball RADclock package
cleanbuildtree
createPatch            a patch of the FF source against the source of the original installed OS
createDist

### Compile and install RADclock
update_workingRADclock grab the latest from the repo
quick_RAD_build        compile and install

### Linux only
update_linuxSource     update source to a new/desired SUBLEVEL.  Is somewhat smart
                       but requires supervision. Run with "-i"  to get risk free information.
save_linuxSource       save and recover files in /usr/src to/from the host OS, sufficient to 
                       enable recovery of the system (debs files) or an ability to compile it.
                       See header for useful workflows including other scripts.
install_earthlyFF      install amd64 kernel already built by earthly, stored in repo 
                       Can also be used to install .debs take from other source, eg 
                       existing ones in /usr/src with  "-e"
install_earthlyRAD     install a radclock already built by earthly, stored in repo

### Typical workflow once set up
quick_FF_build         gets the latest FF source, compiles, installs, ready to reboot
                       Just compiles the current source with "-n"
quick_RAD_build        gets the latest RAD source, compiles, installs, tests, ready to run



## RPi
This directory holds scripts suitable for development for a Raspberry Pi (RPi) on ARM.
These have been tested on 64bit Pi-4B.
Most of these are symbolic links to VM scripts which have been written to be  
AMD/ARM-Pi universal.  ie they detect if on a Pi or not and react accordingly. 

The Pi of course has a different and more complex bootstrapping procedure as a microSD
card must first be prepared, in particular since we do this *headless* from the very beginning
(no screen or keyboard, ssh access only).
In normal operation a key difference is that in the VM files (both source and scripts) are 
obtained over a mount, whereas on the Pi source files must be transferred over, and the 
scripts must already be present on the Pi, this being set up during the bootstrapping phase.

### On the Host:  set up the microSD card for the Pi for initial boot
setupRPi        sets up a given standard kernel and the initial headless connectivity,
                as well as pre-positioning all needed scripts on the card.

### On the PI:  bootstrap the workflow 
setup-onPi      creates directories, installs scripts into position, and sets up symlinks to scripts.
                Sets up ongoing headless operation.

###  Typical workflow once set up
install_earthlyFF      install a new kernel already built by earthly, stored in repo
install_earthlyRAD     install a radclock already built by earthly, stored in repo
 **OR compile radclock locally as in the VM (faster)** 
update_workingRADclock grab the latest from the repo
quick_RAD_build        compile and install


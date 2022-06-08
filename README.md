# RADCLOCK README

Documentation is located in the Documentation directory.
Start with Documentation/RADclock_guide.txt, which describes the available
documentation, and provides notes for installation and use.
If further help is needed please contact Darryl.Veitch@uts.edu.au .


## Building with Earthly

Earthly is a containerised build system that leverages Docker to create repeatable and 
isolated builds, based on targets defined in "Earthfile". 
For example, assuming Earthly is installed (and docker running), you can build the 
amd64 Debian packages for a FF-capable kernel needed by RADclock with :
```
earthly +deb-kernel-build-patched
```
from the main repo directly. More detail is given in RADclock_guide.txt . 


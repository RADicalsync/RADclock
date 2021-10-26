# RADCLOCK README


Documentation is located in the Documentation directory.
Start with Documentation/RADclock_guide.txt, which describes the available
documentation, and provides notes for installation and use.
If further help is needed please contact Darryl.Veitch@uts.edu.au .


## Building with Earthly

Earthly is a containerised build system that leverages Docker to create repeatable and isolated builds. Assuming Earthly is installed, you can build the amd64 Debian packages with:


```
earthly +kernel-build-patched
```


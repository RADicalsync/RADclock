# Simple Earthfile to build the basic RADclock app and library
# 
# By using debian:buster-slim, we are forced to install all the tools and libraries we need.
# This has the happy side effect that we will capture not only our build and library dependencies
# but we also provide the means to compile and ultimately package it as well.
#
# Repeatable builds ftw :)

# Use a bare bones image - make sure we capture all our dependencies
FROM debian:buster-slim

# Create our working directory
WORKDIR /RADclock

debian-deps:
    # Update the package cache
    RUN apt-get update
    # Pull in the tools we need to actually build it
    RUN apt-get -y install build-essential autoconf git libtool-bin vim
    # Pull in the libraries the build needs
    RUN apt-get -y install libpcap-dev libnl-3-dev libnl-genl-3-dev

kernel-deps:
    FROM +debian-deps
    # Add Debians sources
    RUN echo "deb-src http://deb.debian.org/debian buster main" > /etc/apt/sources.list.d/main.list
    # Update apt cache
    RUN apt-get update
    # Install the tools we need for kernel builds
    RUN apt-get -y install build-essential fakeroot
    # Install build dependencies for the kernel
    RUN apt-get -y build-dep linux
    # Pull Linux source
    RUN apt-get -y source linux

kernel-config:
    FROM +kernel-deps
    # Jump in
    WORKDIR linux-4.19.208
    # Build default config
    RUN make defconfig

kernel-build:
    FROM +kernel-config
    # Build the kernel
    RUN make -j`nproc` bindeb-pkg
    # Export the packages
    SAVE ARTIFACT /RADclock/linux*.deb AS LOCAL ./artifacts/kernel/
    #RUN false

kernel-config-patched:
    FROM +ff-kernel-src
    # Jump in
    WORKDIR linux-4.19.208
    # Build default config
    RUN make defconfig

kernel-build-patched:
    FROM +kernel-config-patched
    # Build the kernel
    RUN make -j`nproc` bindeb-pkg
    # Export the packages
    SAVE ARTIFACT /RADclock/linux*.deb AS LOCAL ./artifacts/kernel-patched/

ff-kernel-src:
    # Duplication - need to fix this
    FROM +kernel-deps
    # Handy constants
    ARG SRC=kernel/linux/4.19.0/CurrentSource
    ARG DEST=linux-4.19.208
    # Create driver directory
    RUN mkdir -p $DEST/drivers/ffclock/

    # Copy in the files
    COPY $SRC/af_inet.c                         $DEST/net/ipv4/
    COPY $SRC/af_packet.c                       $DEST/net/packet/
    COPY $SRC/asm-generic_sockios.h             $DEST/include/uapi/asm-generic/sockios.h
    COPY $SRC/dev.c                             $DEST/net/core/
    COPY $SRC/drivers_Kconfig                   $DEST/drivers/Kconfig
    COPY $SRC/drivers_Makefile                  $DEST/drivers/Makefile

    COPY $SRC/ffclock.c                         $DEST/drivers/ffclock/
    COPY $SRC/ffclock.h                         $DEST/include/linux/
    COPY $SRC/Kconfig                           $DEST/drivers/ffclock/
    COPY $SRC/Makefile                          $DEST/drivers/ffclock/

    COPY $SRC/skbuff.c                          $DEST/net/core/
    COPY $SRC/skbuff.h                          $DEST/include/linux/
    COPY $SRC/sock.c                            $DEST/net/core/
    COPY $SRC/socket.c                          $DEST/net/
    COPY $SRC/sock.h                            $DEST/include/net/
    COPY $SRC/sockios.h                         $DEST/include/uapi/linux/
    COPY $SRC/syscall_32.tbl                    $DEST/arch/x86/entry/syscalls/
    COPY $SRC/syscall_64.tbl                    $DEST/arch/x86/entry/syscalls/
    COPY $SRC/syscalls.h                        $DEST/include/linux/
    COPY $SRC/time.c                            $DEST/kernel/time/
    COPY $SRC/timekeeping.c                     $DEST/kernel/time/
    COPY $SRC/timekeeping.c                     $DEST/kernel/time/
    COPY $SRC/vclock_gettime.c                  $DEST/arch/x86/entry/vdso/
    COPY $SRC/vgtod.h                           $DEST/arch/x86/include/asm/

    # Copy assembly build scripts for 64 bit VDSO
    COPY $SRC/vdso.lds.S                        $DEST/arch/x86/entry/vdso/
    COPY $SRC/vdsox32.lds.S                     $DEST/arch/x86/entry/vdso/

    # Copy assembly build scripts for 32 bit VDSO (needed?)
    COPY $SRC/vdso32.lds.S               $DEST/arch/x86/entry/vdso/vdso32/


copy-src:
    FROM +ff-kernel-src
    # Switch our working directory back
    WORKDIR /RADclock
    # Copy everything for now
    COPY . ./
    # Generate version
    RUN ./version.sh


build-radclock-no-kernel:
    FROM +copy-src
    RUN autoreconf -i
    # Configure the build
    RUN ./configure --prefix /radclock-build
    # Actually build it
    RUN make
    # Install it so we can grab it
    RUN make install
    # Pull the artifacts out
    SAVE ARTIFACT /radclock-build AS LOCAL ./artifacts/


build-radclock-with-kernel:
    FROM +copy-src
    # Install the netlink libraries for the RADclock kernel support
    RUN apt-get install -y libnl-3-dev libnl-genl-3-dev
    # Autconf
    RUN autoreconf -i
    # Configure the build
    RUN ./configure --prefix /radclock-build --with-radclock-kernel
    # Actually build it
    RUN make
    # Install it so we can grab it
    RUN make install
    # Pull the artifacts out
    SAVE ARTIFACT /radclock-build AS LOCAL ./artifacts/radclock-with-kernel/


build-arm64:
    BUILD --platform=linux/arm64 +kernel-build-patched


bh-deps:
    RUN apt-get -yqq update 
    RUN apt-get -yqq install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu make wget xz-utils git flex bison libssl-dev bc file build-essential rsync kmod cpio
    GIT CLONE --branch rpi-4.19.y https://github.com/raspberrypi/linux.git /linux
    WORKDIR /linux


bh-patch:
    # Duplication - need to fix this
    FROM +bh-deps
    # Handy constants
    ARG SRC=kernel/linux/4.19.127/CurrentSource
    ARG DEST=/linux
    # Create driver directory
    RUN mkdir -p $DEST/drivers/ffclock/

    # Copy in the files
    COPY $SRC/af_inet.c                         $DEST/net/ipv4/
    COPY $SRC/af_packet.c                       $DEST/net/packet/
    COPY $SRC/asm-generic_sockios.h             $DEST/include/uapi/asm-generic/sockios.h
    COPY $SRC/dev.c                             $DEST/net/core/
    COPY $SRC/drivers_Kconfig                   $DEST/drivers/Kconfig
    COPY $SRC/drivers_Makefile                  $DEST/drivers/Makefile

    COPY $SRC/ffclock.c                         $DEST/drivers/ffclock/
    COPY $SRC/ffclock.h                         $DEST/include/linux/
    COPY $SRC/Kconfig                           $DEST/drivers/ffclock/
    COPY $SRC/Makefile                          $DEST/drivers/ffclock/

    COPY $SRC/skbuff.c                          $DEST/net/core/
    COPY $SRC/skbuff.h                          $DEST/include/linux/
    COPY $SRC/sock.c                            $DEST/net/core/
    COPY $SRC/socket.c                          $DEST/net/
    COPY $SRC/sock.h                            $DEST/include/net/
    COPY $SRC/sockios.h                         $DEST/include/uapi/linux/
    COPY $SRC/syscall_32.tbl                    $DEST/arch/x86/entry/syscalls/
    COPY $SRC/syscall_64.tbl                    $DEST/arch/x86/entry/syscalls/
    COPY $SRC/syscalls.h                        $DEST/include/linux/
    COPY $SRC/time.c                            $DEST/kernel/time/
    COPY $SRC/timekeeping.c                     $DEST/kernel/time/
    COPY $SRC/timekeeping.c                     $DEST/kernel/time/
    COPY $SRC/vclock_gettime.c                  $DEST/arch/x86/entry/vdso/
    COPY $SRC/vgtod.h                           $DEST/arch/x86/include/asm/

    # Copy assembly build scripts for 64 bit VDSO
    COPY $SRC/vdso.lds.S                        $DEST/arch/x86/entry/vdso/
    COPY $SRC/vdsox32.lds.S                     $DEST/arch/x86/entry/vdso/

    # Copy assembly build scripts for 32 bit VDSO (needed?)
    COPY $SRC/vdso32.lds.S               $DEST/arch/x86/entry/vdso/vdso32/

bh-defconfig:
    FROM +bh-patch
    RUN make ARCH=arm64 bcm2711_defconfig

bh-disable-debug:
    FROM +bh-defconfig
    # Remove all the debug stuff
    RUN scripts/config --disable DEBUG_INFO
    # Don't compress the kernel
    RUN sed -i '/KBUILD_IMAGE/ s/.gz//' arch/arm64/Makefile

bh-build:
    FROM +bh-disable-debug
    RUN ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- make bindeb-pkg
    SAVE ARTIFACT ../*.deb AS LOCAL artifacts/arm64/



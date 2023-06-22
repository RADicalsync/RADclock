# Copyright (C) 2021 The RADclock Project (see AUTHORS file)
#
# Earthfile to build the RADclock app and library, and the FFkernel on both
# amd64 and arm64 for the Raspberry Pi (RPi).
VERSION 0.7

# Select the distribution we want
# ARG DIST=buster
ARG --global DIST=bullseye
#ARG DIST=bookworm
#ARG DIST=11.3

# Use a bare bones image - make sure we capture all our dependencies
FROM debian:$DIST-slim

# Create our working directory
WORKDIR /RADclock

# Dependencies common to AMD and ARM
base-deps:
	# Update the package cache
	RUN apt-get update
	# Pull in the tools we need to actually build it
	RUN apt-get -y install build-essential autoconf git libtool-bin vim
	# Pull in the libraries the build needs
	RUN apt-get -y install libpcap-dev libnl-3-dev libnl-genl-3-dev


#### AMD specific targets ####
deb-kernel-deps:
    FROM +base-deps
    # Add Debians sources
    RUN echo "deb-src http://deb.debian.org/debian $DIST main" > /etc/apt/sources.list.d/main.list
    # Update apt cache
    RUN apt-get update
    # Enables deb pkg creation when not root
    RUN apt-get -y install fakeroot
    # Install build dependencies for the kernel
    RUN apt-get -y build-dep linux
    # Pull Linux deb source
    RUN apt-get -y source linux
    # Work out what we have and jump in [ went from 4.19.{181,208,235,..} ]
	 ARG osversion=$(ls -tr |tail -1 |cut -d'-' -f2)
    WORKDIR linux-$osversion			# osversion extractable by other targets
	 RUN --no-cache pwd
	 # Save the source (needed for compile on VM)
	 ARG DEST="./artifacts/kernelsourcefiles/deb-$osversion/"
	 RUN echo "Dumping source to $DEST"
	 SAVE ARTIFACT .    						AS LOCAL $DEST"linux-source"

deb-kernel-config:
	 # Get essential packages and kernel source
    FROM +deb-kernel-deps
    # Build default config
    RUN make defconfig

deb-kernel-config-patched:
	 # Get essential packages and kernel source
	 FROM +deb-kernel-deps
	 # Impose latest FFfile changes onto source
	 ARG osversion=$(pwd | tr '/' '\n' |tail -1 |cut -d'-' -f2) # find matching version
    DO +COPY_IN_FFFILES --SRC=kernel/linux/$osversion/CurrentSource
    # Build default config
    RUN make defconfig

deb-kernel-build:
    FROM +deb-kernel-config
	 # Modify config to match Darryl's VM settings
    RUN scripts/config --disable DEBUG_INFO
    RUN scripts/config --set-str SYSTEM_TRUSTED_KEYS ""
    RUN scripts/config --set-str MODULE_SIG_KEY certs/signing_key.pem
	 # Build the kernel
    RUN make -j`nproc` bindeb-pkg
    # Export the packages
    ARG TARGETARCH
    SAVE ARTIFACT /RADclock/linux*.deb AS LOCAL ./artifacts/kernel/$TARGETARCH/
    #RUN false

deb-kernel-build-patched:
    FROM +deb-kernel-config-patched
	 # Modify config to match Darryl's VM settings
    RUN scripts/config --disable DEBUG_INFO
    RUN scripts/config --set-str SYSTEM_TRUSTED_KEYS ""
	 RUN scripts/config --set-str MODULE_SIG_KEY certs/signing_key.pem
    # Build the kernel
    RUN make -j`nproc` bindeb-pkg
    # Export the packages
    ARG TARGETARCH
    SAVE ARTIFACT /RADclock/linux*.deb AS LOCAL ./artifacts/kernel-patched/$TARGETARCH/

# The FFfiles used are mainly the same under amd64 and arm64
# The SRC directory must be set correctly on calling
COPY_IN_FFFILES:
	COMMAND
	ARG SRC
	ARG DEST="."
	ARG ARM="NO"
	# Create driver directory
	RUN mkdir -p $DEST/drivers/ffclock/

	# Copy in the files
	COPY $SRC/af_inet.c						$DEST/net/ipv4/
	COPY $SRC/af_packet.c					$DEST/net/packet/
	IF [ "$ARM" = "YES" ]	# for raspbian, still using both iotcl files
		COPY $SRC/asm-generic_sockios.h	$DEST/include/uapi/asm-generic/sockios.h
	END
	COPY $SRC/dev.c							$DEST/net/core/
	COPY $SRC/drivers_Kconfig				$DEST/drivers/Kconfig
	COPY $SRC/drivers_Makefile				$DEST/drivers/Makefile

	COPY $SRC/ffclock.c						$DEST/drivers/ffclock/
	COPY $SRC/ffclock.h						$DEST/include/linux/
	COPY $SRC/Kconfig						$DEST/drivers/ffclock/
	COPY $SRC/Makefile						$DEST/drivers/ffclock/

	COPY $SRC/skbuff.c						$DEST/net/core/
	COPY $SRC/skbuff.h						$DEST/include/linux/
	COPY $SRC/sock.c						$DEST/net/core/
	COPY $SRC/socket.c						$DEST/net/
	COPY $SRC/sock.h						$DEST/include/net/
	COPY $SRC/sockios.h						$DEST/include/uapi/linux/
	COPY $SRC/syscalls.h					$DEST/include/linux/
	COPY $SRC/time.c						$DEST/kernel/time/
	COPY $SRC/timekeeping.c					$DEST/kernel/time/
	#COPY $SRC/vclock_gettime.c				$DEST/arch/x86/entry/vdso/
	#COPY $SRC/vgtod.h						$DEST/arch/x86/include/asm/

	# ARCH specific files (related to syscall numbers only)
	#   ARM specific files are offensive to AMD, must not copy in
	#   AMD specific files are ignored by ARM, no point copying in
	IF [ "$ARM" = "YES" ]
		COPY $SRC/unistd.h					$DEST/include/uapi/asm-generic/
		RUN echo "Copying in ARM specific files"
	ELSE
		COPY $SRC/syscall_32.tbl			$DEST/arch/x86/entry/syscalls/
		COPY $SRC/syscall_64.tbl			$DEST/arch/x86/entry/syscalls/
		RUN echo "Copying in AMD specific files"
	END

	# Copy assembly build scripts for 64 bit VDSO
	#COPY $SRC/vdso.lds.S					$DEST/arch/x86/entry/vdso/
	#COPY $SRC/vdsox32.lds.S				$DEST/arch/x86/entry/vdso/

	# Copy assembly build scripts for 32 bit VDSO (needed?)
	#COPY $SRC/vdso32.lds.S					$DEST/arch/x86/entry/vdso/vdso32/


# Universal radclock build without kernel support
# [ actually deb based, but since doesn't use kernel, works on arm as well ]
build-radclock-no-kernel:
	 # Get essential packages and kernel source
	 FROM +deb-kernel-deps

    # Switch our working directory back
    WORKDIR /RADclock
    # Copy everything for now
    COPY . ./
    # Generate version
    RUN ./version.sh

    # Configure the build
    RUN autoreconf -i
    RUN ./configure --prefix /radclock-build
    # Actually build it
    RUN make
    # Install it so we can grab it
    RUN make install
    # Pull the artifacts out
    SAVE ARTIFACT /radclock-build AS LOCAL ./artifacts/


deb-build-radclock-with-kernel:
	 # Get essential packages and kernel source
	 FROM +deb-kernel-deps
	 ARG osversion=$(pwd | tr '/' '\n' |tail -1 |cut -d'-' -f2) # find desired version
	 RUN echo "Will be compiling radclock for a $osversion AMD64 kernel"
	 # no need to copy FFfiles to compile radclock, configure.ac contains details

    # Switch our working directory back
    WORKDIR /RADclock
    # Copy everything for now
    COPY . ./
    # Generate version
    RUN ./version.sh

    # Configure the build using the matching FF files for deb
    RUN autoreconf -i
    RUN ./configure --prefix /radclock-build --with-FFclock-kernel=$osversion
    # Actually build it
    RUN make
    # Install it so we can grab it
    RUN make install
    # Pull the artifacts out
    ARG TARGETARCH
    SAVE ARTIFACT /radclock-build AS LOCAL ./artifacts/radclock-with-kernel/$TARGETARCH/




#### ARM specific targets ####

# TODO:  targets {deb,arm}-build-radclock-with-kernel   are virtually identical,
# combine them using a UDC and an Arch parameter

arm-build-radclock-with-kernel:
	 # Get essential packages and kernel source
    FROM +arm-kernel-deps
	 ARG osversion=4.19.127 		# specify desired version
	 RUN echo "Will be compiling radclock for a $osversion ARM64 kernel"
	 # no need to copy FFfiles to compile radclock, configure.ac contains details

  	 # Switch our working directory back
    WORKDIR /RADclock
    # Copy everything for now
    COPY . ./
    # Generate version
    RUN ./version.sh

    # Configure build for a specified FF kernel version for the Pi
    RUN autoreconf -i
    RUN ./configure --prefix /radclock-build --with-FFclock-kernel=$osversion
    # Actually build it
    RUN make
    # Install it so we can grab it
    RUN make install
    # Pull the artifacts out
    ARG TARGETARCH
    SAVE ARTIFACT /radclock-build AS LOCAL ./artifacts/radclock-with-kernel/$TARGETARCH/


arm-crossbuild64-radclock-with-kernel:
   BUILD --platform=linux/arm64 +arm-build-radclock-with-kernel

# Original attempt: it compiles (takes 2-3hrs), but Peter says it doesn't work on the Pi
#build-arm64:
#    BUILD --platform=linux/arm64 +kernel-build-patched


arm-kernel-deps:
    FROM +base-deps
	 # Install build dependencies for the ARM kernel
    RUN apt-get -yqq install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu make wget xz-utils flex bison libssl-dev bc file rsync kmod cpio
    # Pull Linux arm source
	 GIT CLONE --branch rpi-4.19.y https://github.com/raspberrypi/linux.git /linux
    WORKDIR /linux

arm-kernel-config-patched:
	 # Get essential packages and kernel source
    FROM +arm-kernel-deps
	 # Impose FFfile changes onto source
    DO +COPY_IN_FFFILES --SRC=kernel/rpi/4.19.127/CurrentSource --ARM=YES
	 # Build default config
    RUN make ARCH=arm64 bcm2711_defconfig

    # Remove all the debug stuff
    RUN scripts/config --disable DEBUG_INFO
    # Don't compress the kernel
    RUN sed -i '/KBUILD_IMAGE/ s/.gz//' arch/arm64/Makefile

arm-kernel-build-patched:
    FROM +arm-kernel-config-patched
    # Build the cross-compiled kernel
    RUN ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- make bindeb-pkg   # deb-pkg made same 3 pkgs, no source!
    # Export the packages
    SAVE ARTIFACT ../*.deb AS LOCAL artifacts/kernel-patched/arm64/




#### Misc targets below this point ####


# Collect all files in source that may have FF content  (is the inverse of COPY_IN_FFFILES)
COLLECT_FFFILES:
	COMMAND
	ARG SUBDIR="unknown"
	ARG DEST="./artifacts/kernelsourcefiles/$SUBDIR/"

	# If want entire source
#	SAVE ARTIFACT .							AS LOCAL $DEST/"linux-source"

	#RUN ls -altr
	SAVE ARTIFACT net/ipv4/af_inet.c					AS LOCAL $DEST
	SAVE ARTIFACT net/packet/af_packet.c				AS LOCAL $DEST
#	SAVE ARTIFACT include/uapi/asm-generic/sockios.h	AS LOCAL $DEST"asm-generic_sockios.h"
	SAVE ARTIFACT net/core/dev.c						AS LOCAL $DEST
	SAVE ARTIFACT drivers/Kconfig						AS LOCAL $DEST"drivers_Kconfig"
	SAVE ARTIFACT drivers/Makefile						AS LOCAL $DEST"drivers_Makefile"

	# Not present in non-FF source
	SAVE ARTIFACT --if-exists  drivers/ffclock			AS LOCAL $DEST
	SAVE ARTIFACT --if-exists  include/linux/ffclock.h	AS LOCAL $DEST

	SAVE ARTIFACT net/core/skbuff.c						AS LOCAL $DEST
	SAVE ARTIFACT include/linux/skbuff.h				AS LOCAL $DEST
	SAVE ARTIFACT include/net/sock.h					AS LOCAL $DEST
	SAVE ARTIFACT net/core/sock.c						AS LOCAL $DEST
	SAVE ARTIFACT net/socket.c							AS LOCAL $DEST
	SAVE ARTIFACT include/uapi/linux/sockios.h			AS LOCAL $DEST
	SAVE ARTIFACT include/linux/syscalls.h				AS LOCAL $DEST
	SAVE ARTIFACT kernel/time/time.c					AS LOCAL $DEST
	SAVE ARTIFACT kernel/time/timekeeping.c				AS LOCAL $DEST

	# VSDO files  ** Giving up on VSDO for now, complicated and not essential **
	# SAVE ARTIFACT arch/x86/entry/vdso/vclock_gettime.c	AS LOCAL $DEST
	# SAVE ARTIFACT arch/x86/include/asm/vgtod.h			AS LOCAL $DEST
	# Dropping additional files for VDSO attempt for now, find them in 4.19.235/CurrentSource

	# AMD specific files
	SAVE ARTIFACT arch/x86/entry/syscalls/syscall_*.tbl	AS LOCAL $DEST

	# ARM specific files
	SAVE ARTIFACT include/uapi/asm-generic/unistd.h		AS LOCAL $DEST



# Collect selected kernel source file(s) from kernel being pulled in
extract-source-rpi:
	# Pull RPi source  [ last "4.19" commit, now very stable, call it "Linux" ]
	ARG commit="rpi-4.19.y"
	GIT CLONE --branch $commit https://github.com/raspberrypi/linux.git linux

	# Find out what we have
	#RUN --no-cache pwd; ls -altr;

	# Extract desired files (need one line per wildcard-able file-set)
	WORKDIR linux
	RUN --no-cache pwd

	# Collect all files in source that may have FF content
	DO +COLLECT_FFFILES --SUBDIR=$commit



extract-source-deb:
	# Add Debians sources
	RUN echo "deb-src http://deb.debian.org/debian $DIST main" > /etc/apt/sources.list.d/main.list
	RUN apt-get update
	# Install the tools we need to extract the source
	RUN apt-get -yqq install build-essential
	# Pull Linux source  [ dir created will be of most recent kernel ]
	RUN apt-get -yqq source linux

	# Find out what we have
	#RUN --no-cache pwd; ls -altr
	ARG osversion=$(ls -tr |tail -1 |cut -d'-' -f2)
	ARG commit="deb-$osversion"

	# Extract desired files (need one line per wildcard-able file-set)
	WORKDIR linux-$osversion
	RUN --no-cache pwd
	#RUN ls -altr

	# Collect all files in source that may have FF content
	DO +COLLECT_FFFILES --SUBDIR=$commit



# Tests of various kinds
test-basic:
	RUN echo "I am currently running as $USER on $(hostname) under $(pwd)"

	ARG DIST
	RUN echo "Base distribution set to $DIST"
	RUN echo "deb-src http://deb.debian.org/debian $DIST main" > /etc/apt/sources.list.d/main.list
	RUN cat /etc/apt/sources.list.d/*.list
	RUN cat /etc/apt/sources.list

#	WORKDIR NewTest
#	RUN pwd
#	ARG BASICVAR="myvar"
#	RUN echo $BASICVAR
#	ARG TARGETARCH
#	IF [ "$TARGETARCH" = "amd64" ]
#		RUN echo "Found architecture to be amd64"
#		ARG SRC=kernel/linux/4.19.0/CurrentSource
#		ARG DEST=linux-4.19.235
#	ELSE
#		RUN echo "Assuming architecture is arm64 (actually $TARGETARCH)"
#		ARG SRC=kernel/rpi/4.19.127/CurrentSource
#		ARG DEST=/linux
#	END
#	RUN echo "SRC set to $SRC;  DEST set to $DEST"
#	WORKDIR $DEST

# Testing persistance of action defined/performed in test-basic
test-persistance:
	FROM +test-basic						# embedded comments work
	# Things that are persistent
	RUN pwd									# WORKINGDIR
	# Things that are not
	RUN echo $BASICVAR
	# Extract osversion from current dir name
	ARG osversion=$(pwd | tr '/' '\n' |tail -1 |cut -d'-' -f2)
	RUN echo $osversion

# Test the UDC
test-UDC:
	DO +UDC_ARG_TEST
	DO +UDC_ARG_TEST --SRC="supplied src"
	DO +UDC_ARG_TEST --SRC="supplied src" --ARM=YES

# Test argument passing to a UDC
UDC_ARG_TEST:
	 COMMAND
	 ARG SRC
	 ARG DEST="."
	 ARG ARM="NO"

	 RUN echo " Passed args are:  SRC: $SRC,  DEST: $DEST,  ARM: $ARM"
	IF [ "$ARM" = "YES" ]
	 	RUN echo "ARM is YES"
	 ELSE
	 	RUN echo "ARM is not YES"
	 END

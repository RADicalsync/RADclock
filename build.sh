bash version.sh
export CC=/usr/bin/clang
chmod 774 ./*
autoreconf -if
./configure
make
sudo make install

#
sudo radclock
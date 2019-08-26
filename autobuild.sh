#Small script to build and deploy the lib on debian
./autogen.sh
make
sudo cp src/.libs/libgstjdspfx.so /usr/lib/x86_64-linux-gnu/gstreamer-1.0/
make clean
rm -rf src/.deps
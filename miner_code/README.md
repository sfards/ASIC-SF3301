This is a multi-threaded miner for SFARDS miner device,
fork of Pooler's cpuminer

Dependencies:

        libcurl                 http://curl.haxx.se/libcurl/

        jansson                 http://www.digip.org/jansson/

Build intstructions(Linux only):

./configure

make

Usage instructions:  Run "minerd --help" to see options.

Example:

BTC:

sudo ./minerd -a sha256d -o stratum+tcp://POOL_URL -u USER -p PASSWORD -d /dev/ttyUSBx -f 600

LTC

sudo ./minerd  -o stratum+tcp://POOL_URL -u USER -p PASSWORD -d /dev/ttyUSBx -f 600

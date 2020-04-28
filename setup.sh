
export DEBIAN_FRONTEND=noninteractive

sudo apt-get update 
[[ "$SYSTEM_UPGRADE" == "yes" ]] && sudo apt-get upgrade -yq 
sudo apt-get install -yq apt-utils\
                        binutils \
                        cmake curl \
                        g++-8 git \
                        patch \
                        sudo \
                        tar time tzdata \
                        unzip
                        
sudo apt-get purge -y gcc g++ 
sudo apt-get autoremove -y --purge 

sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 10 
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-8 10 
sudo update-alternatives --install /usr/bin/gcov gcov /usr/bin/gcov-8 10 

git clone https://github.com/Microsoft/vcpkg.git

cd vcpkg

./bootstrap-vcpkg.sh
./vcpkg integrate install
./vcpkg install nlohmann-json docopt spdlog
./vcpkg install boost-serialization boost-flyweight boost-dynamic-bitset boost-graph  
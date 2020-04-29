export DEBIAN_FRONTEND=noninteractive

if [ "$INSIDE_DOCKER" == "yes" ]; then
  su root
  apt-get install sudo
fi

sudo apt-get update
[[ "$INSIDE_DOCKER" == "yes" ]] && sudo apt-get upgrade -yq
sudo apt-get install -yq apt-utils\
                        binutils \
                        cmake curl \
                        g++-8 git \
                        patch \
                        sudo \
                        tar time tzdata \
                        unzip

sudo apt-get purge -y gcc g++
[[ "$INSIDE_DOCKER" == "yes" ]] && sudo apt-get autoremove -y --purge

sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 10
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-8 10
sudo update-alternatives --install /usr/bin/gcov gcov /usr/bin/gcov-8 10

OLD_PWD="`pwd`"
cd $HOME

git clone https://github.com/Microsoft/vcpkg.git

cd vcpkg

./bootstrap-vcpkg.sh
./vcpkg integrate install
./vcpkg install nlohmann-json docopt spdlog
./vcpkg install boost-serialization boost-flyweight boost-dynamic-bitset boost-graph

echo "Environment:

cmake: `cmake --version`

g++: `g++ --version`

gcc: `gcc --version`

gcov: `gcov --version`

vcpkg: `./vcpkg list`
"

if [ "$INSIDE_DOCKER" == "yes" ]; then
  git clone --recurse-submodules https://github.com/dns-groot/groot.git
  cd groot
else
  cd $OLD_PWD
fi
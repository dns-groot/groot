export DEBIAN_FRONTEND=noninteractive

sudo apt-get update
[[ "$INSIDE_DOCKER" == "yes" ]] && sudo apt-get upgrade -yq
sudo apt-get install -yq apt-utils\
                        binutils \
                        cmake curl \
                        g++ git \
                        patch \
                        sudo \
                        tar time \
                        unzip

OLD_PWD="`pwd`"

# install lcov and its dependecies if it is not for docker
if [ "$INSIDE_DOCKER" != "yes" ]; then
  sudo apt-get install build-essential libz-dev
  sudo cpan PerlIO::gzip
  sudo cpan JSON
  cd $HOME
  git clone https://github.com/linux-test-project/lcov.git
  cd lcov
  sudo make install
fi

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
  cd $HOME
  git clone --recurse-submodules https://github.com/dns-groot/groot.git
  cd groot
else
  cd $OLD_PWD
fi
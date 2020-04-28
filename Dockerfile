FROM ubuntu:18.04

LABEL maintainer="sivakesava@cs.ucla.edu"

ENV HOME /home/groot

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get upgrade -yq && \
    apt-get install -yq apt-utils\
                        binutils \
                        cmake curl \
                        g++-8 git \
                        patch \
                        sudo \
                        tar time tzdata \
                        unzip \
                        && \
    apt-get purge -y gcc g++ && \
    apt-get autoremove -y --purge

RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 10 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-8 10 && \
    update-alternatives --install /usr/bin/gcov gcov /usr/bin/gcov-8 10

RUN adduser --disabled-password --home $HOME --shell /bin/bash --gecos '' groot && \
    echo 'groot ALL=(ALL) NOPASSWD:ALL' >>/etc/sudoers && \
    su groot

USER groot
WORKDIR $HOME

RUN git clone --recurse-submodules https://github.com/dns-groot/groot.git
RUN git clone https://github.com/Microsoft/vcpkg.git

WORKDIR $HOME/vcpkg
RUN ./bootstrap-vcpkg.sh
RUN ./vcpkg integrate install
RUN ./vcpkg install nlohmann-json docopt spdlog
RUN ./vcpkg install boost-serialization boost-flyweight boost-dynamic-bitset boost-graph  


WORKDIR $HOME/groot

RUN mkdir build && \ 
    cd build && \
    cmake -DCODE_COVERAGE=ON -BUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug .. && \
    cmake --build . --config Debug && \
    ctest --output-on-failure && \
    bash <(curl -s https://codecov.io/bash)
    
RUN cd build && \
    rm -rf * && \
    cmake .. && \
    cmake --build .

# RUN ./build/test/tester --log_level=test_suite

WORKDIR $HOME/groot

CMD [ "bash" ]

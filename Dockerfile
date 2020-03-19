FROM ubuntu:18.04

LABEL maintainer="sivakesava@cs.ucla.edu"

ENV HOME /home/groot

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get upgrade -yq && \
    apt-get install -yq binutils \
                        cmake curl \
                        g++-8 git \
                        patch python3 \
                        sudo \
                        tar time tzdata \
                        unzip \
                        && \
    apt-get purge -y gcc g++ && \
    apt-get autoremove -y --purge

RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 10 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-8 10

RUN adduser --disabled-password --home $HOME --shell /bin/bash --gecos '' groot && \
    echo 'groot ALL=(ALL) NOPASSWD:ALL' >>/etc/sudoers && \
    su groot

USER groot
WORKDIR $HOME

RUN git clone  https://github.com/dns-groot/groot.git
RUN git clone https://github.com/Microsoft/vcpkg.git

WORKDIR $HOME/vcpkg
RUN ./bootstrap-vcpkg.sh
RUN ./vcpkg integrate install
RUN ./vcpkg install nlohmann-json docopt spdlog
RUN ./vcpkg install boost-serialization boost-flyweight boost-dynamic-bitset boost-graph  


WORKDIR $HOME/groot

RUN mkdir build && \ 
    cd build && \
    cmake .. && \
    make

CMD [ "bash" ]

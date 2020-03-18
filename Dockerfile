FROM ubuntu:18.04

LABEL maintainer="sivakesava@cs.ucla.edu"

ENV HOME /home/groot

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get upgrade -yq && \
    apt-get install -yq binutils \
                        cmake curl \
                        g++ git \
                        patch python \
                        sudo \
                        tar time tzdata \
                        unzip \
                        && \
    apt-get autoremove -y --purge


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
RUN ./vcpkg install boost nlohmann-json docopt spdlog

WORKDIR $HOME/groot
RUN git checkout EC-generation

RUN mkdir build

CMD [ "bash" ]


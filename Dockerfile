FROM ubuntu:18.04

LABEL maintainer="sivakesava@cs.ucla.edu"

ENV HOME /home/groot

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get upgrade -yq && \
    apt-get install -yq binutils \
                        libboost-dev\
                        cmake curl \
                        g++ git \
                        patch \
                        sudo \
                        time tzdata \
                        unzip \
                        && \
    apt-get autoremove -y --purge


RUN adduser --disabled-password --home $HOME --shell /bin/bash --gecos '' groot && \
    echo 'groot ALL=(ALL) NOPASSWD:ALL' >>/etc/sudoers && \
    su groot

USER groot
WORKDIR $HOME

ENV LC_CTYPE=C.UTF-8

RUN git clone --recurse-submodules https://github.com/dns-groot/groot.git

WORKDIR $HOME/groot

RUN git checkout EC-generation

CMD [ "bash" ]

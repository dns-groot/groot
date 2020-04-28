FROM ubuntu:18.04

LABEL maintainer="sivakesava@cs.ucla.edu"

ENV HOME /home/groot
ENV INSIDE_DOCKER="yes"

RUN adduser --disabled-password --home $HOME --shell /bin/bash --gecos '' groot && \
    echo 'groot ALL=(ALL) NOPASSWD:ALL' >>/etc/sudoers && \
    su groot

USER groot
WORKDIR $HOME

COPY setup.sh setup.sh
RUN bash setup.sh

WORKDIR $HOME/groot

RUN mkdir build &&\
    cd build && \
    rm -rf * && \
    cmake .. && \
    cmake --build .

CMD [ "bash" ]

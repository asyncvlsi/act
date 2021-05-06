#
# Usage: docker build . -t act
# To run: docker run -it -v`pwd`:/work act
#

FROM ubuntu:20.04

# Install tools needed for development
RUN apt update && \
    apt upgrade --yes && \
    apt install --yes build-essential m4 libedit-dev zlib1g-dev

ENV VLSI_TOOLS_SRC=/work
ENV ACT_HOME=/usr/local/cad
ENV PATH=$PATH:$ACT_HOME/bin

WORKDIR $VLSI_TOOLS_SRC
COPY . $VLSI_TOOLS_SRC
RUN mkdir -p $ACT_HOME && \
    ./configure $ACT_HOME && \
    ./build && \
    make install
    
# Cleanup all we can to keep container as lean as possible
RUN apt remove --yes build-essential libedit-dev m4 zlib1g-dev && \
    apt autoremove --yes && \
    rm -rf /work /tmp/* /var/lib/apt/lists/*

CMD ["/bin/bash"]

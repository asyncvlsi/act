#
# Usage: docker build . -t act
# To run (X11 support using tclkit on port 8015->now 443): 
#  docker run -it -v`pwd`:/work -p443:443/tcp -e DISPLAY="$DISPLAY" -v /tmp/.X11-unix:/tmp/.X11-unix act
#
# Note: X11 support broken on Ubuntu when docker installed using 'snap'
#
# Use minimal Ubuntu 18.04? Not much better
FROM ubuntu:20.04 AS build

ENV VLSI_TOOLS_SRC=/work
ENV ACT_HOME=/usr/local/cad
ENV PATH=$PATH:$ACT_HOME/bin
ENV DEBIAN_FRONTEND=noninteractive

# Install tools needed for development
RUN apt-get update && \
    apt-get upgrade --yes && \
    apt-get install -y --no-install-recommends tzdata && \
    apt-get install -y build-essential m4 libedit-dev zlib1g-dev libboost-dev tcl-dev tk-dev git curl autoconf vim \
      tigervnc-standalone-server tigervnc-xorg-extension tigervnc-viewer matchbox-window-manager libxss1 

WORKDIR $VLSI_TOOLS_SRC
COPY . $VLSI_TOOLS_SRC
RUN mkdir -p $ACT_HOME && \
    ./configure $ACT_HOME && \
    ./build && \
    make install

# git clone https://github.com/asyncvlsi/dflowmap.git && \ -> add dflowmap

RUN git clone https://github.com/asyncvlsi/interact.git && \
    git clone --branch sdtcore https://github.com/asyncvlsi/chp2prs.git && \
    git clone https://github.com/asyncvlsi/irsim.git && \
    for p in interact chp2prs irsim; do \
      cd $p && (./configure || true) && make depend && make && make install && cd .. \
    ; done && echo "$ACT_HOME/lib" > /etc/ld.so.conf.d/act.conf

# Build Tclkit
COPY web_irsim.sh $ACT_HOME/bin/
RUN curl http://kitcreator.rkeene.org/fossil/tarball/kitcreator-trunk-tip.tar.gz?uuid=trunk | tar xvz && \
    cd kitcreator-trunk-tip && build/pre.sh && \
    KITCREATOR_PKGS=" itcl mk4tcl tcllib tk " KC_TCL_STATICPKGS="1" ./kitcreator --enable-64bit --enable-threads && \
    cp `find . -name tclkit-*` $ACT_HOME/bin/tclkit && \
    curl http://cloudtk.tcl-lang.org/Downloads/CloudTk.kit -o $ACT_HOME/bin/CloudTk.kit
 
# Cleanup all we can to keep container as lean as possible
RUN apt remove --yes build-essential git autoconf && \
    apt autoremove --yes && \
    rm -rf /tmp/* /var/lib/apt/lists/* $VLSI_TOOLS_SRC

# This results in a single layer image
# FROM scratch
# COPY --from=build_with_env $ACT_HOME $ACT_HOME

# Expose Tclkit port
EXPOSE 8015/tcp
# EXPOSE 443/tcp
CMD ["/bin/bash"]

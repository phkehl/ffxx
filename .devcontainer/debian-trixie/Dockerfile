
# Debian 13 Trixie
FROM debian:trixie

# Set timezone, sane language and locale
ENV TZ=Europe/Zurich
ENV LANG=en_GB.UTF-8
ENV LANGUAGE=en_GB
ENV LC_ALL=C.UTF-8
RUN <<EOF
# locale
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get -y update
apt-get -y --with-new-pkgs upgrade
apt-get -y install locales sudo
apt-get clean
ln -snf /usr/share/zoneinfo/$TZ /etc/localtime
echo $TZ > /etc/timezone
sed -i '/en_GB.UTF-8/s/^# //g' /etc/locale.gen
locale-gen
EOF

COPY debian-trixie/debian.sources /etc/apt/sources.list.d/

RUN DEBIAN_FRONTEND=noninteractive apt-get -y update && \
    DEBIAN_FRONTEND=noninteractive apt-get -y --with-new-pkgs upgrade && \
    DEBIAN_FRONTEND=noninteractive apt-get -y install \
        ack \
        aptitude \
        bash-completion \
        bind9-dnsutils \
        bison \
        bsdmainutils \
        build-essential \
        can-utils \
        ccache \
        chrpath \
        clang \
        clang-format \
        clang-tidy \
        clang-tools \
        clangd \
        cmake \
        curl \
        dlocate \
        evtest \
        fakeroot \
        file \
        flex \
        flip \
        g++ \
        gawk \
        gcc \
        gcc-doc \
        gdb \
        git \
        git-lfs \
        gnupg2 \
        gnuplot \
        googletest \
        graphviz \
        htop \
        iproute2 \
        iputils-ping \
        jq \
        less \
        libbacktrace-dev \
        libboost-all-dev \
        libboost-doc \
        libclone-perl \
        libcurl4-openssl-dev \
        libeigen3-dev \
        libevdev-dev \
        libfreetype-dev \
        libgl-dev \
        libglfw3-dev \
        libglm-dev \
        libglu1-mesa-dev \
        libgpiod-dev \
        libgtest-dev \
        libiio-dev \
        libjson-xs-perl \
        libmime-base64-perl \
        libpath-tiny-perl \
        libdata-float-perl \
        libproj-dev \
        libproj25 \
        libsqlite3-dev \
        libssl-dev \
        libssl-doc \
        libtiff-dev \
        libwayland-dev \
        libxrandr-dev \
        libyaml-cpp-dev \
        linux-libc-dev \
        linux-perf \
        liquidprompt \
        lsb-release \
        make \
        man \
        man-db \
        manpages \
        manpages-dev \
        moreutils \
        ncdu \
        net-tools \
        netbase \
        netcat-openbsd \
        nlohmann-json3-dev \
        openssh-client \
        pre-commit \
        proj-bin \
        proj-data \
        psmisc \
        pv \
        python-is-python3 \
        python3-osrf-pycommon \
        python3-pip \
        python3-venv \
        rsync \
        sl \
        socat \
        sqlite3 \
        strace \
        sudo \
        systemd-journal-remote \
        tcpdump \
        tig \
        unzip \
        valgrind \
        vim \
        wget \
        xauth \
        xxd \
        zip \
        zlib1g-dev \
        zlib1g-dev && \
    apt-get clean

RUN <<EOF
addgroup --gid 1000 ffxx
adduser --uid 1000 --gid 1000 ffxx
echo "ffxx ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers.d/ffxx
chmod 0440 /etc/sudoers.d/ffxx
EOF

# dummy
RUN DEBIAN_FRONTEND=noninteractive apt-get -y update && \
    DEBIAN_FRONTEND=noninteractive apt-get -y --with-new-pkgs upgrade && \
    DEBIAN_FRONTEND=noninteractive apt-get -y install \
        bash && \
    apt-get clean

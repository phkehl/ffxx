
# Debian 13 Trixie
FROM gcc:latest

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
    DEBIAN_FRONTEND=noninteractive apt-get -y --no-install-recommends install \
    libcurl4-gnutls-dev libglfw3-dev libfreetype-dev zlib1g-dev libglm-dev libeigen3-dev libgl-dev \
    libglu1-mesa-dev libboost-all-dev libyaml-cpp-dev linux-libc-dev libssl-dev libproj-dev proj-bin proj-data git \
    cmake make xxd nlohmann-json3-dev libxrandr-dev libwayland-dev libmime-base64-perl libpath-tiny-perl libdata-float-perl libbacktrace-dev \
    && \
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

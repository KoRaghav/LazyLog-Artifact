FROM ubuntu:22.04
ARG DEBIAN_FRONTEND=noninteractive

# Proxy Configuration
ENV container=docker
ARG http_proxy
ARG https_proxy
ARG HTTP_PROXY
ARG HTTPS_PROXY

# Install dependencies needed for eRPC, LazyLog, and SSH
RUN apt-get update && apt-get install -y \
    build-essential cmake git python3 python3-pip \
    libnuma-dev librdkafka-dev libboost-all-dev libssl-dev libcrypto++-dev \
    libgflags-dev libgtest-dev gcc-11 g++-11 \
    iproute2 net-tools iputils-ping sudo vim less \
    openssh-server pdsh \
    && rm -rf /var/lib/apt/lists/*
    
RUN ln -sf /usr/bin/python3 /usr/bin/python

# SSH Setup
RUN mkdir /var/run/sshd \
    && sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config \
    && mkdir -p /root/.ssh \
    && ssh-keygen -t rsa -f /root/.ssh/id_rsa -N "" \
    && cat /root/.ssh/id_rsa.pub > /root/.ssh/authorized_keys \
    && chmod 600 /root/.ssh/authorized_keys \
    && echo "Host *\n\tStrictHostKeyChecking no\n\tUserKnownHostsFile /dev/null\n" >> /root/.ssh/config

WORKDIR /opt

# Build eRPC with RAW transport (standard UDP)
RUN git clone https://github.com/erpc-io/eRPC && cd eRPC \
    && git checkout v0.1 \
    && cmake . -DPERF=ON -DTRANSPORT=raw -DLOG_LEVEL=info \
    && make -j$(nproc)

# Copy LazyLog code
COPY . /opt/LazyLog-Artifact
WORKDIR /opt/LazyLog-Artifact

# Configure and Build LazyLog
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCORFU=OFF -DSKIP_TESTS=ON \
    && cmake --build build -j$(nproc)

RUN mkdir -p /data

# Expose ports for eRPC and SSH
EXPOSE 22 31850-31870

# Default entrypoint
CMD ["/usr/sbin/sshd", "-D"]


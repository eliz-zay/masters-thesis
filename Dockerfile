FROM ubuntu:22.04

RUN apt update && apt install -y \
    wget \
    cmake \
    build-essential \
    python3 \
    python3-pip \
    git \
    clang \
    lld \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu \
    && rm -rf /var/lib/apt/lists/*

ENV LLVM_DIR=/usr/local/llvm
ENV PATH="${LLVM_DIR}/bin:${PATH}"
ENV LD_LIBRARY_PATH="${LLVM_DIR}/lib:${LD_LIBRARY_PATH}"

WORKDIR /opt
RUN git clone --depth=1 --branch release/20.x https://github.com/llvm/llvm-project.git && \
    mkdir llvm-project/build

# Build LLVM
WORKDIR /opt/llvm-project/build
RUN cmake \
  -G 'Unix Makefiles' \
  -DLLVM_ENABLE_PROJECTS="clang;lld" \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_DEFAULT_TARGET_TRIPLE='aarch64-linux-gnu' \
  -DLLVM_TARGETS_TO_BUILD="AArch64;X86" \
  ../llvm
RUN make

# Install zig
ENV ZIG_VERSION=0.14.0
ENV ZIG_DIR=/opt/zig

RUN apt-get update && apt-get install -y curl

RUN curl -LO https://ziglang.org/download/${ZIG_VERSION}/zig-linux-x86_64-${ZIG_VERSION}.tar.xz && \
  tar -xf zig-linux-x86_64-${ZIG_VERSION}.tar.xz && \
  mv zig-linux-x86_64-${ZIG_VERSION} ${ZIG_DIR} && \
  ln -s ${ZIG_DIR}/zig /usr/local/bin/zig && \
  rm zig-linux-x86_64-${ZIG_VERSION}.tar.xz

WORKDIR /app
COPY . /app

# Build passes
WORKDIR /app/pass
RUN mkdir build && \
  cd build && \
  LLVM_HOME=/opt/llvm-project/build cmake .. && \
  make

WORKDIR /app/docker

# Make scripts executable
RUN chmod a+x *.sh

ENTRYPOINT ["/app/docker/run.sh"]

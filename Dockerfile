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
  -DLLVM_TARGETS_TO_BUILD=AArch64 \
  ../llvm
RUN make

WORKDIR /app
COPY . /app

# Build passes
WORKDIR /app/pass
RUN mkdir build && \
  cd build && \
  LLVM_HOME=/opt/llvm-project/build cmake .. && \
  make

# Make scripts executable
WORKDIR /app/target
RUN chmod a+x *.sh

WORKDIR /app

CMD ["/bin/bash"]

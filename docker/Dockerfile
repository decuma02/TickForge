# ==============================================================================
# Production Dockerfile for C++17 Development, Diagnostics & Profiling
# ==============================================================================
FROM ubuntu:22.04

# Prevent interactive tzdata prompts during installation
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

# Install core build tools, compilers (GCC-12 & Clang-15), GDB, and profiling tools
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    gcc-12 \
    g++-12 \
    clang-15 \
    clang-tools-15 \
    lld-15 \
    cmake \
    ninja-build \
    make \
    gdb \
    valgrind \
    git \
    curl \
    wget \
    ca-certificates \
    pkg-config \
    # Linux perf tools and dependencies for hardware event counters
    linux-tools-common \
    linux-tools-generic \
    kmod \
    binutils-dev \
    libdw-dev \
    libelf-dev \
    libunwind-dev \
    # Python & utilities
    python3 \
    python3-pip \
    python3-dev \
    sudo \
    tree \
    && rm -rf /var/lib/apt/lists/*

# Set GCC-12 / G++-12 as default alternatives
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 100 \
    && update-alternatives --install /usr/bin/cc cc /usr/bin/gcc-12 100 \
    && update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-12 100

# Fix potential perf version mismatch: Ubuntu wrapper requires specific linux-tools-$(uname -r).
# Create a fallback wrapper to execute any available installed perf binary.
RUN set -eux; \
    PERF_BIN=$(find /usr/lib/linux-tools -type f -name perf 2>/dev/null | head -n 1 || true); \
    if [ -n "$PERF_BIN" ]; then \
        ln -sf "$PERF_BIN" /usr/local/bin/perf; \
    else \
        echo '#!/bin/sh' > /usr/local/bin/perf; \
        echo 'PERF_EXEC=$(find /usr/lib/linux-tools -type f -name perf 2>/dev/null | head -n 1)' >> /usr/local/bin/perf; \
        echo 'if [ -n "$PERF_EXEC" ]; then exec "$PERF_EXEC" "$@"; else exec /usr/bin/perf "$@"; fi' >> /usr/local/bin/perf; \
        chmod +x /usr/local/bin/perf; \
    fi

# ==============================================================================
# Build & Install Google Test (Release 1.14.0)
# ==============================================================================
WORKDIR /tmp/gtest_build
RUN git clone --depth 1 --branch v1.14.0 https://github.com/google/googletest.git . \
    && cmake -B build -S . \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DBUILD_GMOCK=ON \
        -DBUILD_GTEST=ON \
        -Dgtest_force_shared_crt=ON \
    && cmake --build build --target install -j$(nproc) \
    && rm -rf /tmp/gtest_build

# ==============================================================================
# Build & Install Google Benchmark (Release v1.8.3)
# ==============================================================================
WORKDIR /tmp/benchmark_build
RUN git clone --depth 1 --branch v1.8.3 https://github.com/google/benchmark.git . \
    && cmake -B build -S . \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DBENCHMARK_ENABLE_TESTING=OFF \
        -DBENCHMARK_ENABLE_GTEST_TESTS=OFF \
        -DBENCHMARK_ENABLE_INSTALL=ON \
    && cmake --build build --target install -j$(nproc) \
    && rm -rf /tmp/benchmark_build

# ==============================================================================
# Create Non-Root Developer User (UID/GID configurable at build time)
# ==============================================================================
ARG USERNAME=developer
ARG USER_UID=1000
ARG USER_GID=1000

RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m -s /bin/bash $USERNAME \
    && echo "$USERNAME ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

# Set default working directory
WORKDIR /workspace

# Switch to non-root developer user
USER $USERNAME

# Default shell
CMD ["/bin/bash"]

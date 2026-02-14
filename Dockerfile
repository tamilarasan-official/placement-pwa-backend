# Stage 1: Build
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Install build tools and dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    zip \
    unzip \
    tar \
    pkg-config \
    ninja-build \
    libssl-dev \
    uuid-dev \
    libjsoncpp-dev \
    zlib1g-dev \
    libc-ares-dev \
    linux-libc-dev \
    && rm -rf /var/lib/apt/lists/*

# Install vcpkg
RUN git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg && \
    /opt/vcpkg/bootstrap-vcpkg.sh

ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH="${VCPKG_ROOT}:${PATH}"

WORKDIR /app
COPY vcpkg.json CMakeLists.txt ./
COPY controllers/ controllers/
COPY services/ services/
COPY utils/ utils/
COPY middleware/ middleware/
COPY main.cc .

# Build with vcpkg toolchain (static linking)
RUN cmake -B build \
    -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -G Ninja && \
    cmake --build build --config Release

# Stage 2: Runtime (slim)
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the statically-linked binary
COPY --from=builder /app/build/placement-backend .

# Create directories for uploads and logs
RUN mkdir -p uploads logs

EXPOSE 8080

ENV PORT=8080

CMD ["./placement-backend"]

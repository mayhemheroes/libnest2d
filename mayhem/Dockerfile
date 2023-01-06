# Build Stage
FROM --platform=linux/amd64 ubuntu:22.04 as builder

## Install build dependencies.
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y clang cmake libboost-dev libpolyclipping-dev libnlopt-cxx-dev

## Add source code to the build stage.
ADD . /libnest2d
WORKDIR /libnest2d

## Build
RUN cmake -DLIBNEST2D_HEADER_ONLY=OFF -DMAYHEM=1
RUN make -j$(nproc)

# Package Stage
FROM --platform=linux/amd64 ubuntu:22.04
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y libpolyclipping22 libnlopt0 libc6-i386 \
    libc6 libgcc-s1 lib32gcc-s1 libc6 libc6-i386
COPY --from=builder /libnest2d/mayhem/fuzz_libnest /fuzz_libnest
## Set up fuzzing!
ENTRYPOINT []
CMD /fuzz_libnest

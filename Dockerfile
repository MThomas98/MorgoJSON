FROM ubuntu:24.04

RUN apt-get update && apt-get install -y cmake ninja-build g++-13 ca-certificates git

WORKDIR /src

COPY . .

RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++-13
RUN cmake --build build

CMD ["ctest", "--test-dir", "build", "--output-on-failure"]
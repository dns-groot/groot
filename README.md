Groot
<a href="https://microbadger.com/images/sivakesava/groot"><img align="right" src="https://img.shields.io/microbadger/image-size/sivakesava/groot.svg?style=flat&label=docker"></img></a>
==========

[![](https://img.shields.io/docker/cloud/build/sivakesava/groot.svg?logo=docker&style=popout&label=Docker+Image)][docker-hub]

Groot is a static verification tool for DNS. Groot consumes a collection of zone files along with a collection of user-defined properties and systematically checks if any input to DNS can lead to violation of the properties.

## Installation

### Using `docker` (recommended)

_**Note:** The docker image may consume  ~&hairsp;5&hairsp;GB of disk space._

We recommend running Groot within a docker container,
since they have negligible performance overhead.
(See [this report](http://domino.research.ibm.com/library/cyberdig.nsf/papers/0929052195DD819C85257D2300681E7B/$File/rc25482.pdf))

0. [Get `docker` for your OS](https://docs.docker.com/install).
1. Pull our docker image<sup>[#](#note_1)</sup>: `docker pull sivakesava/groot`.
2. Run a container over the image: `docker run -it sivakesava/groot`.<br>
   This would give you a `bash` shell within LoopInvGen directory.

<a name="note_1"><sup>#</sup></a> Alternatively, you could also build the Docker image locally:

```bash
docker build -t sivakesava/groot github.com/dns-groot/groot
```
Docker containers are isolated from the host system.
Therefore, to run Groot on zones files residing on the host system,
you must first [bind mount] them while running the container:

```bash
docker run -v /host/dir:/home/groot/groot/shared -it sivakesava/groot
```

The `/host/dir` on the host system would then be accessible within the container at `~/groot/shared` (with read+write permissions). The executable would be located at `~/groot/build/bin/`.

### Manual Installation

<details>

<summary><kbd>:arrow_down: CLICK</kbd>to reveal instructions</summary>

#### Installation for Windows
1. Install [`vcpkg`](https://docs.microsoft.com/en-us/cpp/build/vcpkg?view=vs-2019) package manager to install dependecies. 
2. Install the C++ libraries (64 bit versions) using:
    - vcpkg install boost:x64-windows docopt:x64-windows nlohmann-json:x64-windows spdlog:x64-windows
    - vcpkg integrate install 
3. Clone the repository and open the solution (groot.sln) using Visual studio. Set the platform to x64 and mode to Release.
4. Configure the project properties to use ISO C++17 Standard (std:c++17) for C++ language standard.
5. Build the project using visual studio to generate the executable. The executable would be located at `~\groot\x64\Release\`.

#### Installation for Ubuntu 18.04 or later
1. Follow the instructions mentioned in the `DockerFile` to natively install in Ubuntu 18.04 or later.
2. The executable would be located at `~/groot/build/bin/`.

</details>

## Running
1. Usage: groot [-hdv] [--properties=<properties_file>] <zone_directory> [--output=<output_directory>]
    - -h --help Show the help screen
    - -v --verbose Logs trace information. 
    - --version Show the groot version
2. Example usage in docker (Ubuntu):
```bash
$ .~/groot/build/bin/groot ~/groot/demo/zone_files --properties=~/groot/demo/properties.json 
```
3. Example usage in Windows:
```bash
$ .~\groot\x64\Release\groot.exe ~\groot\demo\zone_files --properties=~\groot\demo\properties.json 
```
4.  Groot generates a `output.json` file containing the inputs that violates the properties.

[docker-hub]:         https://hub.docker.com/r/sivakesava/groot
[bind mount]:         https://docs.docker.com/storage/bind-mounts

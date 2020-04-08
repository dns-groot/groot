Groot
<a href="https://microbadger.com/images/sivakesava/groot"><img align="right" src="https://img.shields.io/microbadger/image-size/sivakesava/groot.svg?style=flat&label=docker"></img></a>
==========
<!---
[![](https://img.shields.io/docker/cloud/build/sivakesava/groot.svg?logo=docker&style=popout&label=Docker+Image)][docker-hub]
[![](https://github.com/dns-groot/groot/workflows/Docker%20Image%20CI/badge.svg?logo=docker&style=popout&label=Docker+Image)](https://github.com/dns-groot/groot/actions?query=workflow%3A%22Docker+Image+CI%22)
-->
[![](https://img.shields.io/github/workflow/status/dns-groot/groot/Docker%20Image%20CI/master?logo=docker&style=popout&label=Docker+Image)](https://github.com/dns-groot/groot/actions?query=workflow%3A%22Docker+Image+CI%22)


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

<summary><kbd>:arrow_down: CLICK</kbd> to reveal instructions</summary>

#### Installation for Windows
1. Install [`vcpkg`](https://docs.microsoft.com/en-us/cpp/build/vcpkg?view=vs-2019) package manager to install dependecies. 
2. Install the C++ libraries (64 bit versions) using:
    - vcpkg install boost:x64-windows docopt:x64-windows nlohmann-json:x64-windows spdlog:x64-windows
    - vcpkg integrate install 
3. Clone the repository (with  `--recurse-submodules`) and open the solution (groot.sln) using Visual studio. Set the platform to x64 and mode to Release.
4. Configure the project properties to use ISO C++17 Standard (std:c++17) for C++ language standard.
5. Build the project using visual studio to generate the executable. The executable would be located at `~\groot\x64\Release\`.

#### Installation for Ubuntu 18.04 or later
1. Follow the instructions mentioned in the `DockerFile` to natively install in Ubuntu 18.04 or later.
2. The executable would be located at `~/groot/build/bin/`.

</details>

## Property Verification
Check for any violations of the input properties by invoking Groot as:

For docker (Ubuntu):
```bash
$ .~/groot/build/bin/groot ~/groot/demo/zone_files --properties=~/groot/demo/properties.json --output=output.json
```
For Windows:
```bash
$ .~\groot\x64\Release\groot.exe ~\groot\demo\zone_files --properties=~\groot\demo\properties.json --output=output.json
```
Groot outputs any violations to the `output.json` file. 

#### Logging
Groot by default logs debugging messages to `log.txt` file and you may use `-v` flag to log more detailed information.

#### Packaging zone files data
Groot expects all the required zone files to be available in the input directory along with a special file `metadata.json`. The `metadata.json` file has to be created by the user and has to list the file name and the name server from which that zone file was obtained. If the zone files for a domain are obtained from multiple name servers, make sure to give the files a distinct name and fill the metadata accordingly. The user also has to provide the root (top) name servers for his domain in the `metadata.json`. 

<details>

<summary><kbd>:arrow_down: CLICK</kbd> to reveal <code>metadata.json</code> structure</summary>

```json5
{  
  "TopNameServers" : ["...", "...", ..],  //List of top name servers as strings
  "ZoneFiles" : [
      {
         "FileName": "...",
         "NameServer": "..."
      },
      {
         "FileName": "...",
         "NameServer": "..."
      },
      .
      .
  ]
}
```
</details>


[docker-hub]:         https://hub.docker.com/r/sivakesava/groot
[bind mount]:         https://docs.docker.com/storage/bind-mounts

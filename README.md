# groot
Static verifier for DNS

Groot is a static verification tool for DNS. Groot consumes a collection of zone files along with a collection of user-defined properties and systematically checks if any input to DNS can lead to a property violation for the properties.

## Installation for Windows
1. Install [`vcpkg`](https://docs.microsoft.com/en-us/cpp/build/vcpkg?view=vs-2019) package manager to install dependecies. 
2. Install the C++ libraries (64 bit versions) using:
    - vcpkg install boost:x64-windows
    - vcpkg install docopt:x64-windows
    - vcpkg install nlohmann-json:x64-windows
3. Clone the repository and open the solution (groot.sln) using Visual studio. Set the platform to x64.
4. Configure the project properties to use ISO C++17 Standard (std:c++17) for C++ language standard.

## Running
1. Build the project using visual studio.
2. Usage: groot [-hdv] [--properties=<properties_file>] <zone_directory>
    - -h --help Show the help screen
    - --version Show the groot version
    - -d --debug  Generate debugging dot files
    - -v  --verbose Print more information

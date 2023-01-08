GRoot 
==========
<!---
[![](https://img.shields.io/docker/cloud/build/sivakesava/groot.svg?logo=docker&style=popout&label=Docker+Image)][docker-hub]
[![](https://github.com/dns-groot/groot/workflows/Docker%20Image%20CI/badge.svg?logo=docker&style=popout&label=Docker+Image)](https://github.com/dns-groot/groot/actions?query=workflow%3A%22Docker+Image+CI%22)
-->
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg?style=popout)](https://opensource.org/licenses/MIT)
&nbsp;
[![](https://img.shields.io/github/actions/workflow/status/dns-groot/groot/dockerimage.yml?logo=docker&style=popout&label=docker+image)](https://github.com/dns-groot/groot/actions?query=workflow%3A%22Codecov+and+Docker+Image+CI%22)
&nbsp;
<a href='https://hub.docker.com/r/dnsgt/groot'><img src='https://img.shields.io/docker/image-size/dnsgt/groot/latest?logo=docker&logoWidth=24&style=popout'></img></a> 
&nbsp;
[![codecov](https://codecov.io/gh/dns-groot/groot/branch/master/graph/badge.svg?style=popout)](https://codecov.io/gh/dns-groot/groot)


GRoot is a static verification tool for DNS. GRoot consumes a collection of zone files along with a collection of user-defined properties and systematically checks if any input to DNS can lead to violation of the properties.

---

[**Installation**](#installation)
&nbsp; &nbsp; &vert; &nbsp; &nbsp;
[Property Verification](#property-verification)
&nbsp;&middot;&nbsp;
[Available Properties](#available-properties)
&nbsp; &nbsp; &vert; &nbsp; &nbsp;
[Citing GRoot](#citing-groot)
&nbsp;&middot;&nbsp;
[License (MIT)](LICENSE)

---

:page_with_curl: [SIGCOMM 2020](https://conferences.sigcomm.org/sigcomm/2020/) -- [GRoot: Proactive Verification of DNS Configurations](https://doi.org/10.1145/3387514.3405871)

:trophy: [Best Student Paper Award](http://www.sigcomm.org/awards/student-award-recipients)

:desktop_computer: [Slides and Talk](https://www.sivak.dev/projects/5-groot)


### Note 
We have updated the paper to handle empty non-terminals as per the RFCs properly. **[The updated paper is available here](https://sivakesava1.github.io/assets/pdf/sigcomm20_groot.pdf).** For more details, please check the GitHub issue [#11](https://github.com/dns-groot/groot/issues/11).

## Installation

### Using `docker` (recommended)

_**Note:** The docker image may consume  ~&hairsp;1.2&hairsp;GB of disk space._

We recommend running GRoot within a docker container,
since they have negligible performance overhead.
(See [this report](http://domino.research.ibm.com/library/cyberdig.nsf/papers/0929052195DD819C85257D2300681E7B/$File/rc25482.pdf))

0. [Get `docker` for your OS](https://docs.docker.com/install).
1. Pull our docker image<sup>[#](#note_1)</sup>: `docker pull dnsgt/groot`.
2. Run a container over the image: `docker run -it dnsgt/groot`.<br>
   This would give you a `bash` shell within groot directory.

<a name="note_1"><sup>#</sup></a> Alternatively, you could also build the Docker image locally:

```bash
docker build -t dnsgt/groot github.com/dns-groot/groot
```
Docker containers are isolated from the host system.
Therefore, to run GRoot on zones files residing on the host system,
you must first [bind mount] them while running the container:

```bash
docker run -v ~/data:/home/groot/groot/shared -it dnsgt/groot
```

The `~/data` on the host system would then be accessible within the container at `~/groot/shared` (with read+write permissions). The executable would be located at `~/groot/build/bin/`.

### Manual Installation

<details>

<summary><kbd>CLICK</kbd> to reveal instructions</summary>

#### Installation for Windows
1. Install [`vcpkg`](https://docs.microsoft.com/en-us/cpp/build/vcpkg?view=vs-2019) package manager to install dependecies. 
2. Install the C++ libraries (64 bit versions) using:
    - .\vcpkg.exe install boost-serialization:x64-windows boost-flyweight:x64-windows boost-dynamic-bitset:x64-windows boost-graph:x64-windows  boost-accumulators:x64-windows docopt:x64-windows nlohmann-json:x64-windows spdlog:x64-windows
    - .\vcpkg.exe integrate install 
3. Clone the repository (with  `--recurse-submodules`) and open the solution (groot.sln) using Visual studio. Set the platform to x64 and mode to Release.
4. Configure the project properties to use ISO C++17 Standard (std:c++17) for C++ language standard.
5. Set `groot` as `Set as Startup Project` using the solution explorer in the Visual Studio. Build the project using visual studio to generate the executable. The executable would be located at `~\groot\x64\Release\`.

#### Installation for Ubuntu 18.04 or later
1. Follow the instructions mentioned in the `DockerFile` to natively install in Ubuntu 18.04 or later.
2. The executable would be located at `~/groot/build/bin/`.

</details>

## Property Verification
Check for any violations of the input properties by invoking GRoot as:

For docker (Ubuntu):
```bash
~/groot$ ./build/bin/groot test/TestFiles/cc.il.us/zone_files --jobs=test/TestFiles/cc.il.us/jobs.json --output=output.json
```
For Windows:
```bash
~\groot> .\x64\Release\groot.exe test\TestFiles\cc.il.us\zone_files --jobs=test\TestFiles\cc.il.us\jobs.json --output=output.json
```
GRoot outputs any violations to the `output.json` file. 

### Flags
User can log debugging messages to `log.txt` using `-l` and use `-v` flag to log more detailed information. Use `-s` flag to display the statistics of the zone files parsed and the execution time. To log zone file issues (missing glue records, multiple CNAME/DNAME records, duplicate records) separately in `lint.json`, use the `--lint` flag. 

### Packaging zone files data
GRoot expects all the required zone files to be available in the input directory along with a special file `metadata.json`. The `metadata.json` file has to be created by the user and has to list the file name and the name server from which that zone file was obtained. If the zone files for a domain are obtained from multiple name servers, make sure to give the files a distinct name and fill the metadata accordingly. The user also has to provide the root (top) name servers for his domain in the `metadata.json`. 

<details>

<summary><kbd>CLICK</kbd> to reveal an <a href="https://github.com/dns-groot/groot/blob/master/test/TestFiles/cc.il.us/zone_files/metadata.json">example<code>metadata.json</code></a></summary>

```json5
{  
  "TopNameServers" : ["us.illinois.net."],  //List of top name servers as strings
  "ZoneFiles" : [
      {
         "FileName": "cc.il.us..txt", //cc.il.us. zone file from us.illinois.net. name server
         "NameServer": "us.illinois.net."
      },
      {
         "FileName": "richland.cc.il.us..txt", //richland.cc.il.us. zone file from ns1.richland.cc.il.us. name server
         "NameServer": "ns1.richland.cc.il.us.",
         "Origin": "richland.cc.il.us." // optional field to indicate the origin of the input zone file.
      },
      {
         "FileName": "child.richland.cc.il.us..txt", //child.richland.cc.il.us. zone file from ns1.child.richland.cc.il.us. name server
         "NameServer": "ns1.child.richland.cc.il.us."
      },
      {
         "FileName": "child.richland.cc.il.us.-2.txt", //child.richland.cc.il.us. zone file from ns2.child.richland.cc.il.us. name server 
         "NameServer": "ns2.child.richland.cc.il.us." //for same domain (child.richland.cc.il.us.) as the last one but from a different name server
      }
  ]
}
```
</details>

### Inputting Jobs
GRoot can currently verify properties shown below on the zone files and expects the input list in a `json` file format. A **job** verifies properties on a domain and optionally on all its subdomains. The input `json` file can have a list of jobs. GRoot verifies a default set of properties if no input file is provided.

<details>
<summary><kbd>CLICK</kbd> to reveal an <a href="https://github.com/dns-groot/groot/blob/master/test/TestFiles/cc.il.us/jobs.json">example job</a></summary>

```json5
[
   {
      "Domain": "cc.il.us." // Name of the domain to check
      "SubDomain": true, //Whether to check the properties on all the subdomains also
      "Properties":[ 
         {
            "PropertyName": "QueryRewrite",
            "Value": ["illinois.net." , "cc.il.us."]
         },
         {
            "PropertyName": "Rewrites",
            "Value": 1
         },
         {
            "PropertyName": "RewriteBlackholing"
         }
      ]
   }
]
```
</details>

#### Available Properties
<details>
<summary>Delegation Consistency</summary>
   
The parent and child zone files should have the same set of _NS_ and glue _A_ records for delegation.
Input `json` format:
```json5
      {
         "PropertyName": "DelegationConsistency"
      }
```
</details>

<details>
<summary>Finding all aliases</summary>
Lists all the input query names (aliases) that are eventually rewritten to one of the canonical names.   

Input `json` format:
```json5
      {
         "PropertyName": "AllAliases",
         "Value": ["gw1.richland.cc.il.us."] //List of canonical names
      }
```
</details>

<details>
<summary>Lame Delegation</summary>
   
A name server that is authoritative for a zone should provide authoritative answers, otherwise it is a lame delegation.
Input `json` format:
```json5
      {
         "PropertyName": "LameDelegation"
      }
```
</details>

<details>
<summary>Nameserver Contact</summary>
   
The query should not contact any name server that is not a subdomain of the allowed set of domains for any execution in the DNS.
Input `json` format:
```json5
      {
         "PropertyName": "NameserverContact",
         "Value": ["edu.", "net.", "cc.il.us."] //List of allowed domain suffixes
      }
```
</details>

<details>
<summary>Number of Hops</summary>
   
The query should not go through more than _X_ number of hops for any execution in the DNS.
Input `json` format:
```json5
      {
         "PropertyName": "Hops",
         "Value": 2
      }
```
</details>

<details>
<summary>Number of Rewrites</summary>
   
The query should not be rewritten more than _X_ number of time for any execution in the DNS.
Input `json` format:
```json5
      {
         "PropertyName": "Rewrites",
         "Value": 3
      }
```
</details>

<details>
<summary>Query Rewritting</summary>
   
The query should not be rewritten to any domain that is not a subdomain of the allowed set of domains for any execution in the DNS.
Input `json` format:
```json5
      {
         "PropertyName": "QueryRewrite",
         "Value": ["illinois.net." , "cc.il.us."] //List of allowed domain suffixes
      }
```
</details>

<details>
<summary>Response Consistency</summary>
Different executions in DNS that might happen due to multiple name servers should result in the same answers.
   
Input `json` format:
```json5
      {
         "PropertyName": "ResponseConsistency",
         "Types": ["A", "MX"] //Checks the consistency for only these types
      }
```
</details>

<details>
<summary>Response Returned</summary>
Different executions in DNS that might happen due to multiple name servers should result in some non-empty response.
   
Input `json` format:
```json5
      {
         "PropertyName": "ResponseReturned",
         "Types": ["CNAME", "A"] //Checks that some non-empty response is returned for these types
      }
```
</details>

<details>
<summary>Response Value</summary>
Every execution in DNS should return an answer that matches the user input answer.

Input `json` format:
```json5
      {
         "PropertyName": "ResponseValue",
         "Types": ["A"],
         "Value": ["64.107.104.4"] //The expected response
         
      }
```
</details>

<details>
<summary>Rewrite Blackholing</summary>
   
If the query is rewritten for any execution in the DNS, then the new query's domain name should have at least one resource record.

Input `json` format:
```json5
      {
         "PropertyName": "RewriteBlackholing"
      }
```
</details>

<details>
<summary>Structural Delegation Consistency</summary>
   
The parent and child zone files should have the same set of _NS_ and glue _A_ records for delegation irrespective of whether the name server hosting the child zone is reachable from the top name servers. 

Input `json` format:
```json5
      {
         "PropertyName": "StructuralDelegationConsistency"
      }
```
</details>

<details>
<summary>Zero Time To Live</summary>
   
The query should not return a resource record with zero TTL for the given types.  
Input `json` format:
```json5
      {
         "PropertyName": "ZeroTTL",
         "Types": ["A"]
      }
```
</details>

<details>
<summary>DNAME Substitution Check</summary>
   
The query should not  overflow the legal size for a domain name after DNAME rewrite. Records with CNAME target domain overflowing the legal size are ignored by the tool and are reported as issues during parsing itself.   
Input `json` format:
```json5
      {
         "PropertyName": "DNAMESubstitutionCheck"
      }
```
</details>

GRoot, by default, checks for cyclic zone dependency and other loops while verifying any of the above properties. 

## Citing GRoot

```
@inproceedings{10.1145/3387514.3405871,
    author = {Kakarla, Siva Kesava Reddy and Beckett, Ryan and Arzani, Behnaz and Millstein, Todd and Varghese, George},
    title = {GRoot: Proactive Verification of DNS Configurations},
    year = {2020},
    isbn = {9781450379557},
    publisher = {Association for Computing Machinery},
    address = {New York, NY, USA},
    url = {https://doi.org/10.1145/3387514.3405871},
    doi = {10.1145/3387514.3405871},
    booktitle = {Proceedings of the Annual Conference of the ACM Special Interest Group on Data Communication on the 
                 Applications, Technologies, Architectures, and Protocols for Computer Communication},
    pages = {310–328},
    numpages = {19},
    keywords = {Static Analysis, Verification, DNS, Formal Methods},
    location = {Virtual Event, USA},
    series = {SIGCOMM ’20}
}
```

[docker-hub]:         https://hub.docker.com/r/sivakesava/groot
[bind mount]:         https://docs.docker.com/storage/bind-mounts

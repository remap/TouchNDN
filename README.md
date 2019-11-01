# TouchNDN
A data-centric framework for media-rich applications.

## What is TouchNDN

TouchNDN is a collection of C++ and Python plugins and libraries for [TouchDesigner](https://derivative.ca/) visual programming environment that utilizes [NDN](http://named-data.net/) to enable data-centric communication in a distributed network of machines. The project originated at [UCLA REMAP](https://remap.ucla.edu/), an interactive storytelling research laboratory.

Data-centric approach to networking stems from [ICN](https://en.wikipedia.org/wiki/Information-centric_networking), where the main idea is to move away from end host addressing to data addressing.
In these networks, a data packet is a secure and autonomous “grain’” that is storage- and host-agnostic and can be fetched by multiple clients simultaneously (inherent multicast). 
This approach, applied for interactive multimedia applications, allows for better scalability of existing and emergent use cases in such fields as augmented reality, low-latency media dissemination, non-linear video streaming, multi-user VR streaming, etc.
For more information on new experiments for data-centric multimedia, please refer to this [paper](http://ice-ar.named-data.net/assets/papers/gusev2019data-centric.pdf).

# Overview
This repository contains source code for TouchNDN components, organized as Xcode project. 

## Setup
### Prerequisites

Currently, TouchNDN is supported only for macOS systems, since NFD can only be built for Linux-based systems.
The following is a list of software and libraries one may need to run TouchNDN:
* [NFD](https://github.com/named-data/NFD)
	* [install](https://named-data.net/doc/NFD/current/INSTALL.html)
	* [configure](https://named-data.net/doc/NFD/current/INSTALL.html#initial-configuration)
* [NDN-CPP](https://github.com/named-data/ndn-cpp)
* [CNL-CPP](https://github.com/named-data/cnl-cpp)
* [python 3.5.1](https://www.python.org/downloads/release/python-351/) (or same version as installed TouchDesigner uses, last time checked - 3.5.1)
* [PyNDN2](https://github.com/named-data/PyNDN2/blob/master/INSTALL.md) (optional, for Python support)
* [PyCNL](https://github.com/named-data/PyCNL) (optional, for Python support)
* [NDN-RTC](https://github.com/remap/ndnrtc) (optional, work in progress)
* TouchNDN helper code:
```
brew tap remap/touchndn && brew install touchndn
```

For easier setup, use [setup.sh] ([TouchNDN/setup.sh at master · remap/TouchNDN · GitHub](https://github.com/remap/TouchNDN/blob/master/setup.sh)) script provided with the repository.
Or just run this command in the terminal:

```
cd $HOME && curl https://raw.githubusercontent.com/remap/touchndn/master/setup.sh | bash
```

### Build
Once checked out, one need to build plugins using Xcode project.
1. Open *touchNDN.xcworkspace* file from *cpp* folder using Xcode 
2. Go to touchNDN project build settings and set up prerequisites paths:
	* `LIBCNLCPP_INSTALL_DIR` — for CNL-CPP library install path;
	* `LIBNDNCPP_INSTALL_DIR` — for NDN-CPP library install path;
	* `LIBNDNRTC_INSTALL_DIR` — for NDN-RTC library install path (optional).
4. Select *namespaceDAT* target and build the project

This will build TouchDesigner plugins into `touchndn-plugins` folder next to the project file.
These plugins can be loaded into TouchDesigner.

### Run 
To run TouchNDN, one must first launch NFD:
```
nfd-start &> /tmp/nfd.log
``` 

To load TouchNDN plugin, from the operator dialog in TouchDesigner select C++ DAT/TOP (depending on the component you want to load) and select plugin from `touchndn-plugins` folder.

### Examples
There is a `example.toe` project distributed with the repo which can be examined for more practical use cases of TouchNDN.

## Components

### FaceDAT

### KeyChainDAT

### NamespaceDAT

### NDN-RTC TOPs

*ndnrtcOut TOP*

*ndnrtcIn TOP*


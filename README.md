# Lurker

This is a stand-alone program which lurks in one or more Twitch channels.  It creates an instance of the `Twitch::Messaging` class and provides it with a factory for creating instances of the `TwitchNetworkTransport::Connection` class in order to connect to the Twitch chat server, join one or more channels, and receive chat messages, user notifications, and other commands from the server.

## Usage

    Usage: Lurker <CHANNEL>..

    Connect to Twitch chat and listen for messages on one or more channels.

      CHANNEL     Name of a Twitch channel to join

Lurker connects to Twitch chat, joins one or more channels, and reports any chat messages posted to the channel, along with any user notifications or other commands received from the server.

## Supported platforms / recommended toolchains

This is a portable C++11 application which depends only on the C++11 compiler, the C and C++ standard libraries, and other C++11 libraries with similar dependencies, so it should be supported on almost any platform.  The following are recommended toolchains for popular platforms.

* Windows -- [Visual Studio](https://www.visualstudio.com/) (Microsoft Visual C++)
* Linux -- clang or gcc
* MacOS -- Xcode (clang)

## Building

This application is not intended to stand alone.  It is intended to be included in a larger solution which uses [CMake](https://cmake.org/) to generate the build system and provide the application with its dependencies.

There are two distinct steps in the build process:

1. Generation of the build system, using CMake
2. Compiling, linking, etc., using CMake-compatible toolchain

### Prerequisites

* [CMake](https://cmake.org/) version 3.8 or newer
* C++11 toolchain compatible with CMake for your development platform (e.g. [Visual Studio](https://www.visualstudio.com/) on Windows)
* [Twitch](https://github.com/rhymu8354/Twitch.git) - a library for interfacing with Twitch chat
* [TwitchNetworkTransport](https://github.com:rhymu8354/TwitchNetworkTransport.git) - an adapter to provide the `Twitch` library with the network connection facilities from `SystemAbstractions`
* [TlsDecorator](https://github.com/rhymu8354/TlsDecorator.git) - an adapter to use `LibreSSL` to encrypt traffic passing through a network connection provided by `SystemAbstractions`
* [LibreSSL](https://www.libressl.org/) (`libtls`, `libssl`, and `libcrypto`) - an implementation of the Secure Sockets Layer (SSL) and Transport Layer Security (TLS) protocols
* [StringExtensions](https://github.com/rhymu8354/StringExtensions.git) - a
  library containing C++ string-oriented libraries, many of which ought to be
  in the standard library, but aren't.
* [SystemAbstractions](https://github.com/rhymu8354/SystemAbstractions.git) - a cross-platform adapter library for system services whose APIs vary from one operating system to another

### Build system generation

Generate the build system using [CMake](https://cmake.org/) from the solution root.  For example:

```bash
mkdir build
cd build
cmake -G "Visual Studio 15 2017" -A "x64" ..
```

### Compiling, linking, et cetera

Either use [CMake](https://cmake.org/) or your toolchain's IDE to build.
For [CMake](https://cmake.org/):

```bash
cd build
cmake --build . --config Release
```

# Swifty Swift Vim

Swifty Swift Vim is a semantic editor backend swift tailored to the needs of
text editors.

It was originally designed to integrate Swift into [YouCompleteMe](https://github.com/Valloric/YouCompleteMe/).

## YouCompleteMe Usage

Install the [RFC branch of YCMD with Swift Supprt](https://github.com/Valloric/ycmd/pull/487)
into your YouCompleteMe installation.

Typically, this means going into wherever you cloned YouCompleteMe and then:

```
  mv third_party/ycmd third_party/ycmd-master
  git clone  https://github.com/jerrymarino/ycmd.git

  # Checkout the RFC branch 
  git checkout remotes/origin/jmarino_swift_prototype_squashed
  git submodule update --init --recursive

  # Build with swift support
  # ( Also, keep clang and potentially debug symbols --debug-symbols )
  ./build.py  --completers --swift-completer --clang-completer
```

By default VIM does not support the `swift` filetype.

Cat this into your `.vimrc`
```
    " Force swift filetype
    autocmd BufNewFile,BufRead *.swift set filetype=swift
```

Then, assert it's set to the correct value when a `swift` file is open.

```
    :set ft?
```

## Supported Features

- Code Completion
- Semantic Diagnostics

## Design

It is built on existing functionality provided by the open source Swift compiler
project.

It includes the following components

- A completion engine for Swift that interfaces with the swift compiler.
- A HTTP server to interface with completion engines ( YouCompleteMe ).

### Completion Engine

The backing implementation of the completion engine is a C++ completer which is
based on the Swift compiler and related APIs.

When reasonable, capabilities are built against SourceKit, the highest level
Swift tooling API. This leverages code reuse, and by using the highest level
APIs as possible, will simplify keeping the completer both feature complete and
up to date.

It makes calls to SourceKit via the sourcekitd client/server implementation,
for now. This is to consumes the SourceKit API at the highest level. This is
similar usage of these featues in Xcode.

It links against the prebuilt binary in Xcode to simplify source builds and
distribution.

### Features

It eventually supports: semantic completion, GoTo definition, diagnostics,
symbol usage, and documentation displaying. It should support compile command
configuration via flags and a JSON compilation database to support complex
projects, similar to clang's JSON compilation database.

### HTTP Server

Completion logic runs out of process on an HTTP server. The server is primarily
designed to work with YouCompleteMe YCMD. It uses HTTP as a protocol to
integrate with YouCompleteMe:
[https://val.markovic.io/articles/youcompleteme-as-a-server](YouCompleteMe)

The frontend is build on [Beast](https://github.com/vinniefalco/Beast) HTTP and Boost ASIO


## Development

In the root directory, you can setup the repository with 1 line

```
  ./bootstrap
```

I log random musings about developing and using this in `notes.txt`. 

This project is still in early phases, and development happens sporadically.

**Contributions welcome**

### Ideas for starter projects
- Write documentation that explains how to use this
- Improve build system and dependency integration
- Design an end to end integration testing system
- Integrate GoogleTest for CPP units
- Get `GoToDefinition` working end to end
- Implement a semantic search engine
- Add the ability to bootstrap a project from an Xcode project

### Ideas for YCM starter projects
- Integrate Diagnostic support with YouCompleteMe
- Add support for swift `.ycm_extra_conf`s in YCMD.

## Acknowledgements 

The HTTP server stands on the shoulders of [Beast](https://github.com/vinniefalco/Beast)(s).
Many thanks to @vinniefalco for Beast and his guidance for getting this up and
running.

Thank you Apple for opening up the [Swift](https://github.com/apple/swift/) compiler and 
IDE facilities. This project would not be possible without this.

Thanks to @Valloric and the YCMD/YouCompleteMe project.


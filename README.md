# Swifty Swift Vim

Swifty Swift Vim is a semantic editor backend for Vim and YouCompleteMe.

## Usage

Install the pull request branch of YCMD into YouCompleteMe.

Typically, this means going into wherever you cloned YouCompleteMe and then:

```
  mv third_party/ycmd third_party/ycmd-master
  git clone  https://github.com/jerrymarino/ycmd.git

  # Checout the RFC branch
  git checkout remotes/origin/jmarino_swift_prototype_squashed
  git submodule update --init --recursive

  # Build with swift support
  ./build.py
```

By default VIM does not support the `swift` filetype.

Cat this into your `.vimrc`
```
    " Force swift filetype
    autocmd BufNewFile,BufRead *.swift set filetype=swift
```

Then, assert it's set to the correct value when a swift file is open.

```
    :set ft?
```

## Supported Features:

- Semantic Completion


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
symbol useage, and documentation displaying. It should support compile command
configuration via flags and a JSON compilation database to support complex
projects, similar to clang's JSON compilation database.

### HTTP Server

Completion logic runs out of process on an HTTP server. The server is primarily
designed to work with YouCompleteMe YCMD. It uses HTTP as a protocol to
integrate with YouCompleteMe:
[https://val.markovic.io/articles/youcompleteme-as-a-server](YouCompleteMe)

The frontend is build on Beast HTTP and Boost ASIO


## Development

In the root directory, you can setup the repository with 1 line

```
  ./bootstrap
```

I log random musings about the trials and tribulations of developing and using
this in `notes.txt`. 


# Swifty Swift Vim

Swifty Swift Vim is a semantic backend for the Swift programming language
tailored to the needs of editing source code.

The project was founded to fulfil the need of semantic Swift support in text editors and
integrate Swift into [YouCompleteMe](https://github.com/Valloric/YouCompleteMe/).

## YouCompleteMe Usage

Install the [RFC branch of YCMD with Swift Support](https://github.com/Valloric/ycmd/pull/487)
into your YouCompleteMe installation.

For example, this means going into wherever you cloned YouCompleteMe and then:

```
  mv third_party/ycmd third_party/ycmd-master
  git clone  https://github.com/jerrymarino/ycmd.git

  # Checkout the RFC branch 
  git checkout remotes/origin/jmarino_swift_prototype_squashed
  git submodule update --init --recursive

  # Build with Swift support
  # ( Also, clang completer and potentially --debug-symbols )
  ./build.py  --completers --swift-completer --clang-completer
```

By default Vim does not support the `swift` filetype.

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

## Technical Design

It implements Semantic abilities based on the Swift programming language and
exposes them via an HTTP server.

It includes the following components

- A semantic engine for Swift.
- An HTTP server.
- **Thin** [YouCompleteMe integration](https://github.com/Valloric/ycmd/pull/487)

### Semantic Engine

The semantic engine is based on the Swift compiler and related APIs.

When reasonable, capabilities leverage SourceKit, the highest level Swift
tooling API. This leverages code reuse, and by using the highest level APIs as
possible, will simplify keeping the completer both feature complete and up to
date.

It makes calls to SourceKit via the `sourcekitd` client. This is to consumes
the SourceKit API at the highest level and is similar usage of these features in
Xcode.

It links against the prebuilt `sourcekitd` binary in Xcode to simplify
distribution and development.

### Features

It should support:
- code completion
- semantic diagnostics
- symbol navigation ( GoTo definition and more )
- symbol usage
- documentation rendering
- semantic searches

It should also support compile command configuration via flags and a JSON
compilation database to support complex projects, similar to clang's JSON
compilation database.  Finally, it should be blazing fast and stable.

### HTTP Frontend

Semantic abilities are exposed through an HTTP protocol. It should be high
performance and serve multiple requests at a time to meet the needs of users
who can make a lot of requests. The protocol is a JSON protocol to simplify
consumer usage and minimize dependencies.

The HTTP frontend is built on [Beast](https://github.com/vinniefalco/Beast)
HTTP and Boost ASIO, a platform for constructing high performance web services.

From a users perspective, logic runs out of the text editor's processes on the
HTTP server. The server is primarily designed to work with YCMD. See [Valloric's](https://val.markovic.io/articles/youcompleteme-as-a-server)
article for more on this.

## Development

`bootstrap` the repository and build.
```
  ./bootstrap
```

I log random musings about developing this and more in `notes.txt`.

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
- Add support for Swift `.ycm_extra_conf`s in YCMD.

## Acknowledgements 

The HTTP server stands on the shoulders of [Beast](https://github.com/vinniefalco/Beast)(s).
Many thanks to @vinniefalco for Beast and his guidance for getting this up and
running.

Thank you Apple for opening up the [Swift](https://github.com/apple/swift/) compiler and 
IDE facilities. This project would not be possible without this.

Thanks to @Valloric and the YCMD/YouCompleteMe project.


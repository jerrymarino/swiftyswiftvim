# Swifty Swift Vim

Swifty Swift Vim is a semantic editor backend for Vim and YouCompleteMe.

It is built on existing functionality provided by the open source Swift compiler
project.

It includes the following components

- A completion engine for Swift that interfaces with the swift compiler.
- A HTTP server to interface with completion engines ( YouCompleteMe ).

[warning] This is quite hacked together! The prototype should support end to end
semantic completion as a proof of concept for a given Swift program.

## Detailed Design

### Completion Engine

The backing implementation of the completion engine is a C++ completer and the
API is exported to a high level python interface. The completion engine is based
on the Swift compiler and related frameworks.r

When reasonable, capabilities are built against SourceKit, the highest level
Swift tooling AP I. This leverages code reuse, and by using the highest level
APIs as possible, will simplify keeping the completer both feature complete and
up to date. It makes calls to SourceKit via othe sourcekitd client/server
implementation, for now. This is to consumes the SourceKit API at the highest
level.

It eventually supports capabilities including: semantic completion, GoTo
definition, diagnostics, symbol useage, and documentation displaying. It should
support compile command configuration via flags and a JSON compilation database
to support complex projects, similar to clang's JSON compilation database. This
capability should be implemented at the Swift compiler level and is out of scope
of this prototype.

### HTTP Server

Completion logic runs out of process on an HTTP server. The server is primarily
designed to work with YouCompleteMe YCMD. It uses HTTP as a protocol to
integrate with YouCompleteMe:
[https://val.markovic.io/articles/youcompleteme-as-a-server](YouCompleteMe)

The actual implementation is basic request handling: the YCMD integration uses
already built functionality in JediHttp.


# redisxx

A very simple Redis clone written in C++23. 

Probably only works on MacOS as it uses `kevent` for async connections.

Has a simple parser combinator library to implement an overly complicated RESP parser.

Implements a few core Redis commands but only responds in RESPv3.


## Motivation

Build your own Redis in C++

Based on <https://build-your-own.org/redis/>
Also <https://dev.to/frosnerd/writing-a-simple-tcp-server-using-kqueue-cah>

And <https://gist.github.com/josephg/6c078a241b0e9e538ac04ef28be6e787>

Also <https://github.com/jmoyers/http>

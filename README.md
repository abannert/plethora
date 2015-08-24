# plethora

Plethora is an HTTP performance testing command-line tool. It can generate
very high numbers of concurrent connections (1000s).

High concurrency is achieved using libevent.

No SSL support at the moment.

Please see the command-line help for usage instructions.


Basic build instructions:

1. install libevent
2. ./buildconf
3. ./configure
4. make
5. ./plethora -h

# libristretto255 [![Build Status][build-image]][build-link] [![Appveyor Status][appveyor-image]][appveyor-link]

[build-image]: https://travis-ci.org/Ristretto/libristretto255.svg?branch=master
[build-link]: https://travis-ci.org/Ristretto/libristretto255
[appveyor-image]: https://ci.appveyor.com/api/projects/status/nn8d1gwg9agumo9l?svg=true
[appveyor-link]: https://ci.appveyor.com/project/tarcieri/libristretto255

<img width="200" height="200" src="https://ristretto.group/theme/ristretto-sm.png">

Experimental C implementation of **Ristretto255**, a prime order elliptic curve
group created by applying the [Decaf] approach to cofactor elimination to
[Curve25519] ([RFC 7748]).

libristretto255 is based off of the [libdecaf] library by Mike Hamburg.
For more information on Ristretto, please see the Ristretto web site:

https://ristretto.group/

[Decaf]: https://www.shiftleft.org/papers/decaf/
[Curve25519]: https://en.wikipedia.org/wiki/Curve25519
[RFC 7748]: https://tools.ietf.org/html/rfc7748
[libdecaf]: https://sourceforge.net/p/ed448goldilocks/wiki/Home/

## Status

This library is in an extremely early stage of development and is not ready to
be used yet. Check back later for updates.

### Known Issues

#### Travis CI

* [#24]: Sporadic SEGV on Travis C with clang when running
  `ristretto_gen_tables`. This has been worked-around by enabling ASAN,
  but is probably indicative of deeper problems.

## License

Copyright (c) 2014-2018 Ristretto Developers ([AUTHORS.md]),
Cryptography Research, Inc (a division of Rambus).

Distributed under the MIT License. See [LICENSE.txt] for more information.

[AUTHORS.md]:  https://github.com/Ristretto/libristretto255/blob/master/AUTHORS.md
[LICENSE.txt]: https://github.com/Ristretto/libristretto255/blob/master/LICENSE.txt

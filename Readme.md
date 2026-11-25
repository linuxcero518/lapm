# lapm - Lofros Advanced Package Manager

A modern C++20 rewrite of `lofpkg`, the minimalist package manager for Lofros.

## Status

Work in progress. The current phase implements:

- [x] Core types: `Version`, `Manifest`, `Package`
- [x] Error handling with `std::expected`
- [x] Configuration with legacy `/etc/lofpkg` fallback
- [x] Safe archive extraction (path traversal protection)
- [x] `install`, `reinstall`, `remove`, `install-local`, `list`, `view`, `clean`, `repo`
- [x] `update` (downloads lists + GPG verification)
- [x] `upgrade`
- [x] Full `lapm-rep info-all`

## Layout compatibility

`lapm` detects `/etc/lofpkg` and uses it transparently. The legacy
scripts (`lofpkg`, `lofpkg-mk`, `lofpkg-rep`) remain functional. A future
`lapm migrate` command will move data to `/etc/lapm` without breaking
installed packages.

### Multiarch

By default `lapm` reads the target architecture from `<root>/arch` if that file
exists, otherwise falls back to `uname -m`. To lock a multiarch host to a
specific target (for example, an x86_64 host that only ships i686 packages),
write the architecture into the file:

    echo i686 | sudo tee /etc/lapm/arch

You can also override it per-invocation with `--arch i686` or by exporting
`LAPM_ARCH=i686`.

## Build

```sh
make
make test
```

## Dependencies
CMake 3.28+, GCC 14+ (GCC 16 recommended for full std::expected),
libcurl, libarchive.

## Usage

```sh
sudo lapm update
sudo lapm install nano
sudo lapm list
sudo lapm view nano
sudo lapm remove nano
sudo lapm install-local ./nano_8.7-1_x86_64.lof
sudo lapm clean
```

## Licence
GPL-3.0-or-later
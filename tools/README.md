# Pogocache tools

Various scripts. Includes tests, a packager, and docker pusher, etc.

## Tests

Running tests should be done from the root with:

```
make test
```

But they can also be like this:

```
tools/tests/run.sh
tools/tests/run.sh [testname]
```

## Package

Running `make package` will build for various architectures, each as a tar.gz
package containing the Pogocache program and license. These packages are placed
in the 'packages' directory in the repository root.

To build all packages:

```
make package
```

To build a single package:

```
tools/build/run.sh linux-aarch64
```

## Docker push

This is for CI and requires maintainer privileges, but in short, this pushes
the latest docker image, including 'edge', 'latest', and current version.

Must be run from the main branch, and login must be privileged.

```
tools/docker-push.sh
```

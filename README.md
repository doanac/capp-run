A tool for running docker-compose apps created by capp-pub.

## Quickstart

You'll need to build/install [crun](https://github.com/containers/crun).

The `dev-shell` script builds and drops you inside a container with all
the build dependencies:

~~~
 # one time
 $ mkdir build && cd build
 $ ../cmake-init.sh

 # build
 $ ninja  # creates ./capp-run binary

 # helpers
 $ ninja clang-format
 $ ninja clang-tidy
~~~

Next extract the bundle from capp-pub:
~~~
 $ mkdir example && cd example
 $ tar -xzf <path>/compose-bundle.tgz
~~~

Run one of the compose services with:
~~~
 # Extract container image
 $ sudo ../build/capp-run pull test-user
 Pulling test-user: docker.io/library/alpine@sha256:a75afd8b57e7f34e4dad8d65e2c7ba2e1975c795ce1ee22fa34f8cf46f96a3be
 docker.io/library/alpine@sha256:a75afd8b57e7f34e4dad8d65e2c7ba2e1975c795ce1ee22fa34f8cf46f96a3be: Pulling from library/alpine
 Digest: sha256:a75afd8b57e7f34e4dad8d65e2c7ba2e1975c795ce1ee22fa34f8cf46f96a3be
 Status: Image is up to date for alpine@sha256:a75afd8b57e7f34e4dad8d65e2c7ba2e1975c795ce1ee22fa34f8cf46f96a3be
 docker.io/library/alpine@sha256:a75afd8b57e7f34e4dad8d65e2c7ba2e1975c795ce1ee22fa34f8cf46f96a3be
 Extracting

 # Execute the container
 $ sudo ../build/capp-run up test-user
 Starting test-user
 Mounting overlay
 Execing: crun run -f "/var/run/capprun/example/test-user/config.json" example-test-user
 =user: PASS
 =group: PASS
~~~

## Missing Features

* Networking is quite limited, but progressing
* Volumes not yet done
* Can't run all services in a single command. The idea is systemd will handle
  this.

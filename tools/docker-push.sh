#!/bin/bash

# Build and push the latest docker image to docker hub.
# This is intended to be run in a CI like Github Actions.

help() {
  echo "Usage: tools/docker-push.sh --force --local"
  echo ""
  echo "Options:"
  echo "   --local    run locally, not CI. Uses local Docker login."
  echo "   --force    force build and push from any branch"
  echo "   --nopush   do not push (dryrun)"
  echo ""
}

oforce=0
onopush=0
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -f|--force) oforce=1 ;;
        -l|--local) olocal=1 ;;
        -n|--nopush) onopush=1 ;;
        --help) help; exit 0 ;;
        *) echo "Unknown parameter passed: $1"
           echo "Run '$0 --help' for more information"
          exit 1 ;;
    esac
    shift
done

# -- PROGRAM -- #

set -e
cd $(dirname "${BASH_SOURCE[0]}")

# GIT_BRANCH is the current branch name
export GIT_BRANCH=$(git branch --show-current)
# GIT_VERSION - always the last verison number, like 1.12.1.
export GIT_VERSION=$(git describe --tags --abbrev=0)
# GIT_COMMIT_SHORT - the short git commit number, like a718ef0.
export GIT_COMMIT_SHORT=$(git rev-parse --short HEAD)
# DOCKER_REPO - the base repository name to push the docker build to.
export DOCKER_REPO=pogocache/pogocache

if [ "$GIT_BRANCH" != "main" ]; then
  if [ "$oforce" == "1" ]; then
    echo "Force pushing from non-main branch"
  else
    echo "Not pushing, not on main branch"
    echo "Use '$0 --force' to force push from any branch"
    exit 0
  fi
fi

if [ "$olocal" == "1" ]; then
  echo "Using local Docker login"
else
  if [ "$DOCKER_LOGIN" == "" ]; then
    echo "Not pushing, DOCKER_LOGIN not set"
    echo "Use '$0 --local' to use local Docker login"
    exit 1
  elif [ "$DOCKER_PASSWORD" == "" ]; then
    echo "Not pushing, DOCKER_PASSWORD not set"
    echo "Use '$0 --local' to use local Docker login"
    exit 1
  fi
  echo $DOCKER_PASSWORD | docker login -u $DOCKER_LOGIN --password-stdin
  DOCKER_TOKEN=$(curl -s -H "Content-Type: application/json" \
    -X POST \
    -d "{\"username\": \"$DOCKER_LOGIN\", \"password\": \"$DOCKER_PASSWORD\"}" \
    https://hub.docker.com/v2/users/login/ | jq -r .token)
fi

oexists=0
rurl="https://hub.docker.com/v2/repositories/$DOCKER_REPO"
if [ "$(curl -s $rurl/tags/$GIT_VERSION/ | grep "digest")" != "" ]; then
  # Current version has already been pushed to docker
  oexists=1
fi

echo "Building docker image..."

build() {
  build/run.sh linux-$1
  rm -rf pogocache*
  tar -xzf ../packages/pogocache-linux-$1.tar.gz
  mv pogocache-linux-$1/pogocache pogocache
  docker build --platform linux/$2 . -t pogocache/pogocache:$2
  rm -rf pogocache*
}

# Build local images
build arm64-musl arm64
build amd64-musl amd64

if [[ "$onopush" == "1" ]]; then
  echo "User provided --nopush, exit now"
  exit
fi

# Push to docker
docker push pogocache/pogocache:arm64
docker push pogocache/pogocache:amd64

buildx() {
  docker buildx imagetools create \
    -t pogocache/pogocache:$1 \
    pogocache/pogocache:amd64 \
    pogocache/pogocache:arm64
}

buildx edge
if [ "$oexists" == 0 ]; then
  # Version does exists, also push those
  buildx latest
  buildx $GIT_VERSION
else
  echo "Could not push latest and $GIT_VERSION, already exist"
fi

if [ "$DOCKER_TOKEN" == "" ]; then
  echo "The remote tags 'arm64' and 'amd64' must be manually deleted"
else
  echo "Deleting remote tags 'arm64' and 'amd64'"
  curl -H "Authorization: JWT $DOCKER_TOKEN" -X DELETE $rurl/tags/arm64
  curl -H "Authorization: JWT $DOCKER_TOKEN" -X DELETE $rurl/tags/amd64
fi

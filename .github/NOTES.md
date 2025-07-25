## Notes

### Docker release

Manually run:

```sh
docker buildx create --name multiarch --use
docker buildx build --platform linux/amd64,linux/arm64 -t pogocache/pogocache:latest --push .
```

TODO: Use Github Actions workflow similar to https://github.com/tidwall/tile38

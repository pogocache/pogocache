FROM ubuntu:22.04 AS builder
RUN apt update -y && apt install -y build-essential git wget
COPY . /pogocache
RUN cd pogocache && make distclean && make clean && make 

FROM ubuntu:22.04
COPY --from=builder /pogocache/pogocache /usr/local/bin
EXPOSE 9401
ENTRYPOINT ["pogocache"]

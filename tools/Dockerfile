FROM alpine:3.22

COPY entrypoint.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/entrypoint.sh
ENTRYPOINT ["entrypoint.sh"]

COPY pogocache /usr/local/bin/
EXPOSE 9401
CMD ["pogocache"]

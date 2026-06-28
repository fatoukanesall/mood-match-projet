FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    pkg-config \
    libsqlite3-dev \
    libcurl4-openssl-dev \
    libcjson-dev \
    libmicrohttpd-dev \
    libpq-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY src/ ./src/
COPY Makefile ./

RUN make

EXPOSE 8080

CMD ["./moodmatch_backend"]

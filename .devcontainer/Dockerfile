# Start with the Debian 12 (bookworm) image
FROM mcr.microsoft.com/devcontainers/base:bookworm

RUN apt-get update && apt-get upgrade -y

RUN apt-get install -y \
    libargon2-1 \
    libargon2-dev \
    postgresql \
    postgresql-client \
    postgresql-contrib \
    libpq-dev \
    uuid-dev \
    valgrind \
    gdb \
    && apt-get clean

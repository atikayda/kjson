# Dockerfile for building and testing kJSON PostgreSQL extension
FROM postgres:16

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    postgresql-server-dev-16 \
    git \
    make \
    gcc \
    && rm -rf /var/lib/apt/lists/*

# Create build directory
WORKDIR /build

# Copy C library files
COPY c/kjson.h /build/c/
COPY c/kjson.c /build/c/
COPY c/kjson_stringify.c /build/c/
COPY c/kjson_binary.c /build/c/

# Copy PostgreSQL extension files
COPY postgres/ /build/postgres/
# Copy Docker-specific Makefile
COPY postgres/Makefile.docker /build/postgres/

# Build the C library first
WORKDIR /build/c
RUN gcc -c -fPIC -O2 -Wall kjson.c kjson_stringify.c kjson_binary.c

# Build the PostgreSQL extension
WORKDIR /build/postgres
# Create symlink for C library files as expected by Makefile
RUN ln -s /build/c ../c
# Use the Docker-specific Makefile
RUN make -f Makefile.docker && make -f Makefile.docker install

# Initialize database with kjson extension
COPY postgres/docker-init.sql /docker-entrypoint-initdb.d/

# Expose PostgreSQL port
EXPOSE 5432
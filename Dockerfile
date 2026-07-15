FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y \
    cmake g++ libsqlite3-dev python3 python3-pip python3-venv \
    libgtest-dev git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN rm -rf build && mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . -j$(nproc)

FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    python3 python3-pip python3-venv libsqlite3-0 \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m venv /app/.venv && \
    /app/.venv/bin/pip install --no-cache-dir click pyyaml jinja2 rich pandas

COPY --from=builder /app/build/reconcilex /usr/local/bin/reconcilex

COPY python/ /app/python/
COPY templates/ /app/templates/
COPY config/ /app/config/
COPY data/ /app/data/
COPY pyproject.toml /app/

ENV PATH="/app/.venv/bin:$PATH"

EXPOSE 8050
CMD ["python3", "-m", "recon.cli", "dashboard"]
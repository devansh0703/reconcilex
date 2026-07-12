FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    cmake g++ libsqlite3-dev python3 python3-pip python3-venv \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN chmod +x scripts/build.sh && scripts/build.sh Release

FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    python3 python3-pip python3-venv libsqlite30 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/build/reconcilex /usr/local/bin/reconcilex
COPY python/ /app/python/
COPY templates/ /app/templates/
COPY config/ /app/config/
COPY data/ /app/data/
COPY pyproject.toml /app/

RUN python3 -m venv /app/.venv && \
    /app/.venv/bin/pip install -e ".[dev]" 2>/dev/null || \
    /app/.venv/bin/pip install click pyyaml jinja2 rich pandas 2>/dev/null

ENV PATH="/app/.venv/bin:$PATH"

EXPOSE 8050
CMD ["python3", "-m", "recon.cli", "dashboard"]

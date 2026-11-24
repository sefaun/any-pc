# syntax=docker/dockerfile:1.7

############################
# Build stage
############################
FROM node:22-alpine AS build
WORKDIR /app

COPY package.json package-lock.json ./
RUN npm ci --no-audit --no-fund

COPY tsconfig.json tsconfig.build.json nest-cli.json ./
COPY src ./src
RUN npm run build && npm prune --omit=dev

############################
# Runtime stage
############################
FROM node:22-alpine AS runtime

ARG VERSION=dev
LABEL org.opencontainers.image.title="anypc-backend" \
      org.opencontainers.image.description="AnyPC signaling server and Cloudflare Realtime SFU broker" \
      org.opencontainers.image.version="${VERSION}" \
      org.opencontainers.image.source="https://github.com/sefaun/any-pc"

ENV NODE_ENV=production \
    PORT=3000 \
    DATA_DIR=/app/data

WORKDIR /app
RUN mkdir -p /app/data && chown -R node:node /app

COPY --from=build --chown=node:node /app/package.json ./
COPY --from=build --chown=node:node /app/node_modules ./node_modules
COPY --from=build --chown=node:node /app/dist ./dist

USER node
EXPOSE 3000
VOLUME ["/app/data"]

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
  CMD wget -qO- "http://127.0.0.1:${PORT}/api/v1/health" >/dev/null || exit 1

# Node runs as PID 1; the app installs SIGTERM/SIGINT handlers (enableShutdownHooks)
# so `docker stop` triggers a graceful shutdown that flushes the data store.
STOPSIGNAL SIGTERM
CMD ["node", "dist/main.js"]

/**
 * node_server.js — Minimal Node.js HTTP server for benchmarking.
 *
 * Usage:  node node_server.js [port]
 *
 * This mirrors the WebCraft benchmark server: a fixed "Hello, World!" HTML
 * response with keep-alive enabled (default in Node.js http module).
 * No logging, no framework overhead — just the bare runtime.
 */

const http = require("http");

const PORT = parseInt(process.argv[2] || "8081", 10);

const BODY =
  '<!DOCTYPE html>\n<html><head><title>Node.js Benchmark</title></head>\n<body><h1>Hello, World!</h1></body></html>\n';

const HEADERS = {
  "Content-Type": "text/html",
  "Content-Length": Buffer.byteLength(BODY),
  Connection: "keep-alive",
};

const server = http.createServer((req, res) => {
  res.writeHead(200, HEADERS);
  res.end(BODY);
});

server.listen(PORT, "0.0.0.0", () => {
  console.log(`Node.js benchmark server listening on 0.0.0.0:${PORT}`);
});

// Graceful shutdown
process.on("SIGINT", () => {
  server.close(() => process.exit(0));
});
process.on("SIGTERM", () => {
  server.close(() => process.exit(0));
});

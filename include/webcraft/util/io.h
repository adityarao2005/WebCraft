#pragma
#include <webcraft/util/async.h>
#include <iostream>
#include <memory>
#include <string>
#include <cstddef>
#include <cstring>

namespace WebCraft {
	namespace Util {
		namespace IO {
			namespace Sync {
				// Base Stream class
				class Stream {
				public:
					virtual void close() = 0;
					virtual bool isClosed() = 0;
				};

				// Readable Synchronous streams
				class ReadableStream : public Stream {
				public:
					// Read a buffer of size `size` into `buffer` at `offset`
					virtual int read(char* buffer, size_t offset, size_t size) = 0;

					// Read a buffer of size `size` into `buffer`
					virtual int read(char* buffer, size_t size) {
						return read(buffer, 0, size);
					}

					// Read a buffer of size `size` into `buffer` at `offset`
					int read(std::string& buffer, size_t size) {
						char* temp = new char[size];
						int s = read(temp, size);
						buffer = std::string(temp, s);
						delete[] temp;
						return s;
					}

					// Read a buffer of size `size` into `buffer`
					std::string read(size_t size) {
						std::string buffer = "";
						read(buffer, size);
						return buffer;
					}
				};

				// Writable Synchronous streams
				class WritableStream : public Stream {
				public:
					// Write a buffer of size `size` from `buffer`
					virtual void write(const char* buffer, size_t offset, size_t size) = 0;

					// Write a buffer of size `size` from `buffer`
					void write(const char* buffer, size_t size) {
						write(buffer, 0, size);
					}
					// Write a buffer of size `size` from `buffer` at `offset`
					void write(const std::string& buffer) {
						write(buffer.data(), buffer.size());
					}

					// Write a buffer of size `size` from `buffer`
					void write(const std::string& buffer, size_t size) {
						write(buffer, size);
					}

					// Write a buffer of size `size` from `buffer` at `offset`
					void write(const std::string& buffer, size_t offset, size_t size) {
						write(std::string(buffer, offset, size));
					}

					// Write a buffer of size `size` from `buffer`
					void write(const char* buffer) {
						write(buffer, strlen(buffer));
					}
				};

				// Duplex Synchronous streams
				class DuplexStream : public ReadableStream, public WritableStream {

				};

			}

			namespace Async {

				// Base Stream class
				class Stream {
				public:
					virtual async(void) close() = 0;
					virtual async(bool) isClosed() = 0;
				};

				// Readable Asynchronous streams
				class ReadableStream : public Stream {
				public:
					// Read a buffer of size `size` into `buffer` at `offset`
					virtual async(int) read(char* buffer, size_t offset, size_t size) = 0;

					// Read a buffer of size `size` into `buffer`
					async(int) read(char* buffer, size_t size) {
						co_return co_await read(buffer, 0, size);
					}

					// Read a buffer of size `size` into `buffer` at `offset`
					async(int) read(std::string& buffer, size_t size) {
						char* temp = new char[size];
						int s = co_await read(temp, size);
						buffer = std::string(temp, s);
						delete[] temp;
						co_return s;
					}

					// Read a buffer of size `size` into `buffer`
					async(std::string) read(size_t size) {
						std::string buffer = "";
						co_await read(buffer, size);
						co_return buffer;
					}
				};

				// Writable Asynchronous streams
				class WritableStream : public Stream {
				public:
					// Write a buffer of size `size` from `buffer`
					virtual async(void) write(const char* buffer, size_t offset, size_t size) = 0;

					// Write a buffer of size `size` from `buffer`
					async(void) write(const char* buffer, size_t size) {
						co_await write(buffer, 0, size);
					}

					// Write a buffer of size `size` from `buffer` at `offset`
					async(void) write(const std::string& buffer) {
						co_await write(buffer.data(), buffer.size());
					}

					// Write a buffer of size `size` from `buffer`
					async(void) write(const std::string& buffer, size_t size) {
						co_await write(buffer, size);
					}

					// Write a buffer of size `size` from `buffer` at `offset`
					async(void) write(const std::string& buffer, size_t offset, size_t size) {
						co_await write(std::string(buffer, offset, size));
					}

					// Write a buffer of size `size` from `buffer`
					async(void) write(const char* buffer) {
						co_await write(buffer, strlen(buffer));
					}
				};

				// Duplex Asynchronous streams
				class DuplexStream : public ReadableStream, public WritableStream {
				};

			}

			namespace Files {
				template<typename T>
				using unique = std::unique_ptr<T>;

				namespace internal {
					// Represents the file provider
					class FileProvider {
					public:

						// Open a file for reading
						virtual unique<Sync::ReadableStream> openRead(const std::string& path) = 0;
						// Open a file for writing
						virtual unique<Sync::WritableStream> openWrite(const std::string& path) = 0;
						// Open a file for reading and writing
						virtual unique<Sync::DuplexStream> open(const std::string& path) = 0;

						// Open a file for reading asynchronously
						virtual async(unique<Async::ReadableStream>) openReadAsync(const std::string& path) = 0;
						// Open a file for writing asynchronously
						virtual async(unique<Async::WritableStream>) openWriteAsync(const std::string& path) = 0;
						// Open a file for reading and writing asynchronously
						virtual async(unique<Async::DuplexStream>) openAsync(const std::string& path) = 0;
					};
				}

				class Files {
				private:
					static unique<internal::FileProvider> provider;
				public:

					static unique<Sync::ReadableStream> openRead(const std::string& path) {
						return provider->openRead(path);
					}

					static unique<Sync::WritableStream> openWrite(const std::string& path) {
						return provider->openWrite(path);
					}

					static unique<Sync::DuplexStream> open(const std::string& path) {
						return provider->open(path);
					}

					static async(unique<Async::ReadableStream>) openReadAsync(const std::string& path) {
						co_return co_await provider->openReadAsync(path);
					}

					static async(unique<Async::WritableStream>) openWriteAsync(const std::string& path) {
						co_return co_await provider->openWriteAsync(path);
					}

					static async(unique<Async::DuplexStream>) openAsync(const std::string& path) {
						co_return co_await provider->openAsync(path);
					}
				};
			}

			namespace Memory {
				class MemorySyncStream : public Sync::DuplexStream {
				private:
					std::string buffer;
					size_t position;
				public:
					MemorySyncStream() : buffer(""), position(0) {}

					void close() override {
						buffer = "";
						position = 0;
					}

					bool isClosed() override {
						return buffer.empty();
					}

					int read(char* buffer, size_t offset, size_t size) override {
						memcpy(buffer, this->buffer.data() + position + offset, size);
						position += size;
						return size;
					}

					void write(const char* buffer, size_t offset, size_t size) override {
						this->buffer.append(buffer, offset, size);
					}
				};
				class MemoryAsyncStream : public Async::DuplexStream {
				private:
					std::string buffer;
					size_t position;
				public:
					MemoryAsyncStream() : buffer(""), position(0) {}

					async(void) close() override {
						buffer = "";
						position = 0;
						co_return;
					}

					async(bool) isClosed() override {
						co_return buffer.empty();
					}

					async(int) read(char* buffer, size_t offset, size_t size) override {
						memcpy(buffer, this->buffer.data() + position + offset, size);
						position += size;
						co_return size;
					}

					async(void) write(const char* buffer, size_t offset, size_t size) override {
						this->buffer.append(buffer, offset, size);
						co_return;
					}
				};
			};

		}
	}
}
#ifndef INCLUDE_CCAPI_CPP_CCAPI_INFLATE_STREAM_H_
#define INCLUDE_CCAPI_CPP_CCAPI_INFLATE_STREAM_H_
#ifndef CCAPI_DECOMPRESS_BUFFER_SIZE
#define CCAPI_DECOMPRESS_BUFFER_SIZE 1 << 20
#endif
#include "zlib.h"
namespace ccapi {
/**
 * Due to Huobi using gzip instead of zip in data compression, we cannot use beast::zboost::system::inflate_stream. Therefore we have to create our own.
 */
class InflateStream CCAPI_FINAL {
 public:
  explicit InflateStream() {
    this->windowBits = 15;
    this->windowBitsOverride = 0;
    this->istate.zalloc = Z_NULL;
    this->istate.zfree = Z_NULL;
    this->istate.opaque = Z_NULL;
    this->istate.avail_in = 0;
    this->istate.next_in = Z_NULL;
    this->decompressBufferSize = CCAPI_DECOMPRESS_BUFFER_SIZE;
  }
  std::string toString() const {
    std::string output = "InflateStream [windowBits = " + ccapi::toString(windowBits) + "]";
    return output;
  }
  virtual ~InflateStream() {
    if (!this->initialized) {
      return;
    }
    int ret = inflateEnd(&this->istate);
    if (ret != Z_OK) {
      CCAPI_LOGGER_ERROR("error cleaning up zlib decompression state");
    }
  }
  void setWindowBitsOverride(int windowBitsOverride) { this->windowBitsOverride = windowBitsOverride; }
  boost::system::error_code init() {
    int ret;
    if (this->windowBitsOverride == 0) {
      ret = inflateInit2(&this->istate, -1 * this->windowBits);
    } else {
      ret = inflateInit2(&this->istate, this->windowBitsOverride
                         //            31
      );
    }
    if (ret != Z_OK) {
      CCAPI_LOGGER_ERROR("decompress error");
      return boost::system::error_code();
    }
    this->buffer.reset(new unsigned char[this->decompressBufferSize]);
    this->initialized = true;
    return boost::system::error_code();
  }
  boost::system::error_code decompress(uint8_t const *buf, size_t len, std::string &out) {
    if (!this->initialized) {
      CCAPI_LOGGER_ERROR("decompress error");
      return boost::system::error_code();
    }
    int ret;
    this->istate.avail_in = len;
    this->istate.next_in = const_cast<unsigned char *>(buf);
    do {
      this->istate.avail_out = this->decompressBufferSize;
      this->istate.next_out = this->buffer.get();
      int ret = inflate(&this->istate, Z_SYNC_FLUSH);
      if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
        CCAPI_LOGGER_ERROR("decompress error");
        return boost::system::error_code();
      }
      out.append(reinterpret_cast<char *>(this->buffer.get()), this->decompressBufferSize - this->istate.avail_out);
    } while (this->istate.avail_out == 0);
    return boost::system::error_code();
  }
  boost::system::error_code inflate_reset() {
    int ret = inflateReset(&this->istate);
    if (ret != Z_OK) {
      CCAPI_LOGGER_ERROR("decompress error");
      return boost::system::error_code();
    }
    return boost::system::error_code();
  }
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  int windowBits;
  int windowBitsOverride;
  bool initialized;
  std::unique_ptr<unsigned char[]> buffer;
  z_stream istate;
  size_t decompressBufferSize;
};

} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_INFLATE_STREAM_H_

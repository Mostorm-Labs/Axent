//
// Created by Staney on 16/9/2019.
//

#ifndef AUX_STRING_BUFFER_H
#define AUX_STRING_BUFFER_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <cstdarg>
#include <cstring>

#ifndef uint32_t
typedef unsigned int uint32_t;
#endif

//#ifdef _UNICODE
#define N_CHAR  1 //for wchar(2 bytes) or char(1 byte)
#define SB_STRCPY(dst, src, cnt)  {memcpy(dst, src, cnt);*(dst+cnt) = 0;}
//#define SB_STRCPY(dst, src, cnt)  {strncpy(dst, src, cnt);}
//#endif

namespace axdp {
    /**
     * Convert raw bytes to HEX format string
     * */
    class HEX {
    public:
        static std::shared_ptr<char> dump(const uint8_t *buffer, uint16_t len) {
            std::shared_ptr<char> hex(new char[len * 3 + N_CHAR], std::default_delete<char[]>());
            memset(hex.get(), 0, len * 3 + N_CHAR);

            char temp[8];
            for (uint16_t index = 0; index < len; index++) {
                sprintf(temp, "%02X ", buffer[index]);
                strcat(hex.get(), temp);
            }
            // return std::move(hex);
            return hex;
        }

        static std::shared_ptr<char> dump(const char *buffer, uint16_t len) {
            return dump((const uint8_t *) buffer, len);
        }
    };


#define DEFAULT_CAPACITY  128

    /**
     * [cjb remark] thread safe buffer
     * */
    class StringBuffer {
    public:
        StringBuffer() : StringBuffer(DEFAULT_CAPACITY) {}

        explicit StringBuffer(uint32_t capacity) : _len(0) {
            _capacity = capacity;
            _buf = new char[_capacity];
        }

        ~StringBuffer() {
            delete[] _buf;
        }

        explicit StringBuffer(const char *str) : StringBuffer() {
            auto len = static_cast<uint32_t>(strlen(str));
            _ensureCapacity(len + N_CHAR);
            SB_STRCPY(_buf, str, len);
        }

        StringBuffer(const StringBuffer &sb) : StringBuffer(sb.capacity()) {
            SB_STRCPY(_buf, sb.c_str(), sb.length());
        }

        StringBuffer &operator=(const StringBuffer &sb) {
            if (&sb != this) {
                _mutex.lock();
                _ensureCapacity(sb.length() + N_CHAR);
                SB_STRCPY(_buf, sb.c_str(), sb.length());
                _mutex.unlock();
            }
            return *this;
        }

        void format(const char *fmt, ...) {
            clear();
            va_list args, args_copy;
            va_start(args, fmt);
            va_copy(args_copy, args);
            uint32_t len = std::vsnprintf(nullptr, 0, fmt, args);

            _mutex.lock();
            _ensureCapacity(len + N_CHAR);
            // Writes the results to a character string buffer. At most buf_size-1 characters are written.
            // The resulting character string will be terminated with a null character, unless buf_size is zero.
            // If buf_size is zero, nothing is written and buffer may be a null pointer,
            // however the return value (number of bytes that would be written not including the null terminator)
            // is still calculated and returned.
            _len = std::vsnprintf(_buf, capacity(), fmt, args_copy);
            _mutex.unlock();

            va_end(args_copy);
            va_end(args);
        }

        std::shared_ptr<char> toHEX() {
            return HEX::dump(_buf, _len);
        }

        void appendFormat(const char *fmt, ...) {
            va_list args, args_copy;
            va_start(args, fmt);
            va_copy(args_copy, args);
            uint32_t count = std::vsnprintf(nullptr, 0, fmt, args);

            _mutex.lock();
            _ensureCapacity(_len + count + N_CHAR);
            _len += std::vsnprintf(_buf + _len, capacity() - _len, fmt, args_copy);
            _mutex.unlock();

            va_end(args_copy);
            va_end(args);
        }

        StringBuffer &append(const std::string &str) {
            return append(str.c_str(), static_cast<uint32_t>(str.length()));
        }

        StringBuffer &append(const char *str) {
            return append(str, static_cast<uint32_t>(strlen(str)));
        }

        StringBuffer &append(const char *str, uint32_t len) {
            _mutex.lock();
            _ensureCapacity(_len + len + N_CHAR);
            SB_STRCPY(_buf + _len, str, len);
            _len += len;
            _mutex.unlock();

            return *this;
        }

        const char *c_str() const {
            return _buf;
        }

        void clear() {
            _len = 0;
            *_buf = 0;
        }

        uint32_t length() const {
            return _len;
        }

        uint32_t capacity() const {
            return _capacity;
        }

    private:
        void _ensureCapacity(uint32_t newCapacity) {
            // overflow-conscious code
            if (newCapacity > _capacity) {
                _capacity = ((_len << 1) > newCapacity ? (_len << 1) : newCapacity) + 2;
                char *tmpBuf = new char[_capacity];
                SB_STRCPY(tmpBuf, _buf, _len);
                delete[]_buf;
                _buf = tmpBuf;
            }
        }

        uint32_t _len;
        uint32_t _capacity;
        char *_buf;
        std::mutex _mutex;
    };

}
#endif //AUX_STRING_BUFFER_H

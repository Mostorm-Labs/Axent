#include "axdp_utils.h"
//#include "third_party/md5/include/md5.h"

#define BigLittleSwap16(A)  ((((uint16_t)(A) & 0xff00) >> 8) |    \
                             (((uint16_t)(A) & 0x00ff) << 8))

#define BigLittleSwap32(A)  ((((uint32_t)(A) & 0xff000000) >> 24) |    \
                             (((uint32_t)(A) & 0x00ff0000) >> 8) |    \
                             (((uint32_t)(A) & 0x0000ff00) << 8) |    \
                             (((uint32_t)(A) & 0x000000ff) << 24))
FILE *awxlog = nullptr;
namespace axdp {

    namespace utils {


        uint32_t checkEndian() {
            static uint32_t checked = 0, endian = 0;
            union {
                uint32_t i;
                uint8_t s[4];
            } c;

            if (checked) return endian;

            c.i = 0x12345678;
            endian = (0x12 == c.s[0]);
            checked = 1;

            return endian;
        }

        uint32_t htonl(uint32_t h) {
            return checkEndian() ? h : BigLittleSwap32(h);
        }

        uint32_t ntohl(uint32_t n) {
            return checkEndian() ? n : BigLittleSwap32(n);
        }

        uint16_t htons(uint16_t h) {
            return checkEndian() ? h : BigLittleSwap16(h);
        }

        uint16_t ntohs(uint16_t n) {
            return checkEndian() ? n : BigLittleSwap16(n);
        }


        void printfBufferHeader(const uint8_t *buffer) {
            awx_print("(net endian):magic=0x%04x, version=0x%04x, dst=0x%04x,"
                      "src=0x%04x, cmd=0x%04x, len=0x%04x, crc=0x%04x",
                      *(uint16_t *) (buffer + 0),
                      *(uint16_t *) (buffer + 2),
                      *(uint16_t *) (buffer + 4),
                      *(uint16_t *) (buffer + 6),
                      *(uint16_t *) (buffer + 8),
                      *(uint16_t *) (buffer + 10),
                      *(uint16_t *) (buffer + 12)
            );
        }

        void printfBufferPayload(const uint8_t *buffer) {
            if (buffer != nullptr) {
                awx_print("(payload value 4 bytes):0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
                          *(uint8_t *) (buffer + 14),
                          *(uint8_t *) (buffer + 14 + 1),
                          *(uint8_t *) (buffer + 14 + 2),
                          *(uint8_t *) (buffer + 14 + 3),
                          *(uint8_t *) (buffer + 14 + 4),
                          *(uint8_t *) (buffer + 14 + 5));
            }
        }
    }
}
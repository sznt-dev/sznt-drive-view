// Build tool: compresses an installer payload file with the Windows Compression API (MSZIP).
// Usage: pack <in> <out>
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <compressapi.h>
#include <cstdio>
#include <vector>

int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 1;
    std::vector<unsigned char> in;
    unsigned char buf[65536];
    for (size_t n; (n = fread(buf, 1, sizeof(buf), f)) > 0;) in.insert(in.end(), buf, buf + n);
    fclose(f);
    COMPRESSOR_HANDLE c = nullptr;
    if (!CreateCompressor(COMPRESS_ALGORITHM_MSZIP, nullptr, &c)) return 1;
    SIZE_T size = 0;
    Compress(c, in.data(), in.size(), nullptr, 0, &size);
    std::vector<unsigned char> out(size);
    if (!Compress(c, in.data(), in.size(), out.data(), out.size(), &size)) return 1;
    CloseCompressor(c);
    f = fopen(argv[2], "wb");
    if (!f) return 1;
    fwrite(out.data(), 1, size, f);
    fclose(f);
    printf("%s: %zu -> %zu\n", argv[1], in.size(), (size_t)size);
    return 0;
}

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <datawriter/ExclusiveOutputFile.hpp>
#include <string>
#include <system_error>

ExclusiveOutputFile::ExclusiveOutputFile() : output(&buffer) {}

ExclusiveOutputFile::~ExclusiveOutputFile() { close(); }

void ExclusiveOutputFile::open(const std::string& path) {
    close();

    // "x" (C11) creates the file, or fails if it already exists, in one atomic
    // step. C++20 streams cannot express this (std::ios::noreplace is C++23),
    // hence the C handle, which then stays open for every write.
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    file = std::fopen(path.c_str(), "wx");
    if (file == nullptr) {
        const int error = errno;
        throw std::system_error(error, std::generic_category(),
                                "Cannot create output file " + path);
    }

    buffer.attach(file);
    output.clear();
}

void ExclusiveOutputFile::close() {
    if (file == nullptr) {
        return;
    }

    output.flush();
    buffer.attach(nullptr);
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    std::fclose(file);
    file = nullptr;
}

ExclusiveOutputFile::HandleBuffer::int_type ExclusiveOutputFile::HandleBuffer::overflow(
    int_type character) {
    if (traits_type::eq_int_type(character, traits_type::eof())) {
        return traits_type::not_eof(character);
    }
    if (file == nullptr || std::fputc(character, file) == EOF) {
        return traits_type::eof();
    }
    return character;
}

std::streamsize ExclusiveOutputFile::HandleBuffer::xsputn(const char_type* text,
                                                          std::streamsize  count) {
    if (file == nullptr || count <= 0) {
        return 0;
    }
    return static_cast<std::streamsize>(
        std::fwrite(text, sizeof(char_type), static_cast<std::size_t>(count), file));
}

int ExclusiveOutputFile::HandleBuffer::sync() {
    return file == nullptr || std::fflush(file) == 0 ? 0 : -1;
}

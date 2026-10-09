#ifndef EXCLUSIVEOUTPUTFILE_HPP
#define EXCLUSIVEOUTPUTFILE_HPP

#include <cstdio>
#include <ostream>
#include <streambuf>
#include <string>

// An output file owned from creation to close. open() creates the path
// atomically and fails if it already exists, and every write goes through that
// same handle: the path is never reopened, so a file another process puts under
// the same name can be neither truncated nor written into. This is how format
// strategies keep IDataFormatStrategy's no-overwrite contract.
class ExclusiveOutputFile {
   public:
    ExclusiveOutputFile();
    ~ExclusiveOutputFile();

    ExclusiveOutputFile(const ExclusiveOutputFile&)            = delete;
    ExclusiveOutputFile& operator=(const ExclusiveOutputFile&) = delete;
    ExclusiveOutputFile(ExclusiveOutputFile&&)                 = delete;
    ExclusiveOutputFile& operator=(ExclusiveOutputFile&&)      = delete;

    // Throws std::system_error if `path` already exists or cannot be created.
    void open(const std::string& path);
    // Flushes and closes; does nothing when no file is open.
    void               close();
    [[nodiscard]] bool isOpen() const noexcept { return file != nullptr; }

    // Formatted output into the file while it is open.
    std::ostream& stream() noexcept { return output; }

   private:
    // Forwards every write to the C handle, which does its own buffering.
    class HandleBuffer : public std::streambuf {
       public:
        void attach(std::FILE* target) noexcept { file = target; }

       protected:
        int_type        overflow(int_type character) override;
        std::streamsize xsputn(const char_type* text, std::streamsize count) override;
        int             sync() override;

       private:
        std::FILE* file = nullptr;
    };

    std::FILE*   file = nullptr;
    HandleBuffer buffer;
    std::ostream output;  // after `buffer`, which it writes through
};

#endif  // EXCLUSIVEOUTPUTFILE_HPP

#include "scripting/PythonInterpreter.hpp"

#include <pybind11/embed.h>

#include <cstddef>
#include <cstdint>
#include <logging/Logging.hpp>
#include <random>
#include <string>
#include <string_view>

namespace py = pybind11;

namespace {
constexpr unsigned kHalfSeedBits = 32U;

quill::Logger* logger() { return logging::logger(logging::Subsystem::Script); }

// Stands in for sys.stdout or sys.stderr and logs every complete line written to it.
class LogStream {
   public:
    explicit LogStream(bool isStderr) : isStderr(isStderr) {}

    void write(std::string_view text) {
        pending.append(text);
        for (std::size_t lineEnd = pending.find('\n'); lineEnd != std::string::npos;
             lineEnd             = pending.find('\n')) {
            log(std::string_view(pending).substr(0, lineEnd));
            pending.erase(0, lineEnd + 1);
        }
    }

    void flush() {
        if (!pending.empty()) {
            log(pending);
            pending.clear();
        }
    }

   private:
    void log(std::string_view line) const {
        if (isStderr) {
            LOG_WARNING(logger(), "{}", line);
        } else {
            LOG_INFO(logger(), "{}", line);
        }
    }

    bool        isStderr;
    std::string pending;
};

std::uint64_t freshSeed() {
    std::random_device device;
    return (std::uint64_t{device()} << kHalfSeedBits) | device();
}

// "3.12.3 (main, ...) [GCC ...]" -> "3.12.3"
std::string_view pythonVersion() {
    const std::string_view full = Py_GetVersion();
    return full.substr(0, full.find(' '));
}
}  // namespace

PYBIND11_EMBEDDED_MODULE(neuronide_log, module) {
    py::class_<LogStream>(module, "LogStream")
        .def(py::init<bool>(), py::arg("is_stderr"))
        .def("write",
             [](LogStream& stream, const py::str& text) {
                 stream.write(text.cast<std::string>());
                 return py::len(text);
             })
        .def("flush", &LogStream::flush);
}

PythonInterpreter::PythonInterpreter() {
    if (Py_IsInitialized() == 0) {
        py::initialize_interpreter(/*init_signal_handlers=*/false, 0, nullptr,
                                   /*add_program_dir_to_path=*/false);
        ownsInterpreter = true;
    }

    const py::module_ sys       = py::module_::import("sys");
    const py::object  logStream = py::module_::import("neuronide_log").attr("LogStream");
    sys.attr("stdout")          = logStream(false);
    sys.attr("stderr")          = logStream(true);

    seed = freshSeed();
    py::module_::import("random").attr("seed")(seed);
    LOG_INFO(logger(), "Python {}: random seeded with {}", pythonVersion(), seed);
}

// pybind11's own scoped_interpreter calls finalize_interpreter() from its destructor the same way.
// NOLINTNEXTLINE(bugprone-exception-escape)
PythonInterpreter::~PythonInterpreter() {
    if (ownsInterpreter) {
        py::finalize_interpreter();
    }
}

std::uint64_t PythonInterpreter::randomSeed() const { return seed; }

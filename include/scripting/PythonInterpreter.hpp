#ifndef PYTHONINTERPRETER_HPP
#define PYTHONINTERPRETER_HPP

#include <cstdint>

// The embedded Python that ScriptComponents run in. Start one before parsing a scene with scripts
// and keep it until that scene is destroyed. A process can run only one interpreter: if one is
// already running, this one uses it and leaves it running.
//
// Every PythonInterpreter seeds Python's `random` module afresh and logs the seed, so a run's
// draws from it can be repeated. Lines written to sys.stdout (print) and sys.stderr are logged
// under [script]. Python's signal handlers are not installed: SDL turns SIGINT and SIGTERM into
// SDL_QUIT, so a stopped run still closes its recording.
class PythonInterpreter {
   public:
    PythonInterpreter();
    ~PythonInterpreter();

    PythonInterpreter(const PythonInterpreter&)            = delete;
    PythonInterpreter& operator=(const PythonInterpreter&) = delete;
    PythonInterpreter(PythonInterpreter&&)                 = delete;
    PythonInterpreter& operator=(PythonInterpreter&&)      = delete;

    [[nodiscard]] std::uint64_t randomSeed() const;

   private:
    bool          ownsInterpreter = false;
    std::uint64_t seed            = 0;
};

#endif  // PYTHONINTERPRETER_HPP

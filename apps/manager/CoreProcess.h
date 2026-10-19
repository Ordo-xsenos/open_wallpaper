#pragma once
#include <QString>
#include <optional>

// Owns observation of a child, never its lifetime. Closing Manager leaves Core running.
class CoreProcess {
  public:
    CoreProcess() = default;
    ~CoreProcess();
    CoreProcess(const CoreProcess&) = delete;
    CoreProcess& operator=(const CoreProcess&) = delete;
    QString start(const QString& executable, bool diagnostic, const QString& logDirectory,
                  bool debug = false);
    std::optional<quint32> exitCode() const;

  private:
    void* process_ = nullptr;
};

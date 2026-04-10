// pybind11/Python must come before Qt headers to avoid "slots" macro clash
#undef slots
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#define slots Q_SLOTS

#include "PythonEngine.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <core/Backend.h>
#include <core/CanTrace.h>
#include <core/CanDb.h>
#include <core/CanDbMessage.h>
#include <core/CanDbSignal.h>
#include <core/MeasurementSetup.h>
#include <core/MeasurementNetwork.h>
#include <core/Log.h>
#include <driver/CanInterface.h>

namespace py = pybind11;
using namespace py::literals;

// ---------------------------------------------------------------------------
// Global pointer so the embedded module can reach the active engine
// ---------------------------------------------------------------------------
static PythonEngine *g_activeEngine = nullptr;

// ---------------------------------------------------------------------------
// Embedded "cangaroo" Python module
// ---------------------------------------------------------------------------
PYBIND11_EMBEDDED_MODULE(cangaroo, m)
{
    // --- CanMessage binding ---
    py::class_<CanMessage>(m, "Message")
        .def(py::init<>())
        .def(py::init<uint32_t>())
        .def_property("id", &CanMessage::getId, &CanMessage::setId)
        .def_property("dlc", &CanMessage::getLength, &CanMessage::setLength)
        .def_property("extended", &CanMessage::isExtended, &CanMessage::setExtended)
        .def_property("fd", &CanMessage::isFD, &CanMessage::setFD)
        .def_property("rtr", &CanMessage::isRTR, &CanMessage::setRTR)
        .def_property("brs", &CanMessage::isBRS, &CanMessage::setBRS)
        .def_property_readonly("interface_id", &CanMessage::getInterfaceId)
        .def_property_readonly("timestamp", &CanMessage::getFloatTimestamp)
        .def_property_readonly("is_rx", &CanMessage::isRX)
        .def("get_byte", &CanMessage::getByte)
        .def("set_byte", &CanMessage::setByte)
        .def("get_data", [](const CanMessage &msg) -> py::bytes
        {
            std::string data(msg.getLength(), '\0');
            for (int i = 0; i < msg.getLength(); i++)
            {
                data[i] = static_cast<char>(msg.getByte(i));
            }
            return py::bytes(data);
        })
        .def("set_data", [](CanMessage &msg, py::bytes data)
        {
            std::string s = data;
            msg.setLength(static_cast<uint8_t>(std::min<size_t>(s.size(), 64)));
            for (size_t i = 0; i < s.size() && i < 64; i++)
            {
                msg.setByte(i, static_cast<uint8_t>(s[i]));
            }
        })
        .def("__repr__", [](const CanMessage &msg) -> std::string
        {
            return "<cangaroo.Message id=0x"
                   + msg.getIdString().toStdString()
                   + " dlc=" + std::to_string(msg.getLength())
                   + " data=" + msg.getDataHexString().toStdString() + ">";
        });

    // --- Module-level functions ---
    m.def("send", [](CanMessage &msg, uint16_t interface_id)
    {
        if (!g_activeEngine) { return; }
        msg.setInterfaceId(interface_id);
        CanInterface *intf = g_activeEngine->backend().getInterfaceById(interface_id);
        if (intf)
        {
            intf->sendMessage(msg);
        }
    }, py::arg("msg"), py::arg("interface_id") = 0);

    m.def("get_trace", [](int count) -> py::list
    {
        py::list result;
        if (!g_activeEngine) { return result; }

        CanTrace *trace = g_activeEngine->backend().getTrace();
        int total = static_cast<int>(trace->size());
        int start = (count > 0 && count < total) ? total - count : 0;
        for (int i = start; i < total; i++)
        {
            result.append(trace->getMessage(i));
        }
        return result;
    }, py::arg("count") = 0);

    m.def("log", [](const std::string &text)
    {
        if (g_activeEngine)
        {
            log_info(QString::fromStdString(text));
        }
    });

    m.def("log_info", [](const std::string &text)
    {
        if (g_activeEngine)
        {
            log_info(QString::fromStdString(text));
        }
    });

    m.def("log_warning", [](const std::string &text)
    {
        if (g_activeEngine)
        {
            log_warning(QString::fromStdString(text));
        }
    });

    m.def("log_error", [](const std::string &text)
    {
        if (g_activeEngine)
        {
            log_error(QString::fromStdString(text));
        }
    });

    m.def("interface_name", [](uint16_t id) -> std::string
    {
        if (!g_activeEngine) { return ""; }
        return g_activeEngine->backend().getInterfaceName(id).toStdString();
    });

    m.def("receive", [](double timeout_sec) -> py::list
    {
        py::list result;
        if (!g_activeEngine) { return result; }

        unsigned long wait_ms = static_cast<unsigned long>(timeout_sec * 1000);

        // Release the GIL while waiting so other threads aren't blocked
        {
            py::gil_scoped_release release;

            QMutexLocker lck(&g_activeEngine->msgQueueMutex());
            QQueue<CanMessage> &q = g_activeEngine->msgQueue();

            // If queue is empty, wait for the condition variable
            if (q.isEmpty() && !g_activeEngine->stopRequested())
            {
                g_activeEngine->msgQueueCondition().wait(
                    &g_activeEngine->msgQueueMutex(), wait_ms);
            }
        }

        // Re-acquire GIL and drain the queue
        {
            QMutexLocker lck(&g_activeEngine->msgQueueMutex());
            QQueue<CanMessage> &q = g_activeEngine->msgQueue();
            while (!q.isEmpty())
            {
                result.append(q.dequeue());
            }
        }

        return result;
    }, py::arg("timeout") = 1.0);

    m.def("interfaces", []() -> py::list
    {
        py::list result;
        if (!g_activeEngine) { return result; }
        for (CanInterfaceId id : g_activeEngine->backend().getInterfaceList())
        {
            py::dict d;
            d["id"] = id;
            d["name"] = g_activeEngine->backend().getInterfaceName(id).toStdString();
            result.append(d);
        }
        return result;
    });

    // --- DBC / Database access ---

    // Decode all signals from a message using loaded DBCs.
    // Returns a dict: { "message_name": str, "signals": { name: { "value": float, "raw": int, "unit": str } } }
    // Returns None if no DBC definition is found.
    m.def("decode", [](const CanMessage &msg) -> py::object
    {
        if (!g_activeEngine) { return py::none(); }

        CanDbMessage *dbMsg = g_activeEngine->backend().findDbMessage(msg);
        if (!dbMsg) { return py::none(); }

        py::dict sigDict;
        for (CanDbSignal *sig : dbMsg->getSignals())
        {
            if (!sig->isPresentInMessage(msg)) { continue; }

            uint64_t raw = sig->extractRawDataFromMessage(msg);
            double phys = sig->convertRawValueToPhysical(raw);

            py::dict sigInfo;
            sigInfo["value"] = phys;
            sigInfo["raw"] = raw;
            sigInfo["unit"] = sig->getUnit().toStdString();
            sigInfo["min"] = sig->getMinimumValue();
            sigInfo["max"] = sig->getMaximumValue();

            QString valueName = sig->getValueName(raw);
            if (!valueName.isEmpty())
            {
                sigInfo["value_name"] = valueName.toStdString();
            }

            sigDict[py::cast(sig->name().toStdString())] = sigInfo;
        }

        py::dict result;
        result["message"] = dbMsg->getName().toStdString();
        result["id"] = dbMsg->getRaw_id();
        result["signals"] = sigDict;

        CanDbNode *sender = dbMsg->getSender();
        if (sender)
        {
            result["sender"] = sender->name().toStdString();
        }

        return result;
    }, py::arg("msg"));

    // Look up the DBC message definition for a CAN message.
    // Returns a dict with message metadata and signal definitions, or None.
    m.def("lookup", [](const CanMessage &msg) -> py::object
    {
        if (!g_activeEngine) { return py::none(); }

        CanDbMessage *dbMsg = g_activeEngine->backend().findDbMessage(msg);
        if (!dbMsg) { return py::none(); }

        py::list sigList;
        for (CanDbSignal *sig : dbMsg->getSignals())
        {
            py::dict s;
            s["name"] = sig->name().toStdString();
            s["start_bit"] = sig->startBit();
            s["length"] = sig->length();
            s["is_big_endian"] = sig->isBigEndian();
            s["is_unsigned"] = sig->isUnsigned();
            s["factor"] = sig->getFactor();
            s["offset"] = sig->getOffset();
            s["min"] = sig->getMinimumValue();
            s["max"] = sig->getMaximumValue();
            s["unit"] = sig->getUnit().toStdString();
            s["comment"] = sig->comment().toStdString();
            if (sig->isMuxed())
            {
                s["mux_value"] = sig->getMuxValue();
            }
            if (sig->isMuxer())
            {
                s["is_muxer"] = true;
            }
            sigList.append(s);
        }

        py::dict result;
        result["message"] = dbMsg->getName().toStdString();
        result["id"] = dbMsg->getRaw_id();
        result["dlc"] = dbMsg->getDlc();
        result["comment"] = dbMsg->getComment().toStdString();
        result["signals"] = sigList;

        CanDbNode *sender = dbMsg->getSender();
        if (sender)
        {
            result["sender"] = sender->name().toStdString();
        }

        return result;
    }, py::arg("msg"));

    // List all loaded databases with their messages.
    m.def("databases", []() -> py::list
    {
        py::list result;
        if (!g_activeEngine) { return result; }

        MeasurementSetup &setup = g_activeEngine->backend().getSetup();
        for (MeasurementNetwork *net : setup.getNetworks())
        {
            for (const pCanDb &db : net->_canDbs)
            {
                py::dict dbInfo;
                dbInfo["file"] = db->getFileName().toStdString();
                dbInfo["path"] = db->getPath().toStdString();
                dbInfo["network"] = net->name().toStdString();

                py::list msgs;
                for (auto it = db->getMessageList().begin(); it != db->getMessageList().end(); ++it)
                {
                    CanDbMessage *m = it.value();
                    py::dict mInfo;
                    mInfo["name"] = m->getName().toStdString();
                    mInfo["id"] = m->getRaw_id();
                    mInfo["dlc"] = m->getDlc();

                    py::list sigNames;
                    for (CanDbSignal *sig : m->getSignals())
                    {
                        sigNames.append(sig->name().toStdString());
                    }
                    mInfo["signals"] = sigNames;
                    msgs.append(mInfo);
                }
                dbInfo["messages"] = msgs;
                result.append(dbInfo);
            }
        }
        return result;
    });
}


// ---------------------------------------------------------------------------
// PythonEngine implementation
// ---------------------------------------------------------------------------

#ifdef Q_OS_WIN
// Returns the Python home directory to use, or empty string if not found.
// Checks for a bundled stdlib next to the exe first, then falls back to PATH.
static QString findPythonHome()
{
    // 1. Bundled stdlib: <appdir>/lib/python3.x/ (our CI layout)
    {
        QString appDir = QCoreApplication::applicationDirPath();
        QString pyDir = QString("%1/lib/python%2.%3")
                            .arg(appDir)
                            .arg(PY_MAJOR_VERSION)
                            .arg(PY_MINOR_VERSION);
        if (QDir(pyDir).exists())
        {
            return appDir;
        }
    }

    // 2. Python executable in PATH → derive prefix from its location
    for (const char *name : {"python3", "python"})
    {
        QString exe = QStandardPaths::findExecutable(QString::fromLatin1(name));
        if (exe.isEmpty())
            continue;

        QDir dir = QFileInfo(exe).absoluteDir();
        // MSYS2/Unix layout: …/mingw64/bin/python3.exe → home = …/mingw64
        // Windows layout:    C:\Python313\python.exe  → home = C:\Python313
        if (dir.dirName().compare("bin", Qt::CaseInsensitive) == 0 ||
            dir.dirName().compare("Scripts", Qt::CaseInsensitive) == 0)
        {
            dir.cdUp();
        }
        return dir.absolutePath();
    }

    return {};
}
#endif // Q_OS_WIN

// Holds the interpreter lifetime. Must be constructed on the main thread.
// After construction the GIL is released so worker threads can use
// PyGILState_Ensure/Release to acquire it safely on any thread (including
// on Windows where Py_Initialize must run on the main thread).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
struct PythonEngine::PyInterpreterHolder
{
    // _env is declared before guard so it initializes first,
    // setting PYTHONHOME before Py_Initialize is called.
    bool _env{ prepareEnvironment() };
    py::scoped_interpreter guard{};
    PyThreadState *savedState = nullptr;

    static bool prepareEnvironment()
    {
#ifdef Q_OS_WIN
        if (qEnvironmentVariableIsEmpty("PYTHONHOME"))
        {
            QString home = findPythonHome();
            if (!home.isEmpty())
            {
                qputenv("PYTHONHOME", home.toLocal8Bit());
            }
        }
#endif
        return true;
    }

    PyInterpreterHolder()
    {
        savedState = PyEval_SaveThread();
    }

    ~PyInterpreterHolder()
    {
        PyEval_RestoreThread(savedState);
        // guard destructor calls Py_Finalize
    }
};
#pragma GCC diagnostic pop

PythonEngine::PythonEngine(Backend &backend, QObject *parent)
    : QObject(parent)
    , _backend(backend)
{
    try
    {
        _pyInterp = std::make_unique<PyInterpreterHolder>();
    }
    catch (const std::exception &e)
    {
        _initError = QString::fromStdString(e.what());
    }
}

PythonEngine::~PythonEngine()
{
    stopScript(); // join thread before interpreter is finalized
}

void PythonEngine::runScript(const QString &code)
{
    if (!_pyInterp)
    {
        emit scriptError(_initError.isEmpty()
            ? "Python interpreter failed to initialize."
            : _initError);
        return;
    }

    if (_running)
    {
        return;
    }

    _stopRequested = false;
    _running = true;

    {
        QMutexLocker lck(&_msgQueueMutex);
        _msgQueue.clear();
    }

    emit scriptStarted();

    // Join any previous thread before launching a new one
    if (_workerThread && _workerThread->joinable())
    {
        _workerThread->join();
    }

    // Launch a plain std::thread — no Qt event loop needed
    _workerThread = std::make_unique<std::thread>(&PythonEngine::workerFunc, this, code.toStdString());
}

void PythonEngine::stopScript()
{
    _stopRequested = true;
    // Wake receive() if it's waiting on the condition variable
    _msgQueueCondition.wakeAll();

    if (!_workerThread || !_workerThread->joinable())
    {
        return;
    }

    // Wait up to 5 seconds for the thread to finish
    for (int i = 0; i < 50 && _running; i++)
    {
        _msgQueueCondition.wakeAll();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (_workerThread->joinable())
    {
        if (_running)
        {
            // Thread didn't stop in time — detach to avoid blocking the UI forever
            _workerThread->detach();
        }
        else
        {
            _workerThread->join();
        }
    }
    _workerThread.reset();
}

bool PythonEngine::isRunning() const
{
    return _running;
}

void PythonEngine::enqueueMessage(const CanMessage &msg)
{
    if (!_running) { return; }
    QMutexLocker lck(&_msgQueueMutex);
    if (_msgQueue.size() < 10000)
    {
        _msgQueue.enqueue(msg);
        _msgQueueCondition.wakeOne();
    }
}

void PythonEngine::workerFunc(std::string code)
{
    g_activeEngine = this;

    // The scoped_interpreter MUST outlive any catch blocks that access
    // Python error state (e.g. py::error_already_set::what()), otherwise
    // Py_Finalize runs during stack unwinding and the catch block crashes.
    // The interpreter was already initialized on the main thread.
    // Acquire the GIL for this worker thread — works on all platforms including Windows.
    PyGILState_STATE gstate = PyGILState_Ensure();

    try
    {
        // Inject helpers into globals so they persist across all py::exec calls
        auto globals = py::globals();

        globals["_cangaroo_output"] = py::cpp_function(
            [this](const std::string &text, bool is_err)
            {
                QString qtext = QString::fromStdString(text);
                if (is_err)
                {
                    emit scriptError(qtext);
                }
                else
                {
                    emit scriptOutput(qtext);
                }
            });

        globals["_cangaroo_stop_check"] = py::cpp_function(
            [this]() -> bool
            {
                return _stopRequested.load();
            });

        // Redirect stdout/stderr
        py::exec(R"(
import sys

class _SignalWriter:
    def __init__(self, is_err):
        self._is_err = is_err
    def write(self, text):
        if text:
            _cangaroo_output(text, self._is_err)
    def flush(self):
        pass

sys.stdout = _SignalWriter(False)
sys.stderr = _SignalWriter(True)
)");

        // Install trace function for stoppability
        py::exec(R"(
def _cangaroo_trace(frame, event, arg):
    if _cangaroo_stop_check():
        raise KeyboardInterrupt("Script stopped by user")
    return _cangaroo_trace

sys.settrace(_cangaroo_trace)
)");

        try
        {
            // Run the user script
            py::exec(code);
        }
        catch (py::error_already_set &e)
        {
            // Must format the error while the interpreter is still alive
            QString err = QString::fromStdString(e.what());
            if (!err.contains("KeyboardInterrupt"))
            {
                emit scriptError(err + "\n");
            }
        }
    }
    catch (std::exception &e)
    {
        emit scriptError(QString::fromStdString(e.what()) + "\n");
    }

    PyGILState_Release(gstate);

    g_activeEngine = nullptr;
    _running = false;
    emit scriptFinished();
}

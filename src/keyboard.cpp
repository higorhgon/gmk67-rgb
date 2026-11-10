#include "keyboard.h"

#include <QDir>
#include <QFile>
#include <QThread>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {
constexpr int Header = 0x04;
constexpr int CmdEnd = 0x02, CmdRead = 0x12, CmdWrite = 0x13, CmdBegin = 0x18, CmdLive = 0x20, CmdApply = 0xF0;
// The vendor driver waits this long between packets of a saved write.
constexpr unsigned long PacketPauseMs = 15;
}  // namespace

Keyboard::~Keyboard() { close(); }

QString Keyboard::find(const QString &sysRoot, const QString &devRoot) {
    const QDir dir(sysRoot);
    for (const QString &name : dir.entryList({"hidraw*"}, QDir::Dirs | QDir::System | QDir::NoDotAndDotDot, QDir::Name)) {
        QFile uevent(dir.filePath(name + "/device/uevent"));
        QFile descriptor(dir.filePath(name + "/device/report_descriptor"));
        if (!uevent.open(QIODevice::ReadOnly) || !descriptor.open(QIODevice::ReadOnly))
            continue;
        if (!QString::fromUtf8(uevent.readAll()).contains("HID_ID=0003:000005AC:0000024F", Qt::CaseInsensitive))
            continue;
        // The control interface is the keyboard collection (05 01 09 06) with
        // a 64-byte feature report (95 40 75 08 b1 02); a real Apple keyboard
        // with the same IDs doesn't have that report.
        const QByteArray desc = descriptor.readAll();
        if (desc.startsWith(QByteArray::fromHex("05010906")) && desc.contains(QByteArray::fromHex("95407508b102")))
            return devRoot + "/" + name;
    }
    return {};
}

bool Keyboard::open(const QString &path) {
    close();
    m_status = Ready;
    m_error.clear();
    const QString device = path.isEmpty() ? find() : path;
    if (device.isEmpty())
        return fail(NotFound, tr("Connect the GMK67 with its USB cable. The dongle and Bluetooth can't change the lights."));
    m_fd = ::open(QFile::encodeName(device).constData(), O_RDWR | O_CLOEXEC);
    if (m_fd < 0) {
        const int error = errno;
        if (error == EACCES || error == EPERM)
            return fail(NoPermission, tr("No permission to use the keyboard. Install the udev rule below once."));
        return fail(Failed, tr("Can't open the keyboard: %1").arg(QString::fromLocal8Bit(strerror(error))));
    }
    return true;
}

void Keyboard::close() {
    if (m_fd >= 0)
        ::close(m_fd);
    m_fd = -1;
}

bool Keyboard::fail(Status status, const QString &message) {
    m_status = status;
    m_error = message;
    return false;
}

bool Keyboard::send(const QByteArray &data, bool pause) {
    if (pause)
        QThread::msleep(PacketPauseMs);
    unsigned char buffer[Protocol::PacketSize + 1] = {};  // byte 0 is report ID 0
    std::memcpy(buffer + 1, data.constData(), std::min<qsizetype>(data.size(), Protocol::PacketSize));
    if (::ioctl(m_fd, HIDIOCSFEATURE(sizeof buffer), buffer) < 0) {
        const int error = errno;
        close();
        return fail(Failed, tr("The keyboard stopped answering: %1").arg(QString::fromLocal8Bit(strerror(error))));
    }
    return true;
}

bool Keyboard::read(QByteArray *reply, bool pause) {
    if (pause)
        QThread::msleep(PacketPauseMs);
    unsigned char buffer[Protocol::PacketSize + 1] = {};
    if (::ioctl(m_fd, HIDIOCGFEATURE(sizeof buffer), buffer) < 0) {
        const int error = errno;
        close();
        return fail(Failed, tr("The keyboard stopped answering: %1").arg(QString::fromLocal8Bit(strerror(error))));
    }
    if (reply)
        *reply = QByteArray(reinterpret_cast<const char *>(buffer + 1), Protocol::PacketSize);
    return true;
}

// Every saved-write command is acknowledged with "04 <cmd> 00 01".
bool Keyboard::command(int cmd, int pages) {
    QByteArray packet(Protocol::PacketSize, '\0');
    packet[0] = char(Header);
    packet[1] = char(cmd);
    packet[8] = char(pages);
    QByteArray reply;
    if (!send(packet, true) || !read(&reply, true))
        return false;
    if (quint8(reply[0]) != Header || quint8(reply[1]) != cmd || reply[3] != 1)
        return fail(Failed, tr("The keyboard didn't acknowledge command %1.").arg(cmd, 2, 16, QChar('0')));
    return true;
}

bool Keyboard::setEffect(const Protocol::Effect &effect) {
    if (!isOpen() && !open())
        return false;
    return command(CmdBegin) && command(CmdWrite, 1) && send(effect.page(), true)
        && command(CmdEnd) && command(CmdApply);
}

bool Keyboard::readEffect(Protocol::Effect *effect) {
    if (!isOpen() && !open())
        return false;
    QByteArray packet(Protocol::PacketSize, '\0');
    packet[0] = char(Header);
    packet[1] = char(CmdRead);
    packet[8] = 1;
    QByteArray page;
    if (!send(packet, true))
        return false;
    // It usually answers with the page, but an ack can come first, this one
    // or one left over from an earlier write ("04 <cmd> 00 01"). Only a page
    // ends in AA 55; 04 alone can't tell, as it is also Glittering's id.
    auto isPage = [](const QByteArray &b) {
        return b.size() >= 16 && quint8(b[14]) == 0xAA && quint8(b[15]) == 0x55;
    };
    for (int attempt = 0; attempt < 3 && !isPage(page); ++attempt)
        if (!read(&page, true))
            return false;
    if (!isPage(page)) {
        command(CmdEnd);
        return fail(Failed, tr("The keyboard didn't send its current effect."));
    }
    if (!command(CmdEnd))
        return false;
    *effect = Protocol::Effect::fromPage(page);
    return true;
}

bool Keyboard::sendLive(const QList<QColor> &colors) {
    if (!isOpen() && !open())
        return false;
    // As the vendor app's live mode: 04 02 and 04 20 with their replies read,
    // then the eight pages without. No pauses, so animations stay smooth.
    const QByteArray frame = Protocol::liveFrame(colors);
    QByteArray sync(Protocol::PacketSize, '\0');
    sync[0] = char(Header);
    sync[1] = char(CmdEnd);
    QByteArray live = sync;
    live[1] = char(CmdLive);
    live[8] = char(Protocol::LivePages);
    if (!send(sync, false) || !read(nullptr, false) || !send(live, false) || !read(nullptr, false))
        return false;
    for (int page = 0; page < Protocol::LivePages; ++page)
        if (!send(frame.mid(page * Protocol::PacketSize, Protocol::PacketSize), false))
            return false;
    return true;
}

#pragma once

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>

// 红绿灯状态（和视觉端保持一致）
enum class LightState : uint8_t {
    Red     = 0,
    Green   = 1,
    Yellow  = 2,
    Unknown = 3
};

// 极简 Linux 串口封装（termios）
class SerialPort {
public:
    SerialPort() = default;
    ~SerialPort() { closePort(); }

    bool openPort(const std::string& device, int baud = 115200)
    {
        closePort();

        fd_ = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
        if (fd_ < 0) {
            std::cerr << "open " << device << " failed: " << strerror(errno) << "\n";
            return false;
        }

        struct termios tty {};
        if (tcgetattr(fd_, &tty) != 0) {
            std::cerr << "tcgetattr failed: " << strerror(errno) << "\n";
            closePort();
            return false;
        }

        speed_t speed = toSpeed(baud);
        cfsetispeed(&tty, speed);
        cfsetospeed(&tty, speed);

        // 8N1：8 数据位、无校验、1 停止位
        tty.c_cflag |= (CLOCAL | CREAD);
        tty.c_cflag &= ~PARENB;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= CS8;
        tty.c_cflag &= ~CRTSCTS;          // 无硬件流控

        // 原始模式
        tty.c_iflag &= ~(IXON | IXOFF | IXANY);
        tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        tty.c_oflag &= ~OPOST;

        tty.c_cc[VMIN]  = 0;
        tty.c_cc[VTIME] = 1;              // 100ms 超时

        if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
            std::cerr << "tcsetattr failed: " << strerror(errno) << "\n";
            closePort();
            return false;
        }

        tcflush(fd_, TCIOFLUSH);
        return true;
    }

    void closePort()
    {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    bool isOpen() const { return fd_ >= 0; }

    // 写满 len 个字节（处理部分写）
    bool writeBytes(const uint8_t* data, size_t len)
    {
        if (fd_ < 0) return false;

        size_t written = 0;
        while (written < len) {
            ssize_t n = ::write(fd_, data + written, len - written);
            if (n < 0) {
                if (errno == EINTR) continue;
                std::cerr << "write failed: " << strerror(errno) << "\n";
                return false;
            }
            if (n == 0) return false;
            written += static_cast<size_t>(n);
        }
        return true;
    }

private:
    static speed_t toSpeed(int baud)
    {
        switch (baud) {
            case 9600:   return B9600;
            case 19200:  return B19200;
            case 38400:  return B38400;
            case 57600:  return B57600;
            case 115200: return B115200;
            case 230400: return B230400;
            case 460800: return B460800;
            case 921600: return B921600;
            default:     return B115200;
        }
    }

    int fd_ = -1;
};

// 红绿灯帧：AA 55 02 state checksum 0D
// state 最高位 = 健康标志（0=健康，1=不健康）
inline bool sendTrafficLight(SerialPort& port, LightState s, bool healthy = true)
{
    uint8_t buf[6] = {
        0xAA, 0x55, 0x02,
        static_cast<uint8_t>(s),
        0x00,
        0x0D
    };

    if (!healthy) {
        buf[3] |= 0x80;
    }

    buf[4] = static_cast<uint8_t>(buf[0] ^ buf[1] ^ buf[2] ^ buf[3]);
    return port.writeBytes(buf, sizeof(buf));
}

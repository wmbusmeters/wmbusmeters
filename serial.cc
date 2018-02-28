// Copyright (c) 2017 Fredrik Öhrström
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include"util.h"
#include"serial.h"

#include <algorithm>
#include <fcntl.h>
#include <functional>
#include <memory.h>
#include <pthread.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

static int openSerialTTY(const char *tty, int baud_rate);

struct SerialDeviceTTY;

struct SerialCommunicationManagerImp : public SerialCommunicationManager {
    SerialCommunicationManagerImp();
    SerialDevice *createSerialDeviceTTY(string dev, int baud_rate);
    void listenTo(SerialDevice *sd, function<void()> cb);
    void stop();
    void waitForStop();
    bool isRunning();

    void opened(SerialDeviceTTY *sd);
    void closed(SerialDeviceTTY *sd);

private:
    void *eventLoop();
    static void *startLoop(void *);

    bool running_;
    pthread_t thread_;
    int max_fd_;
    vector<SerialDevice*> devices_;
};

struct SerialDeviceImp : public SerialDevice {
    private:
    function<void()> on_data_;

    friend struct SerialCommunicationManagerImp;
};

struct SerialDeviceTTY : public SerialDeviceImp {
    SerialDeviceTTY(string device, int baud_rate, SerialCommunicationManagerImp *manager);

    bool open(bool fail_if_not_ok);
    void close();
    bool send(vector<uchar> &data);
    int receive(vector<uchar> *data);
    int fd() { return fd_; }
    SerialCommunicationManager *manager() { return manager_; }

    private:

    string device_;
    int baud_rate_;
    int fd_;
    pthread_mutex_t write_lock_ = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t read_lock_ = PTHREAD_MUTEX_INITIALIZER;
    SerialCommunicationManagerImp *manager_;
};

SerialDeviceTTY::SerialDeviceTTY(string device, int baud_rate,
                                 SerialCommunicationManagerImp *manager) {
    device_ = device;
    baud_rate_ = baud_rate;
    manager_ = manager;
}

bool SerialDeviceTTY::open(bool fail_if_not_ok)
{
    bool ok = checkCharacterDeviceExists(device_.c_str(), fail_if_not_ok);
    if (!ok) return false;
    fd_ = openSerialTTY(device_.c_str(), baud_rate_);
    if (fd_ == -1) {
        if (fail_if_not_ok) {
            error("Could not open %s with %d baud N81\n", device_.c_str(), baud_rate_);
        } else {
            return false;
        }
    }
    manager_->opened(this);
    verbose("(serial) opened %s\n", device_.c_str());
    return true;
}

void SerialDeviceTTY::close()
{
    ::flock(fd_, LOCK_UN);
    ::close(fd_);
    fd_ = -1;
    manager_->closed(this);
    verbose("(serial) closed %s\n", device_.c_str());
}

bool SerialDeviceTTY::send(vector<uchar> &data)
{
    if (data.size() == 0) return true;

    pthread_mutex_lock(&write_lock_);

    bool rc = true;
    int n = data.size();
    int written = 0;
    while (true) {
        int nw = write(fd_, &data[written], n-written);
        if (nw > 0) written += nw;
        if (nw < 0) {
            if (errno==EINTR) continue;
            rc = false;
            goto end;
        }
        if (written == n) break;
    }

    if (isDebugEnabled()) {
        string msg = bin2hex(data);
        debug("(serial %s) sent \"%s\"\n", device_.c_str(), msg.c_str());
    }

    end:
    pthread_mutex_unlock(&write_lock_);
    return rc;
}

int SerialDeviceTTY::receive(vector<uchar> *data)
{
    pthread_mutex_lock(&read_lock_);

    data->clear();
    int available = 0;
    int num_read = 0;

    ioctl(fd_, FIONREAD, &available);
    if (!available) goto end;

    data->resize(available);

    while (true) {
        int nr = read(fd_, &((*data)[num_read]), available-num_read);
        if (nr > 0) num_read += nr;
        if (nr < 0) {
            if (errno==EINTR) continue;
            goto end;
        }
        if (num_read == available) break;
    }

    if (isDebugEnabled()) {
        string msg = bin2hex(*data);
        debug("(serial %s) received \"%s\"\n", device_.c_str(), msg.c_str());
    }
    end:
    pthread_mutex_unlock(&read_lock_);
    return num_read;
}

SerialCommunicationManagerImp::SerialCommunicationManagerImp()
{
    running_ = true;
    max_fd_ = 0;
    pthread_create(&thread_, NULL, startLoop, this);
    //running_ = (rc == 0);
}

void *SerialCommunicationManagerImp::startLoop(void *a) {
    auto t = (SerialCommunicationManagerImp*)a;
    return t->eventLoop();
}

SerialDevice *SerialCommunicationManagerImp::createSerialDeviceTTY(string device, int baud_rate) {
    SerialDevice *sd = new SerialDeviceTTY(device, baud_rate, this);
    return sd;
}

void SerialCommunicationManagerImp::listenTo(SerialDevice *sd, function<void()> cb) {
    SerialDeviceImp *si = dynamic_cast<SerialDeviceImp*>(sd);
    si->on_data_ = cb;
}

void SerialCommunicationManagerImp::stop()
{
    running_ = false;
}

void SerialCommunicationManagerImp::waitForStop()
{
    while (running_) { usleep(1000*1000);}
    pthread_kill(thread_, SIGUSR1);
    pthread_join(thread_, NULL);
    for (SerialDevice *d : devices_) {
        d->close();
    }
}

bool SerialCommunicationManagerImp::isRunning()
{
    return running_;
}

void SerialCommunicationManagerImp::opened(SerialDeviceTTY *sd) {
    max_fd_ = max(sd->fd(), max_fd_);
    devices_.push_back(sd);
    pthread_kill(thread_, SIGUSR1);
}

void SerialCommunicationManagerImp::closed(SerialDeviceTTY *sd) {
    auto p = find(devices_.begin(), devices_.end(), sd);
    if (p != devices_.end()) {
        devices_.erase(p);
    }
    max_fd_ = 0;
    for (SerialDevice *d : devices_) {
        if (d->fd() > max_fd_) {
            max_fd_ = d->fd();
        }
    }
}

void *SerialCommunicationManagerImp::eventLoop() {
    fd_set readfds;
    while (running_) {
        FD_ZERO(&readfds);

        for (SerialDevice *d : devices_) {
            FD_SET(d->fd(), &readfds);
        }
        int activity = select(max_fd_+1 , &readfds , NULL , NULL , NULL);

        if (!running_) break;
        if (activity < 0 && errno!=EINTR) {
            error("(serial) internal error after select! errno=%d\n", errno);
        }
        if (activity > 0) {
            for (SerialDevice *d : devices_) {
                if (FD_ISSET(d->fd(), &readfds)) {
                    SerialDeviceImp *si = dynamic_cast<SerialDeviceImp*>(d);
                    if (si->on_data_) si->on_data_();
                }
            }
        }
    }
    verbose("(serial) event loop stopped!\n");
    return NULL;
}

SerialCommunicationManager *createSerialCommunicationManager()
{
    return new SerialCommunicationManagerImp();
}

static int openSerialTTY(const char *tty, int baud_rate)
{
    int rc = 0;
    speed_t speed = 0;
    struct termios tios;

    int fd = open(tty, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
        usleep(1000*1000);
        fd = open(tty, O_RDWR | O_NOCTTY | O_NDELAY);
        if (fd == -1) goto err;
    }
    rc = flock(fd, LOCK_EX | LOCK_NB);
    if (rc == -1) {
        // It is already locked by another wmbusmeter process.
        warning("Device %s is already in use and locked.\n", tty);
        goto err;
    }

    switch (baud_rate) {
    case 9600:   speed = B9600;  break;
    case 19200:  speed = B19200; break;
    case 38400:  speed = B38400; break;
    case 57600:  speed = B57600; break;
    case 115200: speed = B115200;break;
    default:
        goto err;
    }

    memset(&tios, 0, sizeof(tios));

    rc = cfsetispeed(&tios, speed);
    if (rc < 0) goto err;
    rc = cfsetospeed(&tios, speed);
    if (rc < 0) goto err;


    tios.c_cflag |= (CREAD | CLOCAL);
    tios.c_cflag &= ~CSIZE;
    tios.c_cflag |= CS8;
    tios.c_cflag &=~ CSTOPB;
    tios.c_cflag &=~ PARENB;

    tios.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tios.c_iflag &= ~INPCK;

    tios.c_iflag &= ~(IXON | IXOFF | IXANY);

    tios.c_oflag &=~ OPOST;
    tios.c_cc[VMIN] = 0;
    tios.c_cc[VTIME] = 0;

    rc = tcsetattr(fd, TCSANOW, &tios);
    if (rc < 0) goto err;

    return fd;

err:
    if (fd != -1) close(fd);
    return -1;
}

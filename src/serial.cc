/*
 Copyright (C) 2017-2019 Fredrik Öhrström

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include"util.h"
#include"serial.h"
#include"shell.h"

#include <algorithm>
#include <fcntl.h>
#include <functional>
#include <memory.h>
#include <pthread.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

static int openSerialTTY(const char *tty, int baud_rate);

struct SerialDeviceImp;
struct SerialDeviceTTY;
struct SerialDeviceCommand;
struct SerialDeviceSimulator;

struct SerialCommunicationManagerImp : public SerialCommunicationManager {
    SerialCommunicationManagerImp(time_t exit_after_seconds);
    ~SerialCommunicationManagerImp() { }

    unique_ptr<SerialDevice> createSerialDeviceTTY(string dev, int baud_rate);
    unique_ptr<SerialDevice> createSerialDeviceCommand(string command, vector<string> args, vector<string> envs,
                                                       function<void()> on_exit);
    unique_ptr<SerialDevice> createSerialDeviceSimulator();

    void listenTo(SerialDevice *sd, function<void()> cb);
    void stop();
    void waitForStop();
    bool isRunning();

    void opened(SerialDeviceImp *sd);
    void closed(SerialDeviceImp *sd);

private:
    void *eventLoop();
    static void *startLoop(void *);

    bool running_;
    pthread_t thread_;
    int max_fd_;
    vector<SerialDevice*> devices_;
    time_t start_time_;
    time_t exit_after_seconds_;
};

struct SerialDeviceImp : public SerialDevice {

    int fd() { return fd_; }
    bool working() { return true; }

    protected:

    function<void()> on_data_;
    int fd_;

    friend struct SerialCommunicationManagerImp;
};

struct SerialDeviceTTY : public SerialDeviceImp {
    SerialDeviceTTY(string device, int baud_rate, SerialCommunicationManagerImp *manager);
    ~SerialDeviceTTY();

    bool open(bool fail_if_not_ok);
    void close();
    bool send(vector<uchar> &data);
    int receive(vector<uchar> *data);
    SerialCommunicationManager *manager() { return manager_; }

    private:

    string device_;
    int baud_rate_ {};
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

SerialDeviceTTY::~SerialDeviceTTY()
{
    close();
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
    verbose("(serialtty) opened %s\n", device_.c_str());
    return true;
}

void SerialDeviceTTY::close()
{
    if (fd_ == -1) return;
    ::flock(fd_, LOCK_UN);
    ::close(fd_);
    fd_ = -1;
    manager_->closed(this);
    verbose("(serialtty) closed %s\n", device_.c_str());
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

struct SerialDeviceCommand : public SerialDeviceImp
{
    SerialDeviceCommand(string command, vector<string> args, vector<string> envs,
                        SerialCommunicationManagerImp *manager,
                        function<void()> on_exit);
    ~SerialDeviceCommand();

    bool open(bool fail_if_not_ok);
    void close();
    bool send(vector<uchar> &data);
    int receive(vector<uchar> *data);
    int fd() { return fd_; }
    bool working();

    SerialCommunicationManager *manager() { return manager_; }

    private:

    string command_;
    int fd_ {};
    int pid_ {};
    vector<string> args_;
    vector<string> envs_;

    pthread_mutex_t write_lock_ = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t read_lock_ = PTHREAD_MUTEX_INITIALIZER;
    SerialCommunicationManagerImp *manager_;
    function<void()> on_exit_;
};

SerialDeviceCommand::SerialDeviceCommand(string command,
                                         vector<string> args,
                                         vector<string> envs,
                                         SerialCommunicationManagerImp *manager,
                                         function<void()> on_exit)
{
    command_ = command;
    args_ = args;
    envs_ = envs;
    manager_ = manager;
    on_exit_ = on_exit;
}

SerialDeviceCommand::~SerialDeviceCommand()
{
    close();
}

bool SerialDeviceCommand::open(bool fail_if_not_ok)
{
    bool ok = invokeBackgroundShell("/bin/sh", args_, envs_, &fd_, &pid_);
    if (!ok) return false;
    manager_->opened(this);
    verbose("(serialcmd) opened %s\n", command_.c_str());
    return true;
}

void SerialDeviceCommand::close()
{
    if (pid_ == 0 && fd_ == -1) return;
    if (pid_ && stillRunning(pid_))
    {
        stopBackgroundShell(pid_);
        pid_ = 0;
    }
    ::flock(fd_, LOCK_UN);
    ::close(fd_);
    fd_ = -1;
    manager_->closed(this);
    verbose("(serialcmd) closed %s\n", command_.c_str());
}

bool SerialDeviceCommand::working()
{
    if (!pid_) return false;
    bool r = stillRunning(pid_);
    if (r) return true;
    close();
    on_exit_();
    return false;
}

bool SerialDeviceCommand::send(vector<uchar> &data)
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
        debug("(serial %s) sent \"%s\"\n", command_.c_str(), msg.c_str());
    }

    end:
    pthread_mutex_unlock(&write_lock_);
    return rc;
}

int SerialDeviceCommand::receive(vector<uchar> *data)
{
    pthread_mutex_lock(&read_lock_);

    data->clear();
    int total = 0;
    int available = 0;
    int num_read = 0;

    ioctl(fd_, FIONREAD, &available);
    if (!available) goto end;

    again:
    total += available;
    data->resize(total);

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
        string msg = safeString(*data);
        debug("(serialcmd) received \"%s\"\n", msg.c_str());
    }

    ioctl(fd_, FIONREAD, &available);
    if (available) goto again;

    end:
    pthread_mutex_unlock(&read_lock_);
    return num_read;
}

struct SerialDeviceSimulator : public SerialDeviceImp
{
    SerialDeviceSimulator(SerialCommunicationManagerImp *m) :
        manager_(m) {};
    ~SerialDeviceSimulator() {};

    bool open(bool fail_if_not_ok) { return true; };
    void close() { };
    bool send(vector<uchar> &data) { return true; };
    int receive(vector<uchar> *data) { return 0; };
    int fd() { return 0; }
    bool working() { return true; }

    SerialCommunicationManager *manager() { return manager_; }

    private:

    SerialCommunicationManagerImp *manager_;
};

SerialCommunicationManagerImp::SerialCommunicationManagerImp(time_t exit_after_seconds)
{
    running_ = true;
    max_fd_ = 0;
    pthread_create(&thread_, NULL, startLoop, this);
    wakeMeUpOnSigChld(thread_);
    start_time_ = time(NULL);
    exit_after_seconds_ = exit_after_seconds;
}

void *SerialCommunicationManagerImp::startLoop(void *a) {
    auto t = (SerialCommunicationManagerImp*)a;
    return t->eventLoop();
}

unique_ptr<SerialDevice> SerialCommunicationManagerImp::createSerialDeviceTTY(string device, int baud_rate) {
    return unique_ptr<SerialDevice>(new SerialDeviceTTY(device, baud_rate, this));
}

unique_ptr<SerialDevice> SerialCommunicationManagerImp::createSerialDeviceCommand(string command,
                                                                                  vector<string> args,
                                                                                  vector<string> envs,
                                                                                  function<void()> on_exit)
{
    return unique_ptr<SerialDevice>(new SerialDeviceCommand(command, args, envs, this, on_exit));
}

unique_ptr<SerialDevice> SerialCommunicationManagerImp::createSerialDeviceSimulator()
{
    return unique_ptr<SerialDevice>(new SerialDeviceSimulator(this));
}

void SerialCommunicationManagerImp::listenTo(SerialDevice *sd, function<void()> cb) {
    SerialDeviceImp *si = dynamic_cast<SerialDeviceImp*>(sd);
    if (!si) {
        error("Internal error: Invalid serial device passed to listenTo.\n");
    }
    si->on_data_ = cb;
}

void SerialCommunicationManagerImp::stop()
{
    running_ = false;
}

void SerialCommunicationManagerImp::waitForStop()
{
    while (running_) { usleep(1000*1000); }
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

void SerialCommunicationManagerImp::opened(SerialDeviceImp *sd) {
    max_fd_ = max(sd->fd(), max_fd_);
    devices_.push_back(sd);
    pthread_kill(thread_, SIGUSR1);
}

void SerialCommunicationManagerImp::closed(SerialDeviceImp *sd) {
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

        struct timeval timeout { 10, 0 };

        if (exit_after_seconds_ > 0) {
            time_t curr = time(NULL);
            time_t diff = curr-start_time_;
            if (diff > exit_after_seconds_) {
                verbose("(serialtty) exit after %ld seconds\n", diff);
                stop();
                break;
            }
            timeout.tv_sec = exit_after_seconds_ - diff;
        }
        int activity = select(max_fd_+1 , &readfds , NULL , NULL, &timeout);

        if (!running_) break;
        if (activity < 0 && errno!=EINTR) {
            warning("(serialtty) internal error after select! errno=%s\n", strerror(errno));
        }
        if (activity > 0) {
            for (SerialDevice *d : devices_) {
                if (FD_ISSET(d->fd(), &readfds)) {
                    SerialDeviceImp *si = dynamic_cast<SerialDeviceImp*>(d);
                    if (si->on_data_) si->on_data_();
                }
            }
        }
        for (SerialDevice *d : devices_) {
            if (!d->working()) {
                stop();
                break;
            }
        }
    }
    verbose("(serialtty) event loop stopped!\n");
    return NULL;
}

unique_ptr<SerialCommunicationManager> createSerialCommunicationManager(time_t exit_after_seconds)
{
    return unique_ptr<SerialCommunicationManager>(new SerialCommunicationManagerImp(exit_after_seconds));
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

SerialCommunicationManager::~SerialCommunicationManager()
{
}

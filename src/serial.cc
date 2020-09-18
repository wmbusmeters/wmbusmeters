/*
 Copyright (C) 2017-2020 Fredrik Öhrström

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
#include"timings.h"

#include <algorithm>
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <functional>
#include <libgen.h>
#include <memory.h>
#include <pthread.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#if defined(__linux__)
#include <linux/serial.h>
#endif

static int openSerialTTY(const char *tty, int baud_rate);
static string showTTYSettings(int fd);

struct SerialDeviceImp;
struct SerialDeviceTTY;
struct SerialDeviceCommand;
struct SerialDeviceFile;
struct SerialDeviceSimulator;
struct Timer
{
    int id;
    int seconds;
    time_t last_call;
    function<void()> callback;
    string name;

    bool isTime(time_t now)
    {
        return (last_call+seconds) <= now;
    }
};

struct SerialCommunicationManagerImp : public SerialCommunicationManager
{
    SerialCommunicationManagerImp(time_t exit_after_seconds, time_t reopen_after_seconds, bool start_event_loop);
    ~SerialCommunicationManagerImp();

    unique_ptr<SerialDevice> createSerialDeviceTTY(string dev, int baud_rate);
    unique_ptr<SerialDevice> createSerialDeviceCommand(string device, string command, vector<string> args, vector<string> envs,
                                                       function<void()> on_exit);
    unique_ptr<SerialDevice> createSerialDeviceFile(string file);
    unique_ptr<SerialDevice> createSerialDeviceSimulator();

    void listenTo(SerialDevice *sd, function<void()> cb);
    void onDisappear(SerialDevice *sd, function<void()> cb);
    void eachEventLooping(function<void()> cb);

    void expectDevicesToWork();
    void stop();
    void startEventLoop();
    void waitForStop();
    bool isRunning();
    void setReopenAfter(int seconds);

    void opened(SerialDeviceImp *sd);
    void closed(SerialDeviceImp *sd);
    void closeAll();

    time_t reopenAfter() { return reopen_after_seconds_; }
    int startRegularCallback(string name, int seconds, function<void()> callback);
    void stopRegularCallback(int id);

    void resetInitiated() { debug("(serial) initiate reset\n"); resetting_ = true; }
    void resetCompleted() { debug("(serial) reset completed\n"); resetting_ = false; }
    vector<string> listSerialDevices();
    vector<string> listSWRadioDevices();
    SerialDevice *lookup(std::string device);

private:

    void *eventLoop();
    void executeTimerCallbacks();
    time_t calculateTimeToNearestTimerCallback(time_t now);
    static void *startLoop(void *);
    static void *runTimers(void *);

    bool running_ {};
    bool expect_devices_to_work_ {}; // false during detection phase, true when running.
    bool resetting_ {}; // Set to true while resetting.
    pthread_t main_thread_ {};
    pthread_t select_thread_ {};
    pthread_t timer_thread_ {};
    int max_fd_ {};
    time_t start_time_ {};
    time_t exit_after_seconds_ {};
    time_t reopen_after_seconds_ {};

    pthread_mutex_t event_loop_lock_ = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t devices_lock_ = PTHREAD_MUTEX_INITIALIZER;
    vector<SerialDeviceImp*> devices_;
    vector<Timer> timers_;
//    pthread_mutex_t timers_lock_ = PTHREAD_MUTEX_INITIALIZER;
    bool calling_timers_ {};
    pthread_mutex_t timer_thread_lock_ = PTHREAD_MUTEX_INITIALIZER; // Only have one regular callback thread running.
    function<void()> on_event_looping_;
};

SerialCommunicationManagerImp::~SerialCommunicationManagerImp()
{
    // Stop the loop.
    stop();
    // Close all managed devices (not yet closed)
    closeAll();
    // Grab the event_loop_lock. This can only be done when the eventLoop has stopped running.
    LOCK("(serial)", "destructor", event_loop_lock_);
    // Now we can be sure the eventLoop has stopped and it is safe to
    // free this Manager object.
}

struct SerialDeviceImp : public SerialDevice
{
    void doNotUseCallbacks() { no_callbacks_ = true; }
    bool skippingCallbacks() { return no_callbacks_; }
    void fill(vector<uchar> &data) {};
    int receive(vector<uchar> *data);
    bool working() { return fd_ != -1; }
    bool readonly() { return is_stdin_ || is_file_; }

    void expectAscii() { expecting_ascii_ = true; }
    void setIsFile() { is_file_ = true; }
    void setIsStdin() { is_stdin_ = true; }
    string device() { return ""; }
    int fd() { return fd_; }

protected:

    pthread_mutex_t read_lock_ = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t write_lock_ = PTHREAD_MUTEX_INITIALIZER;
    function<void()> on_data_;
    function<void()> on_disappear_;
    int fd_ = -1;
    bool expecting_ascii_ {}; // If true, print using safeString instead if bin2hex
    bool is_file_ = false;
    bool is_stdin_ = false;
    bool no_callbacks_ = false;

    friend struct SerialCommunicationManagerImp;

    ~SerialDeviceImp() = default;
};

int SerialDeviceImp::receive(vector<uchar> *data)
{
    bool close_me = false;

    LOCK("(serial)", "receive", read_lock_);
    data->clear();
    int num_read = 0;

    while (true)
    {
        data->resize(num_read+1024);
        int nr = read(fd_, &((*data)[num_read]), 1024);
        if (nr > 0)
        {
            num_read += nr;
        }
        if (nr == 0)
        {
            if (is_stdin_ || is_file_)
            {
                debug("(serial) no more data on fd=%d\n", fd_);
                close_me = true;
            }
            break;
        }
        if (nr < 0)
        {
            if (errno == EINTR && fd_ != -1) continue; // Interrupted try again.
            if (errno == EAGAIN) break;   // No more data available since it would block.
            if (errno == EBADF)
            {
                debug("(serial) got EBADF for fd=%d closing it.\n", fd_);
                close_me = true;
                break;
            }
            break;
        }
    }
    data->resize(num_read);

    if (isDebugEnabled())
    {
        if (expecting_ascii_)
        {
            string msg = safeString(*data);
            debug("(serial) received ascii \"%s\"\n", msg.c_str());
        }
        else
        {
            string msg = bin2hex(*data);
            debug("(serial) received binary \"%s\"\n", msg.c_str());
        }
    }

    UNLOCK("(serial)", "receive", read_lock_);

    if (close_me) close();

    return num_read;
}

struct SerialDeviceTTY : public SerialDeviceImp
{
    SerialDeviceTTY(string device, int baud_rate, SerialCommunicationManagerImp *manager);
    ~SerialDeviceTTY();

    AccessCheck open(bool fail_if_not_ok);
    void close();
    void checkIfShouldReopen();
    bool send(vector<uchar> &data);
    bool working();
    SerialCommunicationManager *manager() { return manager_; }
    string device() { return device_; }

    private:

    string device_;
    int baud_rate_ {};
    SerialCommunicationManagerImp *manager_;
    time_t start_since_reopen_;
    int reopen_after_ {}; // Reopen the device repeatedly after this number of seconds.
    // Necessary for some less than perfect dongles.
};

SerialDeviceTTY::SerialDeviceTTY(string device, int baud_rate,
                                 SerialCommunicationManagerImp *manager)
{
    device_ = device;
    baud_rate_ = baud_rate;
    manager_ = manager;
    start_since_reopen_ = time(NULL);
    reopen_after_ = manager->reopenAfter();
}

SerialDeviceTTY::~SerialDeviceTTY()
{
    close();
}

AccessCheck SerialDeviceTTY::open(bool fail_if_not_ok)
{
    bool ok = checkCharacterDeviceExists(device_.c_str(), fail_if_not_ok);
    if (!ok) return AccessCheck::NotThere;
    fd_ = openSerialTTY(device_.c_str(), baud_rate_);
    if (fd_ < 0)
    {
        if (fail_if_not_ok)
        {
            if (fd_ == -1)
            {
                error("Could not open %s with %d baud N81\n", device_.c_str(), baud_rate_);
            }
            else if (fd_ == -2)
            {
                error("Device %s is already in use and locked.\n", device_.c_str());
            }
        }
        else
        {
            if (fd_ == -1)
            {
                return AccessCheck::NotThere;
            }
            else if (fd_ == -2)
            {
                return AccessCheck::NotSameGroup;
            }
        }
    }
    manager_->opened(this);
    verbose("(serialtty) opened %s\n", device_.c_str());
    return AccessCheck::AccessOK;
}

void SerialDeviceTTY::close()
{
    if (fd_ == -1) return;
    ::flock(fd_, LOCK_UN);
    ::close(fd_);
    fd_ = -1;
    if (on_disappear_)
    {
        on_disappear_();
        on_disappear_ = NULL;
    }
    manager_->closed(this);
    verbose("(serialtty) closed %s\n", device_.c_str());
}

void SerialDeviceTTY::checkIfShouldReopen()
{
    if (fd_ != -1 && reopen_after_ > 0)
    {
        time_t curr = time(NULL);
        time_t diff = curr-start_since_reopen_;
        int available = 0;

        ioctl(fd_, FIONREAD, &available);
        // Is it time to reopen AND there is no data available for reading?
        if (diff > reopen_after_ && !available)
        {
            start_since_reopen_ = curr;

            debug("(serialtty) reopened after %ld seconds\n", diff);
            ::flock(fd_, LOCK_UN);
            ::close(fd_);
            fd_ = openSerialTTY(device_.c_str(), baud_rate_);
            if (fd_ == -1) {
                error("Could not re-open %s with %d baud N81\n", device_.c_str(), baud_rate_);
            }
        }
    }
}

/*
void SerialDeviceTTY::forceReopen()
{
    debug("(serialtty) forced reopen and reset!\n", diff);
    ::flock(fd_, LOCK_UN);
    ::close(fd_);
    fd_ = openSerialTTY(device_.c_str(), baud_rate_);
    if (fd_ == -1)
    {
        error("Could not re-open %s with %d baud N81\n", device_.c_str(), baud_rate_);
    }
}
*/
bool SerialDeviceTTY::send(vector<uchar> &data)
{
    if (data.size() == 0) return true;

    LOCK("(serial)", "send", write_lock_);

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
    UNLOCK("(serial)", "send", write_lock_);
    return rc;
}

bool SerialDeviceTTY::working()
{
    if (fd_ == -1) return false;

    bool working = checkCharacterDeviceExists(device_.c_str(), false);

    if (!working) {
        debug("(serial) device %s is gone\n", device_.c_str());
    }

    return working;
}


struct SerialDeviceCommand : public SerialDeviceImp
{
    SerialDeviceCommand(string device, string command, vector<string> args, vector<string> envs,
                        SerialCommunicationManagerImp *manager,
                        function<void()> on_exit);
    ~SerialDeviceCommand();

    AccessCheck open(bool fail_if_not_ok);
    void close();
    void checkIfShouldReopen() {}
    bool send(vector<uchar> &data);
    int available();
    bool working();
    string device() { return device_; }
    string command() { return command_; }
    SerialCommunicationManager *manager() { return manager_; }

    private:

    string device_;
    string command_;
    int pid_ {};
    vector<string> args_;
    vector<string> envs_;

    pthread_mutex_t write_lock_ = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t read_lock_ = PTHREAD_MUTEX_INITIALIZER;
    SerialCommunicationManagerImp *manager_;
    function<void()> on_exit_;
};

SerialDeviceCommand::SerialDeviceCommand(string device,
                                         string command,
                                         vector<string> args,
                                         vector<string> envs,
                                         SerialCommunicationManagerImp *manager,
                                         function<void()> on_exit)
{
    assert(device != "");
    device_ = device;
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

AccessCheck SerialDeviceCommand::open(bool fail_if_not_ok)
{
    expectAscii();
    bool ok = invokeBackgroundShell("/bin/sh", args_, envs_, &fd_, &pid_);
    if (!ok) return AccessCheck::NotThere;
    manager_->opened(this);
    setIsStdin();
    verbose("(serialcmd) opened %s pid %d fd %d\n", command_.c_str(), pid_, fd_);
    return AccessCheck::AccessOK;
}

void SerialDeviceCommand::close()
{
    int p = pid_, f = fd_;
    if (pid_ == 0 && fd_ == -1) return;
    if (pid_ && stillRunning(pid_))
    {
        stopBackgroundShell(pid_);
        pid_ = 0;
    }
    if (on_disappear_)
    {
        on_disappear_();
        on_disappear_ = NULL;
    }
    ::flock(fd_, LOCK_UN);
    ::close(fd_);
    fd_ = -1;
    manager_->closed(this);
    verbose("(serialcmd) closed %s pid=%d fd=%d\n", command_.c_str(), p, f);
}

bool SerialDeviceCommand::working()
{
    if (fd_ == -1) return false;
    if (!pid_) return false;
    bool r = stillRunning(pid_);
    if (r) return true;
    return false;
}

bool SerialDeviceCommand::send(vector<uchar> &data)
{
    if (data.size() == 0) return true;

    LOCK("(serial)", "sendcmd", write_lock_);

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
    UNLOCK("(serial)", "sendcmd", write_lock_);
    return rc;
}

struct SerialDeviceFile : public SerialDeviceImp
{
    SerialDeviceFile(string file, SerialCommunicationManagerImp *manager);
    ~SerialDeviceFile();

    AccessCheck open(bool fail_if_not_ok);
    void close();
    void checkIfShouldReopen();
    bool send(vector<uchar> &data);
    int available();
    string device() { return file_; }
    SerialCommunicationManager *manager() { return manager_; }

    private:

    string file_;
    SerialCommunicationManagerImp *manager_;
};

SerialDeviceFile::SerialDeviceFile(string file,
                                   SerialCommunicationManagerImp *manager)
{
    file_ = file;
    manager_ = manager;
}

SerialDeviceFile::~SerialDeviceFile()
{
    close();
}

AccessCheck SerialDeviceFile::open(bool fail_if_not_ok)
{
    if (file_ == "stdin")
    {
        fd_ = 0;
        int flags = fcntl(0, F_GETFL);
        flags |= O_NONBLOCK;
        fcntl(0, F_SETFL, flags);
        setIsStdin();
        verbose("(serialfile) reading from stdin\n");
    }
    else
    {
        bool ok = checkFileExists(file_.c_str());
        if (!ok) return AccessCheck::NotThere;
        fd_ = ::open(file_.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd_ == -1)
        {
            if (fail_if_not_ok)
            {
                error("Could not open file %s for reading.\n", file_.c_str());
            }
            else
            {
                return AccessCheck::NotThere;
            }
        }
        setIsFile();
        verbose("(serialfile) reading from file %s\n", file_.c_str());
    }
    manager_->opened(this);
    return AccessCheck::AccessOK;
}

void SerialDeviceFile::close()
{
    if (fd_ == -1) return;
    ::flock(fd_, LOCK_UN);
    ::close(fd_);
    fd_ = -1;
    manager_->closed(this);
    verbose("(serialfile) closed %s %d\n", file_.c_str(), fd_);
}

void SerialDeviceFile::checkIfShouldReopen()
{
}

bool SerialDeviceFile::send(vector<uchar> &data)
{
    return true;
}

struct SerialDeviceSimulator : public SerialDeviceImp
{
    SerialDeviceSimulator(SerialCommunicationManagerImp *m) :
        manager_(m) {
        manager_->opened(this);
        verbose("(serialsimulator) opened\n");
    };
    ~SerialDeviceSimulator() { close(); };

    AccessCheck open(bool fail_if_not_ok) { return AccessCheck::AccessOK; };
    void close() { manager_->closed(this); };
    void checkIfShouldReopen() { }
    bool readonly() { return true; }
    bool send(vector<uchar> &data) { return true; };
    void fill(vector<uchar> &data) { data_ = data; on_data_(); }; // Fill buffer and trigger callback.

    int receive(vector<uchar> *data)
    {
        *data = data_;
        data_.clear();
        return data->size();
    }
    int available() { return data_.size(); }
    int fd() { return -1; }
    bool working() { return false; } // Only one message that has already been handled! So return false here.

    SerialCommunicationManager *manager() { return manager_; }

    private:

    SerialCommunicationManagerImp *manager_;
    vector<uchar> data_;
};

SerialCommunicationManagerImp::SerialCommunicationManagerImp(time_t exit_after_seconds,
                                                             time_t reopen_after_seconds,
                                                             bool start_event_loop)
{
    running_ = true;
    max_fd_ = 0;
    // Block the event loop until everything is configured.
    if (start_event_loop)
    {
        LOCK("(serial)", "constructor", event_loop_lock_);
        pthread_create(&select_thread_, NULL, startLoop, this);
    }
    wakeMeUpOnSigChld(select_thread_);
    start_time_ = time(NULL);
    exit_after_seconds_ = exit_after_seconds;
    reopen_after_seconds_ = reopen_after_seconds;
}

void *SerialCommunicationManagerImp::startLoop(void *a)
{
    auto t = (SerialCommunicationManagerImp*)a;
    return t->eventLoop();
}

void *SerialCommunicationManagerImp::runTimers(void *a)
{
    auto t = (SerialCommunicationManagerImp*)a;
    t->executeTimerCallbacks();
    t->calling_timers_ = false;
    // Now unlock the previously trylocked mutex.
    UNLOCK("(serial)", "runTimers", t->timer_thread_lock_);
    return NULL;
}

unique_ptr<SerialDevice> SerialCommunicationManagerImp::createSerialDeviceTTY(string device,
                                                                              int baud_rate)
{
    return unique_ptr<SerialDevice>(new SerialDeviceTTY(device, baud_rate, this));
}

unique_ptr<SerialDevice> SerialCommunicationManagerImp::createSerialDeviceCommand(string device,
                                                                                  string command,
                                                                                  vector<string> args,
                                                                                  vector<string> envs,
                                                                                  function<void()> on_exit)
{
    return unique_ptr<SerialDevice>(new SerialDeviceCommand(device, command, args, envs, this, on_exit));
}

unique_ptr<SerialDevice> SerialCommunicationManagerImp::createSerialDeviceFile(string file)
{
    return unique_ptr<SerialDevice>(new SerialDeviceFile(file, this));
}

unique_ptr<SerialDevice> SerialCommunicationManagerImp::createSerialDeviceSimulator()
{
    return unique_ptr<SerialDevice>(new SerialDeviceSimulator(this));
}

void SerialCommunicationManagerImp::listenTo(SerialDevice *sd, function<void()> cb)
{
    if (sd == NULL) return;
    SerialDeviceImp *si = dynamic_cast<SerialDeviceImp*>(sd);
    if (!si)
    {
        error("Internal error: Invalid serial device passed to listenTo.\n");
    }
    si->on_data_ = cb;
}

void SerialCommunicationManagerImp::onDisappear(SerialDevice *sd, function<void()> cb)
{
    if (sd == NULL) return;
    SerialDeviceImp *si = dynamic_cast<SerialDeviceImp*>(sd);
    if (!si)
    {
        error("Internal error: Invalid serial device passed to onDisappear.\n");
    }
    si->on_disappear_ = cb;
}

void SerialCommunicationManagerImp::eachEventLooping(function<void()> cb)
{
    on_event_looping_ = cb;
}

void SerialCommunicationManagerImp::expectDevicesToWork()
{
    debug("(serial) expecting devices to work\n");
    expect_devices_to_work_ = true;
}

void SerialCommunicationManagerImp::stop()
{
    // Notify the main waitForStop thread that we are stopped!
    if (running_ == true)
    {
        debug("(serial) stopping manager\n");
        running_ = false;
        if (main_thread_ != 0)
        {
            if (signalsInstalled())
            {
                if (main_thread_) pthread_kill(main_thread_, SIGUSR2);
                if (select_thread_) pthread_kill(select_thread_, SIGUSR1);
            }
        }
    }
}

void SerialCommunicationManagerImp::startEventLoop()
{
    // Release the event loop!
    UNLOCK("(serial)", "startEventLoop", event_loop_lock_);
}

void SerialCommunicationManagerImp::waitForStop()
{
    debug("(serial) waiting for stop\n");

    //expect_devices_to_work_ = true;
    main_thread_ = pthread_self();
    while (running_)
    {
        LOCK("(serial)", "waitForStop", devices_lock_);
        size_t s = devices_.size();
        UNLOCK("(serial)", "waitForStop", devices_lock_);

        if (s == 0) {
            break;
        }
        usleep(1000*1000);
    }
    debug("(serial) closing %d devices\n", devices_.size());
    closeAll();

    if (signalsInstalled())
    {
        if (select_thread_) pthread_kill(select_thread_, SIGUSR1);
    }
    pthread_join(select_thread_, NULL);

}

bool SerialCommunicationManagerImp::isRunning()
{
    return running_;
}

void SerialCommunicationManagerImp::setReopenAfter(int seconds)
{
    reopen_after_seconds_ = seconds;
}

void SerialCommunicationManagerImp::opened(SerialDeviceImp *sd)
{
    LOCK("(serial)", "opened", devices_lock_);
    max_fd_ = max(sd->fd(), max_fd_);
    devices_.push_back(sd);
    if (signalsInstalled())
    {
        if (select_thread_) pthread_kill(select_thread_, SIGUSR1);
    }
    UNLOCK("(serial)", "opened", devices_lock_);
}

void SerialCommunicationManagerImp::closed(SerialDeviceImp *sd)
{
    LOCK("(serial)", "closed", devices_lock_);
    auto p = find(devices_.begin(), devices_.end(), sd);
    if (p != devices_.end())
    {
        devices_.erase(p);
    }
    max_fd_ = 0;
    for (SerialDevice *d : devices_)
    {
        if (d->fd() > max_fd_)
        {
            max_fd_ = d->fd();
        }
    }
    if (devices_.size() == 0 && expect_devices_to_work_ && resetting_ == false)
    {
        debug("(serial) no devices working emergency exit!\n");
        stop();
    }
    UNLOCK("(serial)", "opened", devices_lock_);
}

void SerialCommunicationManagerImp::closeAll()
{
    LOCK("(serial)", "closeAll", devices_lock_);
    vector<SerialDeviceImp*> copy = devices_;
    devices_.clear();
    UNLOCK("(serial)", "closeAll", devices_lock_);

    for (SerialDeviceImp *d : copy)
    {
        if (d->on_disappear_)
        {
            d->on_disappear_();
            d->on_disappear_ = NULL;
        }

        closed(d);
    }
}

void SerialCommunicationManagerImp::executeTimerCallbacks()
{
    time_t curr = time(NULL);
    vector<Timer> to_be_called;

    LOCK("(serial)", "executeTimerCallbacks", devices_lock_);

    for (Timer &t : timers_)
    {
        if (t.isTime(curr))
        {
            trace("[SERIAL] timer isTime! %d %s\n", t.id, t.name.c_str());
            t.last_call = curr;
            to_be_called.push_back(t);
        }
    }

    UNLOCK("(serial)", "executeTimerCallbacks", devices_lock_);

    for (Timer &t : to_be_called)
    {
        trace("[SERIAL] invoking callback %s(%d)\n", t.name.c_str(), t.id);
        t.callback();
    }

}

time_t SerialCommunicationManagerImp::calculateTimeToNearestTimerCallback(time_t now)
{
    time_t r = 1024*1024*1024;
    for (Timer &t : timers_)
    {
        // Expected time when to trigger in the future.
        // Well, could be in the past as well, if we are unlucky.
        time_t done = t.last_call+t.seconds;
        // Now how long time is it left....could be negative
        // if we are late.
        time_t remaining = done-now;
        if (remaining < r) r = remaining;
    }
    return r;
}

void *SerialCommunicationManagerImp::eventLoop()
{
    fd_set readfds;

    LOCK("(serial)", "eventLoop", event_loop_lock_);

    while (running_)
    {
        FD_ZERO(&readfds);

        bool all_working = true;
        LOCK("(serial)", "opened", devices_lock_);
        for (SerialDevice *d : devices_)
        {
            if (!d->skippingCallbacks())
            {
                trace("(SERIAL) select read on fd %d\n", d->fd());
                FD_SET(d->fd(), &readfds);
            }
            if (!d->working()) all_working = false;
        }
        UNLOCK("(serial)", "opened", devices_lock_);

        if (!all_working && expect_devices_to_work_ && resetting_ == false)
        {
            debug("(serial) not all devices working, emergency exit!\n");
            stop();
            break;
        }

        // Perform a select call every second.
        struct timeval timeout { 1, 0 };
        time_t curr = time(NULL);

        if (exit_after_seconds_ > 0)
        {
            time_t diff = curr-start_time_;
            if (diff > exit_after_seconds_)
            {
                // Running time limit hit, now stop.
                verbose("(serial) exit after %ld seconds\n", diff);
                stop();
                break;
            }
        }

        trace("[SERIAL] select timeout %d s\n", timeout.tv_sec);

        bool num_devices = 0;
        LOCK("(serial)", "eventLoop2", devices_lock_);
        for (SerialDevice *d : devices_)
        {
            d->checkIfShouldReopen();
        }
        num_devices = devices_.size();
        UNLOCK("(serial)", "eventLoop2", devices_lock_);

        if (num_devices == 0 && expect_devices_to_work_ && resetting_ == false)
        {
            debug("(serial) no working devices, stopping before entering select.\n");
            stop();
            break;
        }

        int activity = select(max_fd_+1 , &readfds, NULL , NULL, &timeout);
        if (!running_) break;
        if (activity < 0 && errno!=EINTR)
        {
            warning("(serial) internal error after select! errno=%s\n", strerror(errno));
        }
        if (activity > 0)
        {
            // Something has happened that caused the sleeping select to wake up.
            vector<SerialDeviceImp*> to_be_notified;
            LOCK("(serial)", "eventLoop3", devices_lock_);
            for (SerialDevice *d : devices_)
            {
                if (FD_ISSET(d->fd(), &readfds))
                {
                    SerialDeviceImp *si = dynamic_cast<SerialDeviceImp*>(d);
                    to_be_notified.push_back(si);
                }
            }
            UNLOCK("(serial)", "eventLoop3", devices_lock_);

            for (SerialDeviceImp *si : to_be_notified)
            {
                if (si->on_data_)
                {
                    si->on_data_();
                }
            }
        }
        vector<SerialDeviceImp*> non_working;

        LOCK("(serial)", "eventLoop4", devices_lock_);
        for (SerialDeviceImp *d : devices_)
        {
            if (!d->working()) non_working.push_back(d);
        }
        UNLOCK("(serial)", "eventLoop4", devices_lock_);

        for (SerialDeviceImp *d : non_working)
        {
            debug("(serial) closing non working fd=%d\n", d->fd());
            d->close();
        }

        bool timer_found = false;
        for (Timer &t : timers_)
        {
            timer_found |= t.isTime(curr);
        }

        if (timer_found)
        {
            int rc = pthread_mutex_trylock(&timer_thread_lock_);
            // Only start timer thread if it is not running already.
            if (rc == 0)
            {
                // Lock acquired start timer thread.
                // Lock will be released by this newly started thread.
                calling_timers_ = true;
                pthread_create(&timer_thread_, NULL, runTimers, this);
            }
            else
            {
                trace("(serial) not starting timer thread since it is already running.\n");
            }
        }

        // Invoke callback every event looping....
        if (on_event_looping_) on_event_looping_();

        if (non_working.size() > 0 && expect_devices_to_work_ && resetting_ == false)
        {
            debug("(serial) non working devices found, exiting.\n");
            stop();
            break;
        }
    }
    verbose("(serial) event loop stopped!\n");
    UNLOCK("(serial)", "eventLoop", event_loop_lock_);
    return NULL;
}

unique_ptr<SerialCommunicationManager> createSerialCommunicationManager(time_t exit_after_seconds,
                                                                        time_t reopen_after_seconds,
                                                                        bool start_event_loop)
{
    return unique_ptr<SerialCommunicationManager>(new SerialCommunicationManagerImp(exit_after_seconds,
                                                                                    reopen_after_seconds,
                                                                                    start_event_loop));
}

static int openSerialTTY(const char *tty, int baud_rate)
{
    int rc = 0;
    speed_t speed = 0;
    struct termios tios;
    string tty_info;
    //int DTR_flag = TIOCM_DTR;

    int fd = open(tty, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd == -1) {
        usleep(1000*1000);
        fd = open(tty, O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd == -1) goto err;
    }
    rc = flock(fd, LOCK_EX | LOCK_NB);
    if (rc == -1)
    {
        // It is already locked by another wmbusmeter process.
        fd = -2;
        goto err;
    }

    tty_info = showTTYSettings(fd);
    debug("(serial) before config: %s %s\n",  tty, tty_info.c_str());

    switch (baud_rate)
    {
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


    // CREAD=Enable receive CLOCAL=Ignore any Carrier Detect signal.
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


    // This code can toggle DTR... maybe necessary
    // for the pl2303 usb2serial driver/device.
    //rc = ioctl(fd, TIOCMBIC, &DTR_flag);
    //if (rc != 0) goto err;

    tty_info = showTTYSettings(fd);
    debug("(serial) after config:  %s %s\n",  tty, tty_info.c_str());

    return fd;

err:
    if (fd > 0)
    {
        close(fd);
        fd = -1;
    }
    return fd;
}

SerialCommunicationManager::~SerialCommunicationManager()
{
}

int SerialCommunicationManagerImp::startRegularCallback(string name, int seconds, function<void()> callback)
{
    Timer t = { (int)timers_.size(), seconds, time(NULL), callback, name };
    LOCK("(serial)", "startRegularCallback", devices_lock_);
    timers_.push_back(t);
    UNLOCK("(serial)", "startRegularCallback", devices_lock_);
    debug("(serial) registered regular callback %s(%d) every %d seconds\n", name.c_str(), t.id, seconds);
    return t.id;
}

void SerialCommunicationManagerImp::stopRegularCallback(int id)
{
    debug("(serial) stopping regular callback %d\n", id);
    LOCK("(serial)", "stopRegularCallback", devices_lock_);
    for (auto i = timers_.begin(); i != timers_.end(); ++i)
    {
        if ((*i).id == id)
        {
            timers_.erase(i);
            break;
        }
    }
    UNLOCK("(serial)", "stopRegularCallback", devices_lock_);
}


SerialDevice *SerialCommunicationManagerImp::lookup(string device)
{
    for (auto sd : devices_)
    {
        if (sd->device() == device) return sd;
    }
    return NULL;
}


#if defined(__APPLE__)
vector<string> SerialCommunicationManagerImp::listSerialDevices()
{
    vector<string> list;
    list.push_back("Please add code here!");
    return list;
}
vector<string> SerialCommunicationManagerImp::listSWRadioDevices()
{
    vector<string> list;
    list.push_back("Please add code here!");
    return list;
}
#endif

#if defined(__linux__)

static string lookup_device_driver(string tty)
{
    struct stat st;
    tty += "/device";
    int rc = lstat(tty.c_str(), &st);
    if (rc==0 && S_ISLNK(st.st_mode))
    {
        tty += "/driver";
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        int rc = readlink(tty.c_str(), buffer, sizeof(buffer));
        if (rc > 0)
        {
            return basename(buffer);
        }
    }
    return "";
}

static void check_if_serial(string tty, vector<string> *found_serials, vector<string> *found_8250s)
{
    string driver = lookup_device_driver(tty);

    if (driver.size() > 0)
    {
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        strncpy(buffer, tty.c_str(), sizeof(buffer)-1);

        string dev = buffer;

        if (driver == "serial8250")
        {
            found_8250s->push_back(dev);
        }
        else
        {
            // The dev is now something like: /sys/class/tty/ttyUSB0
            // Drop the /sys/class/tty/ prefix and replace with /dev/
            if (dev.rfind("/sys/class/tty/", 0) == 0) {
                dev = string("/dev/")+dev.substr(15);
            }
            found_serials->push_back(dev);
        }
    }
}

static void check_serial8250s(vector<string> *found_serials, vector<string> &found_8250s)
{
    struct serial_struct serinfo;

    for (string dev : found_8250s)
    {
        int fd = open(dev.c_str(), O_RDWR | O_NONBLOCK | O_NOCTTY);

        if (fd >= 0)
        {
            int rc = ioctl(fd, TIOCGSERIAL, &serinfo);
            if (rc == 0)
            {
                if (serinfo.type != PORT_UNKNOWN)
                {
                    found_serials->push_back(dev);
                }
                close(fd);
            }
        }
    }
}

int sorty(const struct dirent **a, const struct dirent **b)
{
    return strcmp((*a)->d_name, (*b)->d_name);
}

vector<string> SerialCommunicationManagerImp::listSerialDevices()
{
    struct dirent **entries;
    vector<string> found_serials;
    vector<string> found_8250s;
    string sysdir = "/sys/class/tty/";

    int n = scandir(sysdir.c_str(), &entries, NULL, sorty);
    if (n < 0)
    {
        perror("scandir");
        return found_serials;
    }

    for (int i=0; i<n; ++i)
    {
        string name = entries[i]->d_name;

        if (name ==  ".." || name == ".")
        {
            free(entries[i]);
            continue;
        }

        string tty = sysdir+name;
        check_if_serial(tty, &found_serials, &found_8250s);
        free(entries[i]);
    }
    free(entries);

    check_serial8250s(&found_serials, found_8250s);

    return found_serials;
}

vector<string> SerialCommunicationManagerImp::listSWRadioDevices()
{
    struct dirent **entries;
    vector<string> found_swradios;
    string devdir = "/dev/";

    int n = scandir(devdir.c_str(), &entries, NULL, sorty);
    if (n < 0)
    {
        perror("scandir");
        return found_swradios;
    }

    for (int i=0; i<n; ++i)
    {
        string name = entries[i]->d_name;

        if (name ==  ".." || name == ".")
        {
            free(entries[i]);
            continue;
        }
        // swradio0 swradio1 swradio2 ...
        if (name.length() > 7 &&
            !strncmp(name.c_str(), "swradio", 7) &&
            name[7] >= '0' &&
            name[7] <= '9')
        {
            string rtlsdr = devdir+name;
            found_swradios.push_back(rtlsdr);
        }
        free(entries[i]);
    }
    free(entries);

    return found_swradios;
}

#endif

#define CHECK_SPEED(x) { if (speed == x) return #x; }

string translateSpeed(speed_t speed)
{
    string flags;

	CHECK_SPEED(B50)
	CHECK_SPEED(B75)
	CHECK_SPEED(B110)
	CHECK_SPEED(B134)
	CHECK_SPEED(B150)
	CHECK_SPEED(B200)
	CHECK_SPEED(B300)
	CHECK_SPEED(B600)
	CHECK_SPEED(B1200)
	CHECK_SPEED(B1800)
	CHECK_SPEED(B2400)
	CHECK_SPEED(B4800)
	CHECK_SPEED(B9600)
#ifdef B57600
	CHECK_SPEED(B57600)
#endif
#ifdef B115200
	CHECK_SPEED(B115200)
#endif
	CHECK_SPEED(B19200)
#ifdef B230400
	CHECK_SPEED(B230400)
#endif
	CHECK_SPEED(B38400)
#ifdef B460800
	CHECK_SPEED(B460800)
#endif
#ifdef B500000
	CHECK_SPEED(B500000)
#endif
#ifdef B57600
	CHECK_SPEED(B57600)
#endif
#ifdef B921600
	CHECK_SPEED(B921600)
#endif
#ifdef B1000000
	CHECK_SPEED(B1000000)
#endif
#ifdef B1152000
	CHECK_SPEED(B1152000)
#endif
#ifdef B1500000
	CHECK_SPEED(B1500000)
#endif
#ifdef B2000000
	CHECK_SPEED(B2000000)
#endif
#ifdef B2500000
	CHECK_SPEED(B2500000)
#endif
#ifdef B3000000
	CHECK_SPEED(B3000000)
#endif
#ifdef B3500000
	CHECK_SPEED(B3500000)
#endif
#ifdef B4000000
	CHECK_SPEED(B4000000)
#endif

    return "UnknownSpeed";
};

#undef CHECK_SPEED

string lookupSpeed(struct termios *tios)
{
    speed_t in = cfgetispeed(tios);
    speed_t out = cfgetispeed(tios);

    if (in == out)
    {
        return translateSpeed(in);
    }

    return translateSpeed(in)+","+translateSpeed(out);
}

#define CHECK_FLAG(x) { if (bits & x) flags += #x "|"; }

string iflags(tcflag_t bits)
{
    string flags;

    CHECK_FLAG(BRKINT)
	CHECK_FLAG(ICRNL)
	CHECK_FLAG(IGNBRK)
	CHECK_FLAG(IGNCR)
	CHECK_FLAG(IGNPAR)
#ifdef IMAXBEL
	CHECK_FLAG(IMAXBEL)
#endif
	CHECK_FLAG(INLCR)
	CHECK_FLAG(ISTRIP)
#ifdef IUTF8
	CHECK_FLAG(IUTF8)
#endif
	CHECK_FLAG(IXANY)
	CHECK_FLAG(IXOFF)
	CHECK_FLAG(IXON)
	CHECK_FLAG(PARMRK)

    if (flags.length() > 0) flags.pop_back();
    return flags;
};

string oflags(tcflag_t bits)
{
    string flags;

	CHECK_FLAG(BS1)
	CHECK_FLAG(NL1)
	CHECK_FLAG(ONLCR)
#ifdef ONOEOT
	CHECK_FLAG(ONOEOT)
#endif
	CHECK_FLAG(OPOST)
#ifdef OXTABS
	CHECK_FLAG(OXTABS)
#endif

    if (flags.length() > 0) flags.pop_back();
    return flags;
};

string cflags(tcflag_t bits)
{
    string flags;

	CHECK_FLAG(CLOCAL)
	CHECK_FLAG(CREAD)
	CHECK_FLAG(CSIZE)
	CHECK_FLAG(CSTOPB)
	CHECK_FLAG(HUPCL)

    if (flags.length() > 0) flags.pop_back();
    return flags;
};

string lflags(tcflag_t bits)
{
    string flags;

	CHECK_FLAG(ECHO)
	CHECK_FLAG(ECHOCTL)
	CHECK_FLAG(ECHOE)
	CHECK_FLAG(ECHOK)
	CHECK_FLAG(ECHOKE)
	CHECK_FLAG(ECHONL)
	CHECK_FLAG(ECHOPRT)
	CHECK_FLAG(FLUSHO)
	CHECK_FLAG(ICANON)
	CHECK_FLAG(IEXTEN)
	CHECK_FLAG(ISIG)
	CHECK_FLAG(NOFLSH)
	CHECK_FLAG(PENDIN)
	CHECK_FLAG(TOSTOP)
#ifdef XCASE
	CHECK_FLAG(XCASE)
#endif

    if (flags.length() > 0) flags.pop_back();
    return flags;
};

#undef CHECK_FLAG

string showSpecialChars(struct termios *tios)
{
    string s;

    int n = sizeof(tios->c_cc)/sizeof(cc_t);
	for (int i=0; i<n; ++i)
    {
        cc_t c = tios->c_cc[i];
        if (c != 0)
        {
            string cc;
            strprintf(cc, "%u", c);
            s += cc+",";
        }
    }
    if (s.length() > 0) s.pop_back();
    return s;
}

static string showTTYSettings(int fd)
{
    string info;
    string bits;

    struct termios tios;
    int modem_bits;

    int rc = tcgetattr(fd, &tios);
    if (rc != 0) goto err;

    info += "speed("+lookupSpeed(&tios)+") ";
    info += "input("+iflags(tios.c_iflag) + ") ";
    info += "output("+oflags(tios.c_oflag) + ") ";
    info += "control("+cflags(tios.c_cflag) + ") ";
    info += "local("+lflags(tios.c_lflag) + ") ";
    info += "special_chars("+showSpecialChars(&tios)+") ";

    rc = ioctl(fd, TIOCMGET, &modem_bits);
    if (rc != 0) goto err;

    if (modem_bits & TIOCM_LE) bits += "LE|"; // Line Enabled (same as DSR below?)
    if (modem_bits & TIOCM_DTR) bits += "DTR|"; // Data Terminal Ready (Computer is ready.)
    if (modem_bits & TIOCM_RTS) bits += "RTS|"; // Request to send (hardware flow control)
    if (modem_bits & TIOCM_ST) bits += "ST|";
    if (modem_bits & TIOCM_SR) bits += "SR|";
    if (modem_bits & TIOCM_CTS) bits += "CTS|"; // Clear to Send (hardware flow control)
    if (modem_bits & TIOCM_CD) bits += "CD|"; // Data Carrier Detected (Modem is connected to another modem)
    if (modem_bits & TIOCM_RI) bits += "RING|"; // Ring Indicator
    if (modem_bits & TIOCM_DSR) bits += "DSR|"; // Data Set Ready (Modem is ready.)

    if (bits.length() > 0) bits.pop_back();
    info += "modem("+bits+")";
    return info;

err:

    return "error";
}

/*
 Copyright (C) 2021 Fredrik Öhrström

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

// Utility to relay serial port SND_NR telegrams using mosquitto pub.

#define DEBUG 0

#define HOMEDIR "/home/yourdir"

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <memory.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

typedef unsigned char uchar;

typedef uchar bool;
#define true 1
#define false 0

int open_serial(const char *tty, int baud_rate)
{
    int rc = 0;
    speed_t speed = 0;
    struct termios tios;
    //int DTR_flag = TIOCM_DTR;

    int fd = open(tty, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd == -1) {
        usleep(1000*1000);
        fd = open(tty, O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd == -1) goto err;
    }

    switch (baud_rate)
    {
    case 300:   speed = B300;  break;
    case 600:   speed = B600;  break;
    case 1200:   speed = B1200;  break;
    case 2400:   speed = B2400;  break;
    case 4800:   speed = B4800;  break;
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
    // Disable parity bit.
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

    return fd;

err:
    if (fd > 0)
    {
        close(fd);
        fd = -1;
    }
    return fd;
}

void manufacturerFlag(uchar lo, uchar hi, uchar *mfct)
{
    int m_field = (hi*256)+lo;
    mfct[0] = (m_field/1024)%32+64;
    mfct[1] = (m_field/32)%32+64;
    mfct[2] = (m_field)%32+64;
}

bool find_telegram(uchar *buf, size_t *len)
{
    size_t l = *len;
    if (*len < 10) return false;
    size_t tl = buf[0];
    if (buf[1] != 0x44) return false;
    if (tl+1 > l) return false;
    size_t hl = 2*(tl+1)+1;
    uchar hex[hl];
    for (size_t i=0; i<tl+1; ++i)
    {
        sprintf(hex+i*2, "%02x", buf[i]);
    }
    hex[hl-1] = 0;
    *len = l-tl-1;

    uchar id[9];
    sprintf(id,   "%02x", buf[7]);
    sprintf(id+2, "%02x", buf[6]);
    sprintf(id+4, "%02x", buf[5]);
    sprintf(id+6, "%02x", buf[4]);
    id[8] = 0;

    uchar mfct[4];
    manufacturerFlag(buf[2], buf[3], mfct);
    mfct[3] = 0;

    char t[21];
    snprintf(t, sizeof(t), "wmbusmeters/%s", id);
    char *program = "mosquitto_pub";
    const char *argv[8];
    argv[0] = program;
    argv[1] = "-h";
    argv[2] = "localhost";
    argv[3] = "-t";
    argv[4] = t;
    argv[5] = "-m";
    argv[6] = hex;
    argv[7] = 0;

    printf("Relaying id:%s mft:%s t:%s \"%s\"\n", id, mfct, t, hex);

    const char *env[2];
    env[0] = "HOME=" HOMEDIR;
    env[1] = 0;

    pid_t pid = fork();
    int status;
    if (pid == 0) {
        close(0); // Close stdin
#if (defined(__APPLE__) && defined(__MACH__)) || defined(__FreeBSD__)
        execve(program, (char*const*)&argv[0], (char*const*)&env[0]);
#else
        execvpe(program, (char*const*)&argv[0], (char*const*)&env[0]);
#endif
        perror("Execvp failed:");
    } else {
        if (pid == -1) {
            fprintf(stderr, "(shell) could not fork!\n");
        }
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            // Child exited properly.
            int rc = WEXITSTATUS(status);
            if (rc != 0) {
                fprintf(stderr, "(shell) %s exited with non-zero return code: %d\n", program, rc);
            }
        }
    }
    return true;
}

int main()
{
    uchar buf[512];
    size_t len = 0;
    int count_to_clear = 0;
    int fd = open_serial("/dev/ttyUSB0", 19200);

    if (fd == -1)
    {
        fprintf(stderr, "Could not open serial port.\n");
    }

    for (;;)
    {
        if (len >= sizeof(buf))
        {
            // Oups, a lot of random data fills up the buffer with no telegrams!
            len = 0;
            count_to_clear = 0;
            if (DEBUG) fprintf(stderr, "overflow clear\n");
        }
        if (DEBUG) fprintf(stderr, "Reading...");
        ssize_t n = read(fd, buf+len, sizeof(buf)-len);
        if (n <= 0)
        {
            if (errno == EBADF)
            {
                fprintf(stderr, "Could not read from serial port.\n");
                break;
            }
            sleep(1);
            count_to_clear++;
            if (count_to_clear >= 2)
            {
                // After 2 seconds of no traffic, we clear the buffer.
                // Any existing telegrams should have been found already.
                len = 0;
                count_to_clear = 0;
                if (DEBUG) fprintf(stderr, "timeout clear\n");
            }
            else
            {
                if (DEBUG) fprintf(stderr, "no data.\n");
            }
            continue;
        }
        len+=n;
        if (DEBUG) fprintf(stderr, "received data, buffer len %zu\n", len);
        bool ok = find_telegram(buf, &len);
        if (!ok)
        {
            if (DEBUG) fprintf(stderr, "No telegram found in data.");
        }
    }
    close(fd);
}

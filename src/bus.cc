/*
 Copyright (C) 2017-2021 Fredrik Öhrström

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

#include"bus.h"
#include"cmdline.h"
#include"config.h"
#include"meters.h"
#include"printer.h"
#include"rtlsdr.h"
#include"serial.h"
#include"shell.h"
#include"threads.h"
#include"util.h"
#include"version.h"
#include"wmbus.h"

#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <set>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

using namespace std;

shared_ptr<BusManager> createBusManager(shared_ptr<SerialCommunicationManager> serial_manager,
                                        shared_ptr<MeterManager> meter_manager)
{
    return shared_ptr<BusManager>(new BusManager(serial_manager, meter_manager));
}

BusManager::BusManager(shared_ptr<SerialCommunicationManager> serial_manager,
                       shared_ptr<MeterManager> meter_manager)
    : serial_manager_(serial_manager),
        meter_manager_(meter_manager),
        bus_devices_mutex_("bus_devices_mutex"),
        bus_send_queue_mutex_("bus_send_queue_mutex"),
        printed_warning_(true)
{
}

void BusManager::removeAllBusDevices()
{
    bus_devices_.clear();
}

void BusManager::openBusDeviceAndPotentiallySetLinkmodes(Configuration *config, string how, Detected *detected)
{
    if (detected->found_type == WMBusDeviceType::DEVICE_UNKNOWN)
    {
        debug("(verbose) ignoring device %s\n", detected->specified_device.str().c_str());
        return;
    }

    LOCK_BUS_DEVICES(open_bus_device);

    debug("(main) opening %s\n", detected->specified_device.str().c_str());

    LinkModeSet lms = detected->specified_device.linkmodes;
    if (lms.empty())
    {
        if (config->use_auto_device_detect)
        {
            lms = config->auto_device_linkmodes;
        }
        if (lms.empty())
        {
            lms = config->default_device_linkmodes;
        }
    }
    string using_link_modes = lms.hr();

    string bus = detected->specified_device.bus_alias.c_str();
    if (bus != "")
    {
        bus = bus+"=";
    }
    string id = detected->found_device_id.c_str();
    if (id != "") id = "["+id+"]";
    string extras = detected->specified_device.extras.c_str();
    if (extras != "") extras = "("+extras+")";
    string fq = detected->specified_device.fq;
    if (fq != "") fq = " using fq "+fq;
    string file = detected->found_file.c_str();
    if (file != "") file = " on "+file;
    string cmd = detected->specified_device.command.c_str();
    if (cmd != "")
    {
        cmd = " using CMD("+cmd+")";
    }

    string listening = "";
    if (detected->found_type != WMBusDeviceType::DEVICE_MBUS)
    {
        listening = tostrprintf(" listening on %s%s%s",
                                 using_link_modes.c_str(),
                                 fq.c_str(),
                                 cmd.c_str());
    }
    string started = tostrprintf("Started %s %s%s%s%s%s%s\n",
                                 how.c_str(),
                                 bus.c_str(),
                                 toLowerCaseString(detected->found_type),
                                 id.c_str(),
                                 extras.c_str(),
                                 file.c_str(),
                                 listening.c_str());

    // A newly plugged in device has been manually configured or automatically detected! Start using it!
    if (config->use_auto_device_detect || detected->found_type != DEVICE_SIMULATION)
    {
        notice_timestamp("%s", started.c_str());
    }
    else
    {
        // Hide the started when running simulations.
        verbose("%s", started.c_str());
    }

    shared_ptr<WMBus> wmbus = createWmbusObject(detected, config);
    if (wmbus == NULL) return;
    bus_devices_.push_back(wmbus);

    // By default, reset your dongle once every 23 hours,
    // so that the reset is not at the exact same time every day.
    int regular_reset = 23*3600;
    if (config->resetafter != 0) regular_reset = config->resetafter;
    wmbus->setResetInterval(regular_reset);
    verbose("(main) regular reset of %s %s%s will happen every %d seconds\n",
            toString(detected->found_type), file.c_str(), cmd.c_str(), regular_reset);

    if (wmbus->canSetLinkModes(lms))
    {
        wmbus->setLinkModes(lms);
    }
    else
    {
        warning("Warning! Desired link modes %s cannot be set for device %s\n",
                lms.hr().c_str(), wmbus->hr().c_str());
    }
    bool simulated = false;
    if (detected->found_type == DEVICE_SIMULATION)
    {
        simulated = true;
        debug("(main) added %s to files\n", detected->found_file.c_str());
        simulation_files_.insert(detected->specified_device.file);
    }
    wmbus->onTelegram([&, simulated](AboutTelegram &about,vector<uchar> data){return meter_manager_->handleTelegram(about, data, simulated);});
    wmbus->setTimeout(config->alarm_timeout, config->alarm_expected_activity);
}

shared_ptr<WMBus> BusManager::createWmbusObject(Detected *detected, Configuration *config)
{
    shared_ptr<WMBus> wmbus;

    shared_ptr<SerialDevice> serial_override;

    if (detected->found_tty_override)
    {
        serial_override = serial_manager_->createSerialDeviceFile(detected->specified_device.file,
                                                                  string("override ")+detected->specified_device.file.c_str());
        verbose("(serial) override with devicefile: %s\n", detected->specified_device.file.c_str());
    }

    switch (detected->found_type)
    {
    case DEVICE_AUTO:
        assert(0);
        error("Internal error DEVICE_AUTO should not be used here!\n");
        break;
    case DEVICE_MBUS:
        verbose("(mbus) on %s\n", detected->found_file.c_str());
        wmbus = openMBUS(*detected, serial_manager_, serial_override);
        break;
    case DEVICE_IM871A:
        verbose("(im871a) on %s\n", detected->found_file.c_str());
        wmbus = openIM871A(*detected, serial_manager_, serial_override);
        break;
    case DEVICE_IM170A:
        verbose("(im170a) on %s\n", detected->found_file.c_str());
        wmbus = openIM170A(*detected, serial_manager_, serial_override);
        break;
    case DEVICE_AMB8465:
        verbose("(amb8465) on %s\n", detected->found_file.c_str());
        wmbus = openAMB8465(*detected, serial_manager_, serial_override);
        break;
    case DEVICE_SIMULATION:
        verbose("(simulation) in %s\n", detected->found_file.c_str());
        wmbus = openSimulator(*detected, serial_manager_, serial_override);
        break;
    case DEVICE_RAWTTY:
        verbose("(rawtty) on %s\n", detected->found_file.c_str());
        wmbus = openRawTTY(*detected, serial_manager_, serial_override);
        break;
    case DEVICE_RTLWMBUS:
        wmbus = openRTLWMBUS(*detected, config->bin_dir, config->daemon, serial_manager_, serial_override);
        break;
    case DEVICE_RTL433:
        wmbus = openRTL433(*detected, config->bin_dir, config->daemon, serial_manager_, serial_override);
        break;
    case DEVICE_CUL:
    {
        verbose("(cul) on %s\n", detected->found_file.c_str());
        wmbus = openCUL(*detected, serial_manager_, serial_override);
        break;
    }
    case DEVICE_RC1180:
    {
        verbose("(rc1180) on %s\n", detected->found_file.c_str());
        wmbus = openRC1180(*detected, serial_manager_, serial_override);
        break;
    }
    case DEVICE_UNKNOWN:
        warning("(main) internal error! cannot create an unknown device! exiting!\n");
        if (config->daemon) {
            // If starting as a daemon, wait a bit so that systemd have time to catch up.
            sleep(1);
        }
        exit(1);
        break;
    }

    if (detected->found_device_id != "" &&  !detected->found_tty_override)
    {
        string did = wmbus->getDeviceId();
        if (did != detected->found_device_id && detected->found_type != DEVICE_RTLWMBUS)
        {
            warning("Not the expected dongle (dongle said %s, you said %s!\n", did.c_str(), detected->found_device_id.c_str());
            return NULL;
        }
    }
    wmbus->setDetected(*detected);
    return wmbus;
}

void BusManager::checkForDeadWmbusDevices(Configuration *config)
{
    LOCK_BUS_DEVICES(check_for_bus_devices);

    trace("[MAIN] checking for dead wmbus devices...\n");

    vector<WMBus*> not_working;
    for (auto &w : bus_devices_)
    {
        if (!w->isWorking())
        {
            not_working.push_back(w.get());

            string id = w->getDeviceId();
            if (id != "") id = "["+id+"]";

            notice_timestamp("Lost %s closing %s%s\n",
                   w->device().c_str(),
                   toLowerCaseString(w->type()),
                   id.c_str());

            w->close();
        }
    }

    for (auto w : not_working)
    {
        auto i = bus_devices_.begin();
        while (i != bus_devices_.end())
        {
            if (w == (*i).get())
            {
                // The erased shared_ptr will delete the WMBus object.
                bus_devices_.erase(i);
                break;
            }
            i++;
        }
    }

    if (bus_devices_.size() == 0)
    {
        if (config->single_device_override)
        {
            if (!config->simulation_found)
            {
                // Expect stdin/file to work.
                // Simulation is special since it will self stop the serial manager.
                serial_manager_->expectDevicesToWork();
            }
        }
        else
        {
            if (config->nodeviceexit)
            {
                if (!printed_warning_)
                {
                    notice("No wmbus device detected. Exiting!\n");
                    serial_manager_->stop();
                    printed_warning_ = true;
                }
            }
            else
            {
                if (!printed_warning_)
                {
                    info("No wmbus device detected, waiting for a device to be plugged in.\n");
                    checkIfMultipleWmbusMetersRunning();
                    printed_warning_ = true;
                }
            }
        }
    }
    else
    {
        printed_warning_ = false;
    }
}

void BusManager::runAnySimulations()
{
    for (auto &w : bus_devices_)
    {
        // Real devices do nothing, but the simulator device will simulate.
        w->simulate();
    }
}


void BusManager::regularCheckup()
{
    LOCK_BUS_DEVICES(regular_checkup);

    for (auto &w : bus_devices_)
    {
        if (w->isWorking())
        {
            w->checkStatus();
        }
    }
}


void BusManager::detectAndConfigureWmbusDevices(Configuration *config, DetectionType dt)
{
    checkForDeadWmbusDevices(config);

    bool must_auto_find_ttys = false;
    bool must_auto_find_rtlsdrs = false;

    // The device=auto has been specified....
    if (config->use_auto_device_detect && dt == DetectionType::ALL)
    {
        must_auto_find_ttys = true;
        must_auto_find_rtlsdrs = true;
    }

    for (SpecifiedDevice &specified_device : config->supplied_bus_devices)
    {
        specified_device.handled = false;
        if (dt != DetectionType::ALL)
        {
            if (specified_device.is_tty || (!specified_device.is_stdin && !specified_device.is_file && !specified_device.is_simulation))
            {
                // The event loop has not yet started and this is not stdin nor a file, nor a simulation file.
                // Therefore, do not try to detect it yet!
                continue;
            }
        }
        if (specified_device.file == "" && specified_device.command == "")
        {
            // File/tty/command not specified, use auto scan later to find actual device file/tty.
            must_auto_find_ttys |= usesTTY(specified_device.type);
            must_auto_find_rtlsdrs |= usesRTLSDR(specified_device.type);
            continue;
        }
        if (specified_device.command != "")
        {
            string identifier = "cmd_"+to_string(specified_device.index);
            shared_ptr<SerialDevice> sd = serial_manager_->lookup(identifier);
            if (sd != NULL)
            {
                trace("(main) command %s already configured\n", identifier.c_str());
                specified_device.handled = true;
                continue;
            }
            Detected detected = detectWMBusDeviceWithCommand(specified_device, config->default_device_linkmodes, serial_manager_);
            specified_device.handled = true;
            openBusDeviceAndPotentiallySetLinkmodes(config, "config", &detected);
        }
        if (specified_device.file != "")
        {
            shared_ptr<SerialDevice> sd = serial_manager_->lookup(specified_device.file);
            if (sd != NULL)
            {
                trace("(main) %s already configured\n", sd->device().c_str());
                specified_device.handled = true;
                continue;
            }
            if (simulation_files_.count(specified_device.file) > 0)
            {
                debug("(main) %s already configured as simulation\n", specified_device.file.c_str());
                specified_device.handled = true;
                continue;
            }
            if (do_not_open_file_again_.count(specified_device.file) > 0)
            {
                // This was stdin/file, it should only be opened once.
                trace("[MAIN] ignoring handled file %s\n", specified_device.file.c_str());
                specified_device.handled = true;
                continue;
            }

            if (not_serial_wmbus_devices_.count(specified_device.file) > 0)
            {
                // Enumerate all serial devices that might connect to a wmbus device.
                vector<string> ttys = serial_manager_->listSerialTTYs();
                // Did a non-wmbus-device get unplugged? Then remove it from the known-not-wmbus-device set.
                remove_lost_serial_devices_from_ignore_list(ttys);
                if (not_serial_wmbus_devices_.count(specified_device.file) > 0)
                {
                    trace("[MAIN] ignoring failed file %s\n", specified_device.file.c_str());
                    specified_device.handled = true;
                    continue;
                }
            }

            if (!checkCharacterDeviceExists(specified_device.file.c_str(), false) &&
                !checkFileExists(specified_device.file.c_str()) &&
                specified_device.file != "stdin")
            {
                trace("Cannot open %s, no such device.\n", specified_device.file.c_str(),
                        specified_device.str().c_str());
                continue;
            }

            Detected detected = detectWMBusDeviceWithFile(specified_device, config->default_device_linkmodes, serial_manager_);

            if (detected.found_type == DEVICE_UNKNOWN)
            {
                if (checkCharacterDeviceExists(specified_device.file.c_str(), false))
                {
                    // Yes, this device actually exists, there is a need to ignore it.
                    not_serial_wmbus_devices_.insert(specified_device.file);
                }
            }

            if (detected.specified_device.is_stdin || detected.specified_device.is_file || detected.specified_device.is_simulation)
            {
                // Only read stdin and files once!
                do_not_open_file_again_.insert(specified_device.file);
            }
            openBusDeviceAndPotentiallySetLinkmodes(config, "config", &detected);
        }

        specified_device.handled = true;
    }

    if (must_auto_find_ttys)
    {
        perform_auto_scan_of_serial_devices(config);
    }

    if (must_auto_find_rtlsdrs)
    {
        perform_auto_scan_of_swradio_devices(config);
    }

    for (shared_ptr<WMBus> &wmbus : bus_devices_)
    {
        assert(wmbus->getDetected() != NULL);
        find_specified_device_and_mark_as_handled(config, wmbus->getDetected());
    }

    for (SpecifiedDevice &specified_device : config->supplied_bus_devices)
    {
        if (dt == DetectionType::ALL && !specified_device.handled)
        {
            time_t last_alarm = specified_device.last_alarm;
            time_t now = time(NULL);

            // If the device is missing, warn once per minute.
            if (now - last_alarm > 60)
            {
                specified_device.last_alarm = now;
                string device = specified_device.str();
                string info = tostrprintf("the device %s is not working", device.c_str());
                logAlarm(Alarm::SpecifiedDeviceNotFound, info);
            }
        }
    }
}

void BusManager::remove_lost_serial_devices_from_ignore_list(vector<string> &devices)
{
    vector<string> to_be_removed;

    // Iterate over the devices known to NOT be wmbus devices.
    for (const string& nots : not_serial_wmbus_devices_)
    {
        auto i = std::find(devices.begin(), devices.end(), nots);
        if (i == devices.end())
        {
            // The device has been removed, therefore
            // we have to forget that the device was not a wmbus device.
            // Since next time someone plugs in a device, it might be a different
            // one getting the same /dev/ttyUSBxx
            to_be_removed.push_back(nots);
        }
    }

    for (string& r : to_be_removed)
    {
        not_serial_wmbus_devices_.erase(r);
    }
}

void BusManager::perform_auto_scan_of_serial_devices(Configuration *config)
{
    // Enumerate all serial devices that might connect to a wmbus device.
    vector<string> ttys = serial_manager_->listSerialTTYs();

    // Did a non-wmbus-device get unplugged? Then remove it from the known-not-wmbus-device set.
    remove_lost_serial_devices_from_ignore_list(ttys);

    for (string& tty : ttys)
    {
        trace("[MAIN] serial device %s\n", tty.c_str());
        if (not_serial_wmbus_devices_.count(tty) > 0)
        {
            trace("[MAIN] skipping already probed not wmbus serial device %s\n", tty.c_str());
            continue;
        }
        if (config->do_not_probe_ttys.count("all") > 0 ||
            config->do_not_probe_ttys.count(tty) > 0)
        {
            trace("[MAIN] not probing forbidden tty %s\n", tty.c_str());
            continue;
        }
        shared_ptr<SerialDevice> sd = serial_manager_->lookup(tty);
        if (!sd)
        {
            // This serial device is not in use, but is there a device on it?
            debug("(main) device %s not currently used, detect contents...\n", tty.c_str());

            // What should the desired linkmodes be? We have no specified device since this an auto detect.
            // But we might have an auto linkmodes?
            LinkModeSet desired_linkmodes = config->auto_device_linkmodes;
            if (desired_linkmodes.empty())
            {
                // Nope, lets fall back on the default_linkmodes.
                desired_linkmodes = config->default_device_linkmodes;
            }
            Detected detected = detectWMBusDeviceOnTTY(tty, desired_linkmodes, serial_manager_);
            if (detected.found_type != DEVICE_UNKNOWN)
            {
                // See if we had a specified device without a file,
                // that matches this detected device.
                bool found = find_specified_device_and_update_detected(config, &detected);
                if (config->use_auto_device_detect || found)
                {
                    // Open the device, only if auto is enabled, or if the device was specified.
                    openBusDeviceAndPotentiallySetLinkmodes(config, found?"config":"auto", &detected);
                }
            }
            else
            {
                // This serial device was something that we could not recognize.
                // A modem, an android phone, a teletype Model 33, etc....
                // Mark this serial device as unknown, to avoid repeated detection attempts.
                not_serial_wmbus_devices_.insert(tty);
                verbose("(main) ignoring %s, it does not respond as any of the supported wmbus devices.\n", tty.c_str());
            }
        }
    }
}

void BusManager::perform_auto_scan_of_swradio_devices(Configuration *config)
{
    // Enumerate all swradio devices, that can be used.
    vector<string> serialnrs = listRtlSdrDevices();

    if (serialnrs.size() > 0)
    {
        if (!rtlsdr_found_ || !rtlwmbus_found_)
        {
            rtlsdr_found_ = check_if_rtlsdr_exists_in_path();
            rtlwmbus_found_ = check_if_rtlwmbus_exists_in_path();
//            rtl433_found_ = check_if_rtl433_exists_in_path();
        }
        if (!rtlsdr_found_)
        {
            warning("Warning! Auto scan has found an rtl_sdr dongle, but you have no rtl_sdr in the path!\n");
        }
        if (!rtlwmbus_found_)
        {
            warning("Warning! Auto scan has found an rtl_sdr dongle, but you have no rtl_wmbus in the path!\n");
        }
    }

    // We are missing rtl_sdr and/or rtl_wmbus, stop here.
    if (!rtlsdr_found_ || !rtlwmbus_found_) return;

    // Did an unavailable swradio-device get unplugged? Then remove it from the known-not-swradio-device set.
    remove_lost_swradio_devices_from_ignore_list(serialnrs);

    for (string& serialnr : serialnrs)
    {
        trace("[MAIN] rtlsdr device %s\n", serialnr.c_str());
        if (not_swradio_wmbus_devices_.count(serialnr) > 0)
        {
            trace("[MAIN] skipping already probed rtlsdr %s\n", serialnr.c_str());
            continue;
        }
        shared_ptr<SerialDevice> sd = serial_manager_->lookup(serialnr);
        if (!sd)
        {
            debug("(main) rtlsdr device %s not currently used.\n", serialnr.c_str());
            Detected detected;
            detected.specified_device.type = WMBusDeviceType::DEVICE_RTLWMBUS;
            AccessCheck ac = detectRTLSDR(serialnr, &detected);
            if (ac != AccessCheck::AccessOK)
            {
                // We cannot access this swradio device.
                not_swradio_wmbus_devices_.insert(serialnr);
                verbose("(main) ignoring rtlsdr %s since it is unavailable.\n", serialnr.c_str());
            }
            else
            {
                // Use the serialnr as the id.
                detected.found_device_id = serialnr;
                bool found = find_specified_device_and_update_detected(config, &detected);
                if (config->use_auto_device_detect || found)
                {
                    // Open the device, only if auto is enabled, or if the device was specified.
                    openBusDeviceAndPotentiallySetLinkmodes(config, found?"config":"auto", &detected);
                }
            }
        }
    }
}

void BusManager::find_specified_device_and_mark_as_handled(Configuration *c, Detected *d)
{
    SpecifiedDevice *sd = find_specified_device_from_detected(c, d);

    if (sd)
    {
        sd->handled = true;
    }
}

bool BusManager::find_specified_device_and_update_detected(Configuration *c, Detected *d)
{
    SpecifiedDevice *sd = find_specified_device_from_detected(c, d);

    if (sd)
    {
        if (sd->type == DEVICE_RTL433 && d->found_type == DEVICE_RTLWMBUS)
        {
            d->found_type = DEVICE_RTL433;
        }

        d->specified_device = *sd;
        debug("(main) found specified device (%s) that matches detected device (%s)\n",
              sd->str().c_str(),
              d->str().c_str());
        return true;
    }

    return false;
}

void BusManager::remove_lost_swradio_devices_from_ignore_list(vector<string> &devices)
{
    vector<string> to_be_removed;

    // Iterate over the devices known to NOT be wmbus devices.
    for (const string& nots : not_swradio_wmbus_devices_)
    {
        auto i = std::find(devices.begin(), devices.end(), nots);
        if (i == devices.end())
        {
            // The device has been removed, therefore
            // we have to forget that the device was not a wmbus device.
            // Since next time someone plugs in a device, it might be a different
            // one getting the same /dev/ttyUSBxx
            to_be_removed.push_back(nots);
        }
    }

    for (string& r : to_be_removed)
    {
        not_swradio_wmbus_devices_.erase(r);
    }
}

SpecifiedDevice *BusManager::find_specified_device_from_detected(Configuration *c, Detected *d)
{
    // Iterate over the supplied devices and look for an exact type+id match.
    // This will find specified devices like: im871a[12345678]
    for (SpecifiedDevice & sd : c->supplied_bus_devices)
    {
        if (sd.file == "" && sd.id != "" && sd.id == d->found_device_id &&
            (sd.type == d->found_type ||
             (sd.type == DEVICE_RTL433 && d->found_type == DEVICE_RTLWMBUS)))
        {
            return &sd;
        }
    }

    // Iterate over the supplied devices and look for a type match.
    // This will find specified devices like: im871a, rtlwmbus
    for (SpecifiedDevice & sd : c->supplied_bus_devices)
    {
        if (sd.file == "" && sd.id == "" &&
            (sd.type == d->found_type ||
             (sd.type == DEVICE_RTL433 && d->found_type == DEVICE_RTLWMBUS)))
        {
            return &sd;
        }
    }

    return NULL;
}

WMBus* BusManager::findBus(string bus_alias)
{
    for (auto &w : bus_devices_)
    {
        if (w->busAlias() == bus_alias) return w.get();
    }
    return NULL;
}

void BusManager::queueSendBusContent(const SendBusContent &sbc)
{
    LOCK_BUS_SEND_QUEUE(queue_content);

    bus_send_queue_.push_back(sbc);

    debug("(bus) queued send %s bus=%s %s\n", toString(sbc.starts_with), sbc.bus.c_str(), sbc.content.c_str());
}

void BusManager::sendQueue()
{
    LOCK_BUS_SEND_QUEUE(send_content);

    for (SendBusContent &sbc : bus_send_queue_)
    {
        WMBus *bus = findBus(sbc.bus);
        if (bus == NULL)
        {
            warning("(bus) could not send content to non-existant bus, %s bus=%s %s\n", toString(sbc.starts_with), sbc.bus.c_str(), sbc.content.c_str());
            continue;
        }
        if (sbc.content.length() > 250*2)
        {
            warning("(bus) could not send too long hex, maximum is 500 hex chars, %s bus=%s %s\n", toString(sbc.starts_with), sbc.bus.c_str(), sbc.content.c_str());
            continue;
        }
        vector<uchar> content;
        bool ok = hex2bin(sbc.content, &content);
        if (!ok)
        {
            warning("(bus) could not send bad hex, %s bus=%s %s\n", toString(sbc.starts_with), sbc.bus.c_str(), sbc.content.c_str());
            continue;
        }
        bus->sendTelegram(sbc.starts_with, content);
        notice("Sent %d bytes to bus %s\n", sbc.content.length()/2, sbc.bus.c_str());
    }
    bus_send_queue_.clear();
}

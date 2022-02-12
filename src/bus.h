/*
 Copyright (C) 2021 Fredrik Öhrström (gpl-3.0-or-later)

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

#ifndef BUS_H_
#define BUS_H_

#include"config.h"
#include"threads.h"
#include"util.h"
#include"units.h"
#include"wmbus.h"

#include<memory>
#include<set>
#include<string>

enum class DetectionType { STDIN_FILE_SIMULATION, ALL };

struct MeterManager;
struct Configuration;

struct BusManager
{
    BusManager(shared_ptr<SerialCommunicationManager> serial_manager,
               shared_ptr<MeterManager> meter_manager);

    void detectAndConfigureWmbusDevices(Configuration *config, DetectionType dt);
    void removeAllBusDevices();
    void checkForDeadWmbusDevices(Configuration *config);
    void openBusDeviceAndPotentiallySetLinkmodes(Configuration *config, string how, Detected *detected);
    shared_ptr<WMBus> createWmbusObject(Detected *detected, Configuration *config);

    void runAnySimulations();
    void regularCheckup();
    void sendQueue();

    int numBusDevices() { return  bus_devices_.size(); }
    WMBus *findBus(string bus_alias);
    void queueSendBusContent(const SendBusContent &sbc);

private:

    void remove_lost_serial_devices_from_ignore_list(vector<string> &devices);
    void perform_auto_scan_of_serial_devices(Configuration *config);
    void perform_auto_scan_of_swradio_devices(Configuration *config);
    void find_specified_device_and_mark_as_handled(Configuration *c, Detected *d);
    bool find_specified_device_and_update_detected(Configuration *c, Detected *d);
    void remove_lost_swradio_devices_from_ignore_list(vector<string> &devices);
    SpecifiedDevice *find_specified_device_from_detected(Configuration *c, Detected *d);


    shared_ptr<SerialCommunicationManager> serial_manager_;
    shared_ptr<MeterManager> meter_manager_;

    // Current active set of wmbus devices that can receive telegrams.
    // This can change during runtime, plugging/unplugging wmbus dongles.
    vector<shared_ptr<WMBus>> bus_devices_;
    RecursiveMutex bus_devices_mutex_;
#define LOCK_BUS_DEVICES(where) WITH(bus_devices_mutex_, bus_devices_mutex, where)

    // Then check if the rtl_sdr and/or rtl_wmbus and/or rtl_433 is available.
    bool rtlsdr_found_ = false;
    bool rtlwmbus_found_ = false;
    bool rtl433_found_ = false;

    // Remember devices that were not detected as wmbus devices.
    // To avoid probing them again and again.
    std::set<std::string> not_serial_wmbus_devices_;

    // The software radio devices are always swradio devices
    // but they might not be available for wmbusmeters.
    std::set<std::string> not_swradio_wmbus_devices_;

    // When manually supplying stdin or a file, then, after
    // it has been read, do not open it again!
    std::set<std::string> do_not_open_file_again_;

    // Store simulation files here.
    std::set<std::string> simulation_files_;

    // Queue with bus content to send.
    std::vector<SendBusContent> bus_send_queue_;
    RecursiveMutex bus_send_queue_mutex_;
#define LOCK_BUS_SEND_QUEUE(where) WITH(bus_send_queue_mutex_, bus_send_queue_mutex, where)

    // Set as true when the warning for no detected wmbus devices has been printed.
    bool printed_warning_ = false;
};

shared_ptr<BusManager> createBusManager(shared_ptr<SerialCommunicationManager> serial_manager,
                                        shared_ptr<MeterManager> meter_manager);

#endif

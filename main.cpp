#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstring>
#include <ctime>
#include <hidapi/hidapi.h>
#include "supported_devices.h"

// App metadata
std::string APP_VERSION = "v1.0.0";
std::string APP_AUTHOR = "modular-arisu";

/**
 * @brief Sends a feature report and waits for sync response.
 * @return true if both send and get succeed, false otherwise.
 */
bool send_and_sync(hid_device* handle, const unsigned char* data) {
    unsigned char buf[65];
    memset(buf, 0, sizeof(buf));

    buf[0] = 0x00; // Report ID
    memcpy(&buf[1], data, 64);

    // Send Set_Report
    if (hid_send_feature_report(handle, buf, 65) < 0) {
        fprintf(stderr, "[%s] Send failed: %ls\n", "ERROR", hid_error(handle));
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Get Get_Report to sync
    if (hid_get_feature_report(handle, buf, 65) < 0) {
        fprintf(stderr, "[%s] Read sync failed: %ls\n", "ERROR", hid_error(handle));
        return false;
    }

    return true;
}

void get_now(struct tm* result) {
    time_t t = time(NULL);
#ifdef _WIN32
    localtime_s(result, &t);
#else
    localtime_r(&t, result);
#endif
}

bool sync_time(const char* dev_path) {
    hid_device* handle = hid_open_path(dev_path);
    if (!handle) {
        std::cerr << "Failed to open device handle." << std::endl;
        return false;
    }

    bool success = true;

    // Step 1: Init 1
    unsigned char init1[64] = { 0x04, 0x18 };
    if (!send_and_sync(handle, init1)) success = false;

    // Step 2: Init 2
    if (success) {
        unsigned char init2[64] = { 0x04, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
        if (!send_and_sync(handle, init2)) success = false;
    }

    // Step 3: Time Sync
    struct tm now;
    if (success) {
        get_now(&now);
        unsigned char time_pkt[64] = { 0, };
        time_pkt[1] = 0x01;
        time_pkt[2] = 0x5a;
        time_pkt[3] = (unsigned char)(now.tm_year % 100);
        time_pkt[4] = (unsigned char)(now.tm_mon + 1);
        time_pkt[5] = (unsigned char)now.tm_mday;
        time_pkt[6] = (unsigned char)now.tm_hour;
        time_pkt[7] = (unsigned char)now.tm_min;
        time_pkt[8] = (unsigned char)now.tm_sec;
        time_pkt[10] = 0x21;
        time_pkt[11] = 0x01;
        time_pkt[13] = 0x04;
        time_pkt[62] = 0xaa;
        time_pkt[63] = 0x55;

        if (!send_and_sync(handle, time_pkt)) success = false;
    }

    // Step 4: Apply
    if (success) {
        unsigned char apply_pkt[64] = { 0x04, 0x02 };
        if (!send_and_sync(handle, apply_pkt)) success = false;
    }

    // 4. Final Result
    if (success) {
        printf("Sync Completed Successfully: %04d-%02d-%02d %02d:%02d:%02d\n",
            now.tm_year + 1900, now.tm_mon + 1, now.tm_mday,
            now.tm_hour, now.tm_min, now.tm_sec);
    }
    else {
        std::cerr << "Sync failed during protocol steps." << std::endl;
    }

    hid_close(handle);
    return success;
}

int main(int argc, char* argv[]) {
    printf("Archon keyboard LCD clock synchronization tool (%s) by %s\n\n",
        APP_VERSION.c_str(), APP_AUTHOR.c_str());

    unsigned short target_vid = 0x0;
    unsigned short target_pid = 0x0;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--vid" && i + 1 < argc) {
            target_vid = (unsigned short)std::stoul(argv[++i], nullptr, 16);
        }
        else if (arg == "--pid" && i + 1 < argc) {
            target_pid = (unsigned short)std::stoul(argv[++i], nullptr, 16);
        }
    }

    if (hid_init()) {
        std::cerr << "Failed to init hidapi library." << std::endl;
        return -1;
    }

    std::cout << target_vid << std::endl;
    std::cout << target_pid << std::endl;


    struct hid_device_info* devs;
    struct hid_device_info* cur_dev;
    devs = hid_enumerate(0x0, 0x0);

    std::vector<detected_device> detected_devices;

    for (cur_dev = devs; cur_dev != nullptr; cur_dev = cur_dev->next) {
        for (const auto& supported : SUPPORTED_DEVICES) {

            // Is on the supported device list
            if (cur_dev->vendor_id == supported.vid && cur_dev->product_id == supported.pid) {

                // Knock to see if interface is open to set/get feature
                hid_device* handle = hid_open_path(cur_dev->path);
                if (handle) {
                    unsigned char probe_buf[65] = { 0 };
                    if (hid_get_feature_report(handle, probe_buf, 65) > 0) {
                        // Is open
                        detected_device dd;
                        dd.path = cur_dev->path;
                        dd.vid = cur_dev->vendor_id;
                        dd.pid = cur_dev->product_id;
                        dd.model_name = supported.model_name;
                        detected_devices.push_back(dd);
                    }
                    hid_close(handle);
                }
            }
        }
    }

    hid_free_enumeration(devs);

    for (const auto& dev : detected_devices) {
        std::cout << dev.path;
        std::cout << "\n";
        sync_time(dev.path.c_str());
    }

    hid_exit();
}
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstring>
#include <ctime>
#include <hidapi/hidapi.h>

// Device Definitions
struct DeviceConfig {
    uint16_t vid;
    uint16_t pid;
    int interface_no;
    const char* name;
};

const DeviceConfig AK74_CONF = { 0x0C45, 0x800A, 3, "AK74" };
const DeviceConfig AK47_CONF = { 0x0C45, 0x7403, 3, "AK47" }; // AK47용 PID/Interface는 예시이므로 확인 필요

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

int main(int argc, char* argv[]) {
    // 1. Parse Arguments
    DeviceConfig selected = AK74_CONF; // Default
    if (argc > 1) {
        if (strcmp(argv[1], "--ak47") == 0) {
            selected = AK47_CONF;
            printf("AK47 is not supported yet. Head to https://github.com/");
        }
        else if (strcmp(argv[1], "--ak74") == 0) {
            selected = AK74_CONF;
        }
    }

    printf("Target Device: %s (VID:0x%04X, PID:0x%04X, IF:%d)\n",
        selected.name, selected.vid, selected.pid, selected.interface_no);

    if (hid_init()) return -1;

    // 2. Enumerate and Find Path
    struct hid_device_info* devs, * cur_dev;
    devs = hid_enumerate(selected.vid, selected.pid);

    char* target_path = nullptr;
    for (cur_dev = devs; cur_dev != nullptr; cur_dev = cur_dev->next) {
        if (cur_dev->interface_number == selected.interface_no) {
#ifdef _WIN32
            target_path = _strdup(cur_dev->path);
#else
            target_path = strdup(cur_dev->path);
#endif
            break;
        }
    }

    if (!target_path) {
        std::cerr << "Device interface not found!" << std::endl;
        hid_free_enumeration(devs);
        hid_exit();
        return -1;
    }

    hid_device* handle = hid_open_path(target_path);
    free(target_path);
    hid_free_enumeration(devs);

    if (!handle) {
        std::cerr << "Failed to open device handle." << std::endl;
        hid_exit();
        return -1;
    }

    // 3. Protocol Execution with Error Checking
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
    hid_exit();
    return success ? 0 : -1;
}
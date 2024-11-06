#include <pcap/pcap.h>
#include <pcap/can_socketcan.h>
#include <pcap/bluetooth.h>
#include <pcap/ipnet.h>
#include <pcap/usb.h>
#include <pcap/vlan.h>
#include <pcap/sll.h>
#include <pcap/nflog.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <memory>

// Function to safely copy a string from fuzz input
char* safe_strndup(const uint8_t* data, size_t size) {
    if (size == 0) return nullptr;
    char* str = (char*)malloc(size + 1);
    if (!str) return nullptr;
    memcpy(str, data, size);
    str[size] = '\0';
    return str;
}

// Function to safely allocate memory for pcap_pkthdr
struct pcap_pkthdr* safe_pcap_pkthdr_alloc(size_t size) {
    if (size < sizeof(struct pcap_pkthdr)) return nullptr;
    return (struct pcap_pkthdr*)malloc(sizeof(struct pcap_pkthdr));
}

// Function to safely allocate memory for packet data
uint8_t* safe_packet_data_alloc(size_t size) {
    if (size == 0) return nullptr;
    return (uint8_t*)malloc(size);
}

// Main fuzzing function
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // Ensure we have enough data for basic operations
    if (size < sizeof(struct pcap_pkthdr) + 1) return 0;

    // Extract file name from fuzz input
    size_t filename_size = size / 2;
    char* filename = safe_strndup(data, filename_size);
    if (!filename) return 0;

    // Extract packet header and data from fuzz input
    struct pcap_pkthdr* pkthdr = safe_pcap_pkthdr_alloc(size);
    if (!pkthdr) {
        free(filename);
        return 0;
    }
    memcpy(pkthdr, data + filename_size, sizeof(struct pcap_pkthdr));

    size_t packet_data_size = size - filename_size - sizeof(struct pcap_pkthdr);
    uint8_t* packet_data = safe_packet_data_alloc(packet_data_size);
    if (!packet_data) {
        free(filename);
        free(pkthdr);
        return 0;
    }
    memcpy(packet_data, data + filename_size + sizeof(struct pcap_pkthdr), packet_data_size);

    // Open pcap file for appending
    pcap_t* pcap = pcap_open_dead(DLT_EN10MB, 65535);
    if (!pcap) {
        free(filename);
        free(pkthdr);
        free(packet_data);
        return 0;
    }

    pcap_dumper_t* dumper = pcap_dump_open_append(pcap, filename);
    if (!dumper) {
        pcap_close(pcap);
        free(filename);
        free(pkthdr);
        free(packet_data);
        return 0;
    }

    // Dump packet to file
    pcap_dump((u_char*)dumper, pkthdr, packet_data);

    // Flush the dump file
    if (pcap_dump_flush(dumper) != 0) {
        pcap_dump_close(dumper);
        pcap_close(pcap);
        free(filename);
        free(pkthdr);
        free(packet_data);
        return 0;
    }

    // Close the dump file
    pcap_dump_close(dumper);
    pcap_close(pcap);

    // Free allocated resources
    free(filename);
    free(pkthdr);
    free(packet_data);

    return 0;
}
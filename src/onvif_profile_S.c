#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "soapStub.h"
#include "soapDeviceBindingProxy.h"
#include "wsddapi.h"

// Function to discover ONVIF devices on the network
void discover_onvif_devices() {
    struct soap *soap = soap_new();
    struct wsdd__ProbeType req;
    struct __wsdd__ProbeMatches resp;
    soap_default_wsdd__ProbeType(soap, &req);
    req.Scopes = NULL;
    req.Types = "dn:NetworkVideoTransmitter";

    if (soap_send___wsdd__Probe(soap, SOAP_MCAST_ADDR, NULL, &req) == SOAP_OK) {
        while (soap_recv___wsdd__ProbeMatches(soap, &resp) == SOAP_OK) {
            if (resp.wsdd__ProbeMatches) {
                for (int i = 0; i < resp.wsdd__ProbeMatches->__sizeProbeMatch; i++) {
                    printf("Device found: %s\n", resp.wsdd__ProbeMatches->ProbeMatch[i].XAddrs);
                }
            }
        }
    } else {
        soap_print_fault(soap, stderr);
    }
    soap_destroy(soap);
    soap_end(soap);
    soap_free(soap);
}

// Function to configure ONVIF device IP address
void configure_onvif_device_ip(const char *device_xaddr, const char *new_ip) {
    struct soap *soap = soap_new();
    struct _tds__SetNetworkInterfaces req;
    struct _tds__SetNetworkInterfacesResponse resp;
    struct tt__NetworkInterfaceSetConfiguration config;

    soap_default__tds__SetNetworkInterfaces(soap, &req);
    soap_default_tt__NetworkInterfaceSetConfiguration(soap, &config);

    config.Enabled = soap_malloc(soap, sizeof(enum xsd__boolean));
    *config.Enabled = xsd__boolean__true_;
    config.IPv4 = soap_malloc(soap, sizeof(struct tt__IPv4NetworkInterfaceSetConfiguration));
    soap_default_tt__IPv4NetworkInterfaceSetConfiguration(soap, config.IPv4);
    config.IPv4->Manual = soap_malloc(soap, sizeof(struct tt__PrefixedIPv4Address));
    soap_default_tt__PrefixedIPv4Address(soap, config.IPv4->Manual);
    config.IPv4->Manual->Address = soap_strdup(soap, new_ip);
    config.IPv4->Manual->PrefixLength = 24;
    config.IPv4->DHCP = xsd__boolean__false_;

    req.InterfaceToken = "eth0";
    req.NetworkInterface = &config;

    if (soap_call___tds__SetNetworkInterfaces(soap, device_xaddr, NULL, &req, &resp) == SOAP_OK) {
        printf("IP address changed to %s\n", new_ip);
    } else {
        soap_print_fault(soap, stderr);
    }
    soap_destroy(soap);
    soap_end(soap);
    soap_free(soap);
}

int main() {

    // Discover ONVIF devices
    discover_onvif_devices();

    // Example: Configure a specific device (replace with actual device XAddr and new IP)
    const char *device_xaddr = "http://<device_ip>/onvif/device_service"; // Device XAddr
    const char *new_ip = "192.168.1.100"; // New IP address to assign to the device
    configure_onvif_device_ip(device_xaddr, new_ip); // Change IP address of device

    return 0;
}
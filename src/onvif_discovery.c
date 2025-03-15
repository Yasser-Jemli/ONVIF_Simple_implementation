#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#define WS_DISCOVERY_PORT 3702
#define BUFFER_SIZE 4096
#define WS_DISCOVERY_ADDRESS "239.255.255.250"

typedef struct Device {
    char ip[INET_ADDRSTRLEN];
    char service_url[256];
    char firmware_version[256];
    char sw_version[256];
    char profiles_supported[256];
    char capabilities[256];
    struct Device *next;
} Device;

// Global Variables 
Device *device_list = NULL;
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;

// Probe message
const char *probe_message =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<e:Envelope xmlns:e=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:w=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\""
    " xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\""
    " xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\">"
    "  <e:Header>"
    "    <w:MessageID>uuid:84ede3de-7dec-11d0-c360-f01234567890</w:MessageID>"
    "    <w:To e:mustUnderstand=\"true\">urn:schemas-xmlsoap-org:ws:2005:04:discovery</w:To>"
    "    <w:Action e:mustUnderstand=\"true\">"
    "      http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe"
    "    </w:Action>"
    "  </e:Header>"
    "  <e:Body>"
    "    <d:Probe>"
    "      <d:Types>dn:NetworkVideoTransmitter</d:Types>"
    "    </d:Probe>"
    "  </e:Body>"
    "</e:Envelope>";

void extract_service_url(xmlNodePtr node, char *service_url) {
    xmlNodePtr child = node->children;
    while (child) {
        if (child->type == XML_ELEMENT_NODE && xmlStrcmp(child->name, (const xmlChar *)"XAddrs") == 0) {
            xmlChar *url = xmlNodeGetContent(child);
            strncpy(service_url, (const char *)url, 256);
            xmlFree(url);
            return;
        }
        child = child->next;
    }
}

void extract_firmware_version(xmlNodePtr node, char *firmware_version) {
    xmlNodePtr child = node->children;
    while (child) {
        if (child->type == XML_ELEMENT_NODE && xmlStrcmp(child->name, (const xmlChar *)"FirmwareVersion") == 0) {
            xmlChar *version = xmlNodeGetContent(child);
            strncpy(firmware_version, (const char *)version, 256);
            xmlFree(version);
            return;
        }
        child = child->next;
    }
}

void extract_sw_version(xmlNodePtr node, char *sw_version) {
    xmlNodePtr child = node->children;
    while (child) {
        if (child->type == XML_ELEMENT_NODE && xmlStrcmp(child->name, (const xmlChar *)"SoftwareVersion") == 0) {
            xmlChar *version = xmlNodeGetContent(child);
            strncpy(sw_version, (const char *)version, 256);
            xmlFree(version);
            return;
        }
        child = child->next;
    }
}

void extract_profiles_supported(xmlNodePtr node, char *profiles_supported) {
    xmlNodePtr child = node->children;
    while (child) {
        if (child->type == XML_ELEMENT_NODE && xmlStrcmp(child->name, (const xmlChar *)"Scopes") == 0) {
            xmlChar *scopes = xmlNodeGetContent(child);
            strncpy(profiles_supported, (const char *)scopes, 256);
            xmlFree(scopes);
            return;
        }
        child = child->next;
    }
}

void extract_capabilities(xmlNodePtr node, char *capabilities) {
    xmlNodePtr child = node->children;
    while (child) {
        if (child->type == XML_ELEMENT_NODE && xmlStrcmp(child->name, (const xmlChar *)"Capabilities") == 0) {
            xmlChar *caps = xmlNodeGetContent(child);
            strncpy(capabilities, (const char *)caps, 256);
            xmlFree(caps);
            return;
        }
        child = child->next;
    }
}

void add_device(const char *ip, const char *service_url, const char *firmware_version, const char *sw_version, const char *profiles_supported, const char *capabilities) {
    pthread_mutex_lock(&list_mutex);
    Device *new_device = (Device *)malloc(sizeof(Device));
    if (!new_device) {
        fprintf(stderr, "Memory allocation failed\n");
        pthread_mutex_unlock(&list_mutex);
        return;
    }
    strncpy(new_device->ip, ip, INET_ADDRSTRLEN);
    strncpy(new_device->service_url, service_url, 256);
    strncpy(new_device->firmware_version, firmware_version, 256);
    strncpy(new_device->sw_version, sw_version, 256);
    strncpy(new_device->profiles_supported, profiles_supported, 256);
    strncpy(new_device->capabilities, capabilities, 256);
    new_device->next = device_list;
    device_list = new_device;
    pthread_mutex_unlock(&list_mutex);
}

void print_device_table() {
    pthread_mutex_lock(&list_mutex);
    Device *current = device_list;
    printf("\n%-20s %-40s %-20s %-20s %-20s %-20s\n", "IP Address", "Service URL", "Firmware Version", "SW Version", "Profiles Supported", "Capabilities");
    printf("----------------------------------------------------------------------------------------------------------------------------\n");
    while (current) {
        printf("%-20s %-40s %-20s %-20s %-20s %-20s\n", current->ip, current->service_url, current->firmware_version, current->sw_version, current->profiles_supported, current->capabilities);
        current = current->next;
    }
    pthread_mutex_unlock(&list_mutex);
}

void *listen_for_responses(void *arg) {
    int sock;
    struct sockaddr_in recv_addr;
    char buffer[BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return NULL;
    }

    // Enable address reuse
    int optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(WS_DISCOVERY_PORT);
    recv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)&recv_addr, sizeof(recv_addr)) < 0) {
        perror("Bind failed");
        close(sock);
        return NULL;
    }

    while (1) {
        struct sockaddr_in sender_addr;
        socklen_t addr_len = sizeof(sender_addr);
        ssize_t recv_len = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, 
                                    (struct sockaddr *)&sender_addr, &addr_len);
        if (recv_len > 0) {
            buffer[recv_len] = '\0';
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &sender_addr.sin_addr, ip, sizeof(ip));
            printf("Received message from %s:\n%s\n\n\n", ip, buffer);

            // Parse the XML response to handle multiple ProbeMatch messages
            xmlDocPtr doc = xmlReadMemory(buffer, recv_len, "noname.xml", NULL, 0);
            if (doc == NULL) {
                fprintf(stderr, "Failed to parse XML response\n");
                continue;
            }

            xmlNodePtr root = xmlDocGetRootElement(doc);
            xmlNodePtr body = root->children;
            while (body && (body->type != XML_ELEMENT_NODE || xmlStrcmp(body->name, (const xmlChar *)"Body") != 0)) {
                body = body->next;
            }

            if (body) {
                xmlNodePtr probe_matches = body->children;
                while (probe_matches) {
                    if (probe_matches->type == XML_ELEMENT_NODE && xmlStrcmp(probe_matches->name, (const xmlChar *)"ProbeMatches") == 0) {
                        xmlNodePtr probe_match = probe_matches->children;
                        while (probe_match) {
                            if (probe_match->type == XML_ELEMENT_NODE && xmlStrcmp(probe_match->name, (const xmlChar *)"ProbeMatch") == 0) {
                                char service_url[256] = {0};
                                char firmware_version[256] = {0};
                                char sw_version[256] = {0};
                                char profiles_supported[256] = {0};
                                char capabilities[256] = {0};
                                extract_service_url(probe_match, service_url);
                                extract_firmware_version(probe_match, firmware_version);
                                extract_sw_version(probe_match, sw_version);
                                extract_profiles_supported(probe_match, profiles_supported);
                                extract_capabilities(probe_match, capabilities);
                                add_device(ip, service_url, firmware_version, sw_version, profiles_supported, capabilities);
                            }
                            probe_match = probe_match->next;
                        }
                    }
                    probe_matches = probe_matches->next;
                }
            }

            xmlFreeDoc(doc);
            xmlCleanupParser();
        }
    }
    close(sock);
    return NULL;
}

void send_discovery_probe() {
    int sock;
    struct sockaddr_in broadcast_addr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }

    // Enable broadcast
    int optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));

    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(WS_DISCOVERY_PORT);
    inet_pton(AF_INET, WS_DISCOVERY_ADDRESS, &broadcast_addr.sin_addr);

    if (sendto(sock, probe_message, strlen(probe_message), 0, 
               (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
        perror("Failed to send discovery probe");
    } else {
        printf("Discovery probe sent.\n\n");
    }

    close(sock);
}

void cleanup_devices() {
    pthread_mutex_lock(&list_mutex);
    Device *current = device_list;
    while (current) {
        Device *next = current->next;
        free(current);
        current = next;
    }
    device_list = NULL;
    pthread_mutex_unlock(&list_mutex);
}

int main() {
    pthread_t listener_thread;

    if (pthread_create(&listener_thread, NULL, listen_for_responses, NULL) != 0) {
        fprintf(stderr, "Failed to create listener thread\n");
        return 1;
    }

    printf("Press ENTER to start discovery...\n");
    getchar();
    send_discovery_probe();

    sleep(5);  // Give time to receive responses

    pthread_cancel(listener_thread);
    pthread_join(listener_thread, NULL);

    print_device_table();  // Print the table of discovered devices
    cleanup_devices();  // Free allocated memory

    return 0;
}
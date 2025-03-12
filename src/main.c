#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#define WS_DISCOVERY_ADDRESS "239.255.255.250"
#define WS_DISCOVERY_PORT 3702

// #define WS_DISCOVERY_ADDRESS    "127.0.0.1"

// #define WS_DISCOVERY_ADDRESS "192.168.23.14"
// #define WS_DISCOVERY_PORT 8000
#define BUFFER_SIZE 4096

typedef struct Device {
    char ip[INET_ADDRSTRLEN];
    char service_url[256];
    struct Device *next;
} Device;

Device *device_list = NULL;
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;

// const char *probe_message =
//     "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
//     "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:a=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\">"
//     "  <s:Header>"
//     "    <a:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</a:Action>"
//     "    <a:MessageID>uuid:12345678-1234-1234-1234-123456789012</a:MessageID>"
//     "    <a:ReplyTo>"
//     "      <a:Address>urn:schemas-xmlsoap-org:ws:2005:04:discovery</a:Address>"
//     "    </a:ReplyTo>"
//     "  </s:Header>"
//     "  <s:Body>"
//     "    <d:Probe/>"
//     "  </s:Body>"
//     "</s:Envelope>";


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


void add_device(const char *ip, const char *service_url) {
    pthread_mutex_lock(&list_mutex);
    Device *new_device = (Device *)malloc(sizeof(Device));
    if (!new_device) {
        fprintf(stderr, "Memory allocation failed\n");
        pthread_mutex_unlock(&list_mutex);
        return;
    }
    strncpy(new_device->ip, ip, INET_ADDRSTRLEN);
    strncpy(new_device->service_url, service_url, 256);
    new_device->next = device_list;
    device_list = new_device;
    pthread_mutex_unlock(&list_mutex);
}

void extract_service_url(const char *response, char *service_url) {
    xmlDocPtr doc = xmlReadMemory(response, strlen(response), "noname.xml", NULL, 0);
    if (doc == NULL) {
        fprintf(stderr, "Failed to parse XML response\n");
        strcpy(service_url, "Unknown");
        return;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    xmlNodePtr node = root;

    // Recursively search for the <d:XAddrs> tag
    while (node) {
        if (node->type == XML_ELEMENT_NODE && xmlStrcmp(node->name, (const xmlChar *)"XAddrs") == 0) {
            xmlChar *content = xmlNodeGetContent(node);
            if (content) {
                strncpy(service_url, (const char *)content, 255);
                service_url[255] = '\0'; // Ensure null termination
                xmlFree(content);
                break;
            }
        }
        node = (node->next) ? node->next : node->children;
    }

    xmlFreeDoc(doc);
    xmlCleanupParser();
}

void *listen_for_responses(void *arg) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return NULL;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(WS_DISCOVERY_PORT);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sock);
        return NULL;
    }

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int recv_len = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_len);
        if (recv_len > 0) {
            buffer[recv_len] = '\0';
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
            char service_url[256] = {0};
            extract_service_url(buffer, service_url);
            add_device(ip, service_url);
            printf("Discovered ONVIF Device: %s (%s)\n", ip, service_url);
        }
    }
    close(sock);
    return NULL;
}

void send_discovery_probe() {
    int sock;
    struct sockaddr_in multicast_addr;
    
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }

    memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_port = htons(WS_DISCOVERY_PORT);
    multicast_addr.sin_addr.s_addr = inet_addr(WS_DISCOVERY_ADDRESS);

    if (sendto(sock, probe_message, strlen(probe_message), 0, (struct sockaddr *)&multicast_addr, sizeof(multicast_addr)) < 0) {
        perror("Failed to send discovery probe");
    } else {
        printf("Discovery probe sent.\n");
    }

    close(sock);
}

void list_devices() {
    pthread_mutex_lock(&list_mutex);
    Device *current = device_list;
    printf("\n--- Available ONVIF Devices ---\n");
    while (current) {
        printf("IP: %s, Service URL: %s\n", current->ip, current->service_url);
        current = current->next;
    }
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

    printf("\nPress ENTER to list discovered devices...\n");
    getchar();
    list_devices();

    pthread_cancel(listener_thread);
    pthread_join(listener_thread, NULL);

    return 0;
} 
#include <stdio.h>
#include <curl/curl.h>


#define URL "https://example.com/"


int main(void){
    CURL *curl_handler;
    CURLcode res;
    curl_global_init(CURL_GLOBAL_ALL); // this not a thread safe so you need to use it in single threaded programe
    curl_handler = curl_easy_init();
    
    if (curl_handler) {
        curl_easy_setopt(curl_handler, CURLOPT_URL, URL);
        res = curl_easy_perform(curl_handler);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl_handler);
    }

    curl_global_cleanup(); 

    return 0;
}
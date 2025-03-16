#include <curl/curl.h>

int main(void){

    CURL *curl_handler;
    curl_global_init(CURL_GLOBAL_ALL); // this not a threaded safe so you need to use it in single threaded programe
    curl_handler = curl_easy_init();


    curl_easy_cleanup(curl_handler);
    curl_global_cleanup(); 

    return 0;
}
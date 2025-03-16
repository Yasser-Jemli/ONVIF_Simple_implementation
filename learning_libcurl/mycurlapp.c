// you need to install libcurl if not available on your system 
// on ubuntu : sudo apt install libcurl4-openssl-dev
#include <curl/curl.h>


int main(void){

    CURL *curl =   curl_easy_init();
    curl_easy_cleanup ; 

    return 0;
}
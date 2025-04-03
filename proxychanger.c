#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <unistd.h>
#include <time.h>

#define PROXY_URL "https://api.proxyscrape.com/v2/?request=getproxies&protocol=http&timeout=10000&country=all"
#define MAX_PROXIES 100
#define BUFFER_SIZE 20000
#define SWITCH_TIME 1200 // 20 minutes
#define FISH_CONFIG_PATH "/home/User/.config/fish/config.fish"

typedef struct {
    char ip[50];
} Proxy;

// structure to handle the curl response
typedef struct {
    char *data;
    size_t size;
} MemoryStruct;

// write the data received to memory
size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    MemoryStruct *mem = (MemoryStruct *)userdata;
    size_t total_size = size * nmemb;

    if (mem->size + total_size >= BUFFER_SIZE - 1) {
        fprintf(stderr, "Error: Response too large for buffer\n");
        return 0;
    }

    strncat(mem->data, (char *)ptr, total_size);
    mem->size += total_size;
    return total_size;
}

// test if the proxy works
int test_proxy(const char *proxy) {
    CURL *curl;
    CURLcode res;
    long response_code = 0;
    
    curl = curl_easy_init();
    if (!curl) return 0;
    
    char proxy_url[100];
    snprintf(proxy_url, sizeof(proxy_url), "http://%s", proxy);
    
    curl_easy_setopt(curl, CURLOPT_URL, "http://httpbin.org/ip");
    curl_easy_setopt(curl, CURLOPT_PROXY, proxy_url);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    
    res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    curl_easy_cleanup(curl);
    
    return (res == CURLE_OK && response_code == 200);
}

// download proxy list from ProxyScrape
int fetch_proxies(Proxy proxies[], int *count) {
    CURL *curl;
    CURLcode res;
    
    char *buffer = (char *)malloc(BUFFER_SIZE);
    if (!buffer) {
        fprintf(stderr, "Memory error\n");
        return 0;
    }
    
    memset(buffer, 0, BUFFER_SIZE);
    MemoryStruct chunk = { buffer, 0 };

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, PROXY_URL);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "Error downloading proxies: %s\n", curl_easy_strerror(res));
            free(buffer);
            return 0;
        }
    } else {
        fprintf(stderr, "Error initializing cURL\n");
        free(buffer);
        return 0;
    }

    // Process received proxies
    char *line = strtok(buffer, "\r\n");
    *count = 0;
    while (line && *count < MAX_PROXIES) {
        snprintf(proxies[*count].ip, sizeof(proxies[*count].ip), "%s", line);
        (*count)++;
        line = strtok(NULL, "\r\n");
    }

    free(buffer);
    return *count;
}

// set the proxy in fish
void set_fish_proxy(const char *proxy) {
    FILE *file = fopen(FISH_CONFIG_PATH, "r");
    if (!file) {
        fprintf(stderr, "Cannot open %s for reading.\n", FISH_CONFIG_PATH);
        return;
    }

    char line[256];
    char new_config[10000] = "";
    int proxy_section = 0;

    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "# Automatic proxies by ProxyChanger")) {
            proxy_section = 1;
            continue;
        }
        if (proxy_section && strstr(line, "set -x")) {
            continue;
        }
        strncat(new_config, line, sizeof(new_config) - strlen(new_config) - 1);
    }
    fclose(file);

    FILE *new_file = fopen(FISH_CONFIG_PATH, "w");
    if (!new_file) {
        fprintf(stderr, "could not open %s for writing.\n", FISH_CONFIG_PATH);
        return;
    }

    fprintf(new_file, "%s", new_config);
    fprintf(new_file, "\n# Automatic proxy added by ProxyChanger\n");
    fprintf(new_file, "set -x http_proxy \"http://%s\"\n", proxy);
    fprintf(new_file, "set -x https_proxy \"http://%s\"\n", proxy);
    fprintf(new_file, "set -x all_proxy \"http://%s\"\n", proxy);
    fclose(new_file);

    printf("global proxy changed to: %s\n", proxy);
    printf("Execute 'source ~/.config/fish/config.fish' in your Fish terminal to aplly changes\n");
    //eror line 
    //system("source ~/.config/fish/config.fish");
}

// helper script to apply the configuration
void create_helper_script() {
    FILE *script = fopen("apply_proxy.fish", "w");
    if (!script) {
        fprintf(stderr, "The helper script could not be created.\n");
        return;
    }

    fprintf(script, "#!/usr/bin/fish\n");
    fprintf(script, "source %s\n", FISH_CONFIG_PATH);
    fprintf(script, "echo \"proxy applied in this terminal session\"\n");
    fprintf(script, "echo \"Actual Prxy: $http_proxy\"\n");
    fclose(script);

    // make the script executable
    system("chmod +x apply_proxy.fish");
    printf("created 'apply_proxy.fish' helper script.\n");
    printf("To apply the proxy in a new terminal, run: fish ./apply_proxy.fish\n");
}

int main() {
    Proxy proxies[MAX_PROXIES];
    int proxy_count = 0, current_proxy = 0;

    // Crear script auxiliar
    create_helper_script();

    printf("Downloading proxy list...\n");
    if (!fetch_proxies(proxies, &proxy_count)) {
        fprintf(stderr, "couldnt get proxies.\n");
        return 1;
    }

    printf(" %d Proxies were obtained.\n", proxy_count);
    
    while (1) {
        if (proxy_count == 0) {
            printf("No proxies available, downloading again...\n");
            if (!fetch_proxies(proxies, &proxy_count)) {
                sleep(60);
                continue;
            }
        }

        // Test proxies
        int found_working_proxy = 0;
        int proxies_tested = 0;
        
        while (!found_working_proxy && proxies_tested < proxy_count) {
            printf("Testing proxy: %s...\n", proxies[current_proxy].ip);
            
            if (test_proxy(proxies[current_proxy].ip)) {
                printf("Proxy works correctly.\n");
                found_working_proxy = 1;
            } else {
                printf("Proxy not working, testing next...\n");
                current_proxy = (current_proxy + 1) % proxy_count;
                proxies_tested++;
            }
        }
        
        if (!found_working_proxy) {
            printf("No proxy works, downloading new proxies...\n");
            proxy_count = 0;
            sleep(30); // wait before trying again
            continue;
        }

        printf("Configuring proxy: %s...\n", proxies[current_proxy].ip);
        set_fish_proxy(proxies[current_proxy].ip);
        
        // keep the proxy for the configured time
        printf("Using this proxy for %d seconds...\n", SWITCH_TIME);
        sleep(SWITCH_TIME);
        
        // move to next proxy
        current_proxy = (current_proxy + 1) % proxy_count;
    }

    return 0;
}

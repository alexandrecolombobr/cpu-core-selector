#include <iostream>
#include <sstream>
#include <fstream>
#include <cstring>
#include <omp.h>
#include <openssl/evp.h>
#include <pthread.h>
#include <libudev.h>

// Function that calculates sha512 hash of some string
void compute_sha512(const unsigned char *input, size_t length, unsigned char *output) {
    EVP_MD_CTX *mdctx;
    const EVP_MD *md;
    unsigned int len;

    // Initialize the OpenSSL library
    if (OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CONFIG, NULL) == 0) {
        std::cerr << "Failed to initialize OpenSSL library" << std::endl;
        return;
    }

    // Create a new digest context
    mdctx = EVP_MD_CTX_new();
    if (mdctx == nullptr) {
        std::cerr << "Failed to create EVP_MD_CTX" << std::endl;
        return;
    }

    // Get the SHA-512 digest
    md = EVP_sha512();
    if (md == nullptr) {
        std::cerr << "Failed to get SHA-512 digest" << std::endl;
        EVP_MD_CTX_free(mdctx);
        return;
    }

    // Initialize the digest context
    if (EVP_DigestInit_ex(mdctx, md, nullptr) != 1) {
        std::cerr << "Failed to initialize digest context" << std::endl;
        EVP_MD_CTX_free(mdctx);
        return;
    }

    // Update the digest with the input data
    if (EVP_DigestUpdate(mdctx, input, length) != 1) {
        std::cerr << "Failed to update digest" << std::endl;
        EVP_MD_CTX_free(mdctx);
        return;
    }

    // Finalize the digest
    if (EVP_DigestFinal_ex(mdctx, output, &len) != 1) {
        std::cerr << "Failed to finalize digest" << std::endl;
        EVP_MD_CTX_free(mdctx);
        return;
    }

    // Clean up
    EVP_MD_CTX_free(mdctx);
}

// Function to pin the current thread to a specific core
void set_thread_affinity(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    pthread_t thread = pthread_self();
    if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) {
        std::cerr << "Failed to set thread affinity for core " << core_id << std::endl;
    }
}

int count_cpulist(const std::string &list) {
    int count = 0;
    std::stringstream ss(list);
    std::string token;

    while (std::getline(ss, token, ',')) {
        size_t dash = token.find('-');

        if (dash == std::string::npos) {
            count += 1;
        } else {
            int start = std::stoi(token.substr(0, dash));
            int end = std::stoi(token.substr(dash + 1));

            count += (end - start + 1);
        }
    }

    return count;
}

std::string get_sysattr(struct udev *udev, const char *path, const char *attr) {
    struct udev_device *dev = udev_device_new_from_syspath(udev, path);

    if (!dev)
        return "";

    const char *value = udev_device_get_sysattr_value(dev, attr);
    std::string result = value ? value : "";
    udev_device_unref(dev);
    return result;
}

std::string detect_power_mode() {
    std::ifstream f("/sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference");

    if (!f.is_open())
        return "unknown";

    std::string value;
    std::getline(f, value);

    if (value == "performance")
        return "performance";

    if (value == "power")
        return "power-saver";

    if (value == "balance_performance" || value == "balance_power")
        return "balanced";

    return value;
}

int first_cpu_from_cpulist(const std::string& list) {
    size_t comma = list.find(',');
    std::string first = list.substr(0, comma);

    size_t dash = first.find('-');

    if (dash == std::string::npos)
        return std::stoi(first);

    return std::stoi(first.substr(0, dash));
}

// This sample program runs dummy hash computations on a single
// thread pinned to a CPU core.
// It detects the active CPU governor and automatically selects
// a P-core in performance mode or an E-core in power saver mode.
int main() {
    // We will use udev to gather system info
    struct udev *udev = udev_new();

    if (!udev) {
        std::cerr << "Failed to create udev context\n";
        return 1;
    }

    // Example string to hash
    const char *text = "Hello, world!";
    size_t length = std::strlen(text);

    // Output buffer for SHA-512 hash
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;

    // My setup is an Intel Core i3 1215u with 8 threads
    // (2+2 P-cores/Hyperthreading and 4 E-cores).
    // The program should detect how many cores are available
    // and their type, and select which one to use based on
    // current CPU governor settings.
    std::string pcore_list = get_sysattr(udev, "/sys/devices/cpu_core", "cpus");
    std::string ecore_list = get_sysattr(udev, "/sys/devices/cpu_atom", "cpus");

    int pcores = count_cpulist(pcore_list);
    int ecores = count_cpulist(ecore_list);
    int total = pcores + ecores;

    std::cout << "CPU topology detected\n";
    std::cout << "---------------------\n";

    std::cout << "Total cores : " << total << "\n";
    std::cout << "P-Cores     : " << pcores << "\n";
    std::cout << "E-Cores     : " << ecores << "\n";

    std::cout << "\nRaw lists\n";
    std::cout << "P-Core CPUs : " << pcore_list << "\n";
    std::cout << "E-Core CPUs : " << ecore_list << "\n";

    // Now the program is going to detect which power
    // profile is current selected
    std::string power_mode = detect_power_mode();
    std::cout << "Power profile : " << power_mode << "\n";
    bool power_mode_is_performance = (power_mode == "performance");

    // We now select which CPU will execute the operation
    int thread = power_mode_is_performance
                     ? first_cpu_from_cpulist(pcore_list)
                     : first_cpu_from_cpulist(ecore_list);

    // Timing variables
    double start_time, end_time;
    start_time = omp_get_wtime();

    // Pin this single thread to a specific core
    std::cout << "Calculating hash on thread " << thread << "\n";
    set_thread_affinity(thread);

    // Compute the SHA-512 hash dummy operation
    compute_sha512(reinterpret_cast<const unsigned char *>(text), length, hash);
    int it = 10000000;
    while (it > 0) {
        compute_sha512(reinterpret_cast<const unsigned char *>(hash), (size_t) EVP_MAX_MD_SIZE, hash);
        it--;
    }

    // End timing
    end_time = omp_get_wtime();

    // Print elapsed time
    std::cout << "Elapsed time: " << (end_time - start_time) << " seconds for thread " << thread << std::endl;

    // Print the hash in hexadecimal format
    std::cout << "SHA-512 hash: ";
    for (int i = 0; i < 64; ++i) {
        printf("%02x", hash[i]);
    }
    std::cout << std::endl;

    udev_unref(udev);

    return 0;
}

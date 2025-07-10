#include <windows.h>
#include <iostream>
#include <string>

#include <chrono>
#include <thread>
#include <filesystem>

#include "DICOMC.h"

int main()
{
    std::cout << "Initializing SCP..." << std::endl;
    LPVOID scp = DICOMWLSPCreate();
    if (!scp) 
    {
        std::cout << "Failed to create SCP instance." << std::endl;
        return 1;
    }

    std::cout << "Setting template file..." << std::endl;
    DICOMWLSPSetTemplateFile(scp, "template.dcm");

    std::cout << "Clearing all datasets..." << std::endl;
    DICOMWLSPClear(scp);

    std::cout << "Adding new dataset..." << std::endl;
    int index = -1;
    if (!DICOMWLSPAddDataset(scp, &index)) 
    {
        std::cout << "Failed to add dataset." << std::endl;
    }
    else 
    {
        std::cout << "Dataset added at index: " << index << std::endl;
    }

    std::cout << "Counting datasets..." << std::endl;
    int count = 0;
    DICOMWLSPCntDataset(scp, &count);
    std::cout << "Total datasets: " << count << std::endl;

    std::cout << "Getting dataset by index..." << std::endl;
    LPVOID ds = DICOMWLSPGetDataset(scp, index);
    std::cout << "Dataset pointer: " << ds << std::endl;

    std::cout << "Starting SCP..." << std::endl;
    DICOMWLSPStart(scp);

    std::string statusText;
    std::cout << "Fetching status..." << std::endl;
    DICOMWLSPStatus(scp, &statusText);
    std::cout << "Status: " << statusText << std::endl;

    static std::string eingabe = "";
    std::thread([]()
        {
            std::cin >> eingabe;
        }).detach();
    while (eingabe == "")
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        DICOMWLSPStatus(scp, &statusText);
        std::cout << "\nStatus:\n" << statusText << std::endl;
    }

    std::cout << "Marking dataset as dirty..." << std::endl;
    DICOMWLSPMarkDirty(scp, index);

    std::cout << "Saving dirty dataset..." << std::endl;
    DICOMWLSPFlushDataset(scp, index);

    std::cout << "Saving all datasets..." << std::endl;
    DICOMWLSPFlushAll(scp);

    std::cout << "Saving only dirty datasets..." << std::endl;
    DICOMWLSPFlushDirty(scp);

    std::cout << "Deleting dataset..." << std::endl;
    DICOMWLSPDelDataset(scp, index);

    std::cout << "Stopping SCP..." << std::endl;
    DICOMWLSPStop(scp);

    std::cout << "Cleanup complete." << std::endl;
    return 0;
}

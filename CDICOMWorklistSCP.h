#ifndef CDICOMWorklistSCP_H
#define CDICOMWorklistSCP_H

#include <dcmtk/dcmnet/scp.h>
#include <unordered_map>
#include <set>
#include <mutex>

// Represents a DICOM Modality Worklist SCP server.
// Provides dataset management, status tracking, and file persistence.
class DICOMWorklistSCP : public DcmSCP 
{
public:
    DICOMWorklistSCP();
    ~DICOMWorklistSCP();

    // Configuration
    bool setTemplateFile(const std::string& filename);  

    // Dataset management
    bool addDataset(int* index);                                  
    bool deleteDataset(int index);                            
    bool getDatasetCount(int* count) const;                       
    std::shared_ptr<DcmDataset> getDataset(int index) const;                        
    bool clearAllDatasets();                                                 

    // Lifecycle control
    bool start();                                              
    bool stop();                                                 
    bool getStatus(std::string& status);                          

    // Saving logic
    bool markDatasetDirty(int index);
    bool saveDataset(int index);
    bool saveDirtyDatasets();
    bool saveAllDatasets();

protected:
    OFCondition handleIncomingCommand(
        T_DIMSE_Message* msg,
        const DcmPresentationContextInfo& presInfo);

private:
    bool loadAllDatasets();

    // Maintains current server status and request metrics.
    struct SCPStatus
    {
        // Indicates whether the SCP server is currently running and accepting associations
        bool isRunning_;

        // Tracks the total number of received DIMSE commands (e.g., C-FIND)
        int requestCount_;

        // Human-readable description of the current server state (e.g., "Idle", "Listening")
        std::string statusText_;

        // Aggregated error log with timestamps, reset after each ToString() call
        std::string lastErrors_;


        SCPStatus(bool isRunning = false, int requestCount = 0, std::string statusText = "Idle", std::string lastErrors = "");
        std::string ToString();
        void error(const std::string& message);
    };

    // Temporarily sets server status during processing.
    // Automatically restores previous status on destruction.
    struct ScopedStatus
    {
        DICOMWorklistSCP::SCPStatus& status_;
        std::string previousStatusText_;
        std::string finalStatusText_;
        ScopedStatus(DICOMWorklistSCP::SCPStatus& status, const std::string& actionName = "", const std::string& finalStatusText = "None");
        void changeStatus(const std::string& finalStatusText = "None");
        ~ScopedStatus();
    };

    // Container for DICOM datasets.
    // Handles indexing, dirty tracking, and saving to files.
    struct Worklist
    {
        struct Item
        {
            // Pointer to the actual DICOM dataset associated with this worklist item
            std::shared_ptr<DcmDataset> dataset_;

            // Filename used to persist this dataset on disk
            std::string fileName_;

            // Flag indicating whether this dataset has been modified and requires saving
            bool dirty_;

            Item(std::shared_ptr<DcmDataset> dataset, std::string fileName, bool dirty);
        };

        std::string dataFolder_ = "./worklist/";
        std::unordered_map<int, Item*> indexMap_;
        std::set<int> freeIndexes_;


        Item* operator[](int index) const;
        bool loadAllDatasets(SCPStatus& serverStatus);
        int add(std::shared_ptr<DcmDataset> dataset);
        bool markDatasetDirty(int index);
        bool remove(int id);
        void clear(SCPStatus& serverStatus);
        bool saveDatasetInFile(int index, SCPStatus& serverStatus);
        bool saveAllDatasetsInFile(SCPStatus& serverStatus);
        bool saveDirtyDatasetsInFile(SCPStatus& serverStatus);
        int count() const;

    private:
        std::string newFileName(const std::string& prefix = "dataset");
        int getFreeIndex();
    };

    // Synchronization primitive to ensure thread-safe access to shared state
    mutable std::mutex mutex_;

    // Path to the template DICOM file used when creating new worklist entries
    std::string templateFile_;

    // Server status tracker that logs state, number of processed requests, and error messages
    mutable SCPStatus serverStatus_;

    // Internal container for managing all loaded and active worklist datasets
    Worklist datasets_;

};


#endif // CDICOMWorklistSCP_H

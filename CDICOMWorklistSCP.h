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
        bool isRunning_;
        int requestCount_;
        std::string statusText_;
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
            std::shared_ptr<DcmDataset> dataset_;
            std::string fileName_;
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

    mutable std::mutex mutex_;
    std::string templateFile_;
    mutable SCPStatus serverStatus_;
    Worklist datasets_;
};


#endif // CDICOMWorklistSCP_H

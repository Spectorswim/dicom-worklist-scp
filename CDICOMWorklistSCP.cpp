// ===============================================================================================================
// ========================================== File CDICOMWorklistSCP.cpp =========================================
// ===============================================================================================================


#include "CDICOMWorklistSCP.h"
#include <filesystem>
#include <chrono>
#include <sstream>


// ===============================================================================================================
// ======================================== DICOMWorklistSCP Implementation ======================================
// ===============================================================================================================


// Constructor for the DICOM Worklist SCP server.
// Initializes server status and ensures the worklist folder exists.
// Also triggers dataset loading from disk to prepare in-memory cache.
DICOMWorklistSCP::DICOMWorklistSCP()
    : serverStatus_{}
{
    if (!std::filesystem::exists(datasets_.dataFolder_))
    {
        std::filesystem::create_directories(datasets_.dataFolder_);
    }

    loadAllDatasets();
}

// Destructor for the SCP server.
// Automatically stops the server if still running,
// ensuring graceful shutdown and release of network resources.
DICOMWorklistSCP::~DICOMWorklistSCP() 
{
    if (serverStatus_.isRunning_)
    {
        stop();
    }
}

// ------------------------------------------------ Configuration ------------------------------------------------

// Sets the path to a DICOM dataset template file.
// This template will be cloned into new datasets when calling addDataset().
// Returns true if the specified file exists; false otherwise.
// Thread-safe and updates server status.
bool DICOMWorklistSCP::setTemplateFile(const std::string& fileName) 
{
    std::lock_guard<std::mutex> lock(mutex_);
    ScopedStatus scoped(serverStatus_, "Template file setting");
    templateFile_ = fileName;
    return std::filesystem::exists(templateFile_);
}

// ---------------------------------------------- Dataset management ---------------------------------------------

// Adds a new dataset to the internal worklist.
// If a template file is set via setTemplateFile(), its contents will be cloned into the new dataset.
// The newly added dataset is marked as dirty and assigned a unique index, returned via the output parameter.
// Thread-safe and updates SCP status for processing.
bool DICOMWorklistSCP::addDataset(int* index)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ScopedStatus scoped(serverStatus_, "Adding a dataset");

    auto newDataset = std::make_shared<DcmDataset>();
    if (!templateFile_.empty())
    {
        DcmFileFormat fileformat;
        if (fileformat.loadFile(templateFile_.c_str()).good())
        {
            *newDataset = *fileformat.getDataset();
        }
    }

    *index = datasets_.add(newDataset);
    return true;
}

// Deletes a dataset from the internal worklist by index.
// Also removes the associated DICOM file from disk and frees the index for reuse.
// Returns true if deletion was successful.
// Thread-safe and updates SCP status
bool DICOMWorklistSCP::deleteDataset(int index)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ScopedStatus scoped(serverStatus_, "Deleting a dataset");

    return datasets_.remove(index);
}

// Retrieves the total number of datasets currently stored in the worklist.
// The count is returned via the output parameter 'count'.
// Returns true if the parameter is valid and retrieval was successful; false otherwise.
// Thread-safe and updates SCP processing status.
bool DICOMWorklistSCP::getDatasetCount(int* count) const
{
    // Ensure the output parameter is valid
    if (!count) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    ScopedStatus scoped(serverStatus_, "Getting dataset count");
    *count = datasets_.count();
    return true;
}

// Retrieves the dataset stored under the specified index from the internal worklist.
// Returns a shared pointer to the DcmDataset if the index exists; otherwise, returns nullptr.
// The caller must check the returned pointer before usage.
// Thread-safe and updates SCP status for tracking.
std::shared_ptr<DcmDataset> DICOMWorklistSCP::getDataset(int index) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    ScopedStatus scoped(serverStatus_, "Getting dataset");
    auto item = datasets_[index];
    return item ? item->dataset_ : nullptr;
}

// Clears the entire dataset worklist, removing all loaded datasets from memory and deleting their associated DICOM files from disk.
// Frees all indexes and resets the internal state.
// Returns true on successful completion.
// Thread-safe and updates SCP status.
// !! This operation is destructive and cannot be reversed.
bool DICOMWorklistSCP::clearAllDatasets() 
{
    std::lock_guard<std::mutex> lock(mutex_);
    ScopedStatus scoped(serverStatus_, "Clearing the list");
    datasets_.clear(serverStatus_);
    return true;
}

// ---------------------------------------------- Lifecycle control ----------------------------------------------

// Starts the DICOM Worklist SCP server instance.
// Configures network parameters, transfer syntax, and presentation contexts needed for DICOM association negotiation.
// Opens the listening port and spawns a background thread to accept incoming DICOM associations.
// If the server is already running, returns true immediately.
// Updates server status to reflect activity (via ScopedStatus) and sets running flag.
// Returns true if listening started successfully; false otherwise.
// Thread-safe thanks to locking via std::mutex.
bool DICOMWorklistSCP::start()
{
    std::lock_guard<std::mutex> lock(mutex_);
    ScopedStatus scoped(serverStatus_, "Starting");

    if (serverStatus_.isRunning_)
    {
        return true;
    }

    setPort(104);
    setAETitle("WORKLIST_SCP");
    setMaxReceivePDULength(16384);
    setConnectionTimeout(30);
    setDIMSETimeout(30);
    setACSETimeout(30);

    OFList<OFString> syntaxes;
    syntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
    addPresentationContext(UID_FINDModalityWorklistInformationModel, syntaxes);


    // Begin listening for incoming DICOM associations using configured parameters
    OFCondition status = openListenPort();
    if (status.good())
    {
        std::thread([this]() 
            {
                acceptAssociations();
            }).detach();
        serverStatus_.isRunning_ = true;
        scoped.changeStatus("Listening");
        return true;
    }
    else
    {
        return false;
    }
}

// Stops the DICOM Worklist SCP server gracefully.
// If the server is not running, returns true immediately.
// Otherwise, terminates listening by completing the current association.
// Sets server status to "Idle" and marks the instance as inactive.
// Thread-safe and designed to be safely called multiple times.
bool DICOMWorklistSCP::stop()
{
    std::lock_guard<std::mutex> lock(mutex_);
    ScopedStatus scoped(serverStatus_, "Stopping", "Idle");

    if (!serverStatus_.isRunning_)
        return true;

    stopAfterCurrentAssociation();

    serverStatus_.isRunning_ = false;
    return true;
}

// Retrieves the current status of the SCP server as a human-readable string.
// The result includes running state, request count, and descriptive status.
// The status string is written into the output parameter.
// Thread-safe and suitable for external logging or monitoring tools.
bool DICOMWorklistSCP::getStatus(std::string& status)
{
    std::lock_guard<std::mutex> lock(mutex_);
    status = serverStatus_.ToString();
    return true;
}

// ------------------------------------------------ Saving logic -------------------------------------------------

// Marks the dataset associated with the given index as "dirty",
// indicating that it has been modified and should be saved to disk.
// Integrates with saveDirtyDatasets() to optimize file I/O.
// Thread-safe and updates SCP processing status.
bool DICOMWorklistSCP::markDatasetDirty(int index)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ScopedStatus scoped(serverStatus_, "Marking dataset as dirty");
    return datasets_.markDatasetDirty(index);
}

// Saves all datasets in the worklist that are marked as "dirty" (i.e., modified but not yet saved).
// This function avoids rewriting unchanged files and improves storage efficiency.
// Internally calls Worklist::saveDirtyDatasetsInFile().
// Thread-safe and updates server processing status.
bool DICOMWorklistSCP::saveDirtyDatasets()
{
    std::lock_guard<std::mutex> lock(mutex_);
    ScopedStatus scoped(serverStatus_, "Saving dirty datasets");
    return datasets_.saveDirtyDatasetsInFile(serverStatus_);
}

// Saves the dataset at the specified index to disk.
// Intended for precise control over individual dataset updates, avoiding bulk saves.
// Internally delegates to Worklist::saveDatasetInFile().
// Thread-safe and updates SCP processing status.
bool DICOMWorklistSCP::saveDataset(int index)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ScopedStatus scoped(serverStatus_, "Saving a dataset by index");
    return datasets_.saveDatasetInFile(index, serverStatus_);
}

// Saves all datasets currently stored in the worklist to disk, regardless of their modification state.
// This method ensures complete synchronization between memory and persistent storage.
// All datasets, including unmodified ones, are overwritten, and their dirty flags reset.
// Thread-safe and updates SCP processing status.
bool DICOMWorklistSCP::saveAllDatasets()
{
    std::lock_guard<std::mutex> lock(mutex_);
    ScopedStatus scoped(serverStatus_, "Saving all datasets");
    return datasets_.saveAllDatasetsInFile(serverStatus_);
}

// Loads all datasets from the data folder into memory.
// Each valid DICOM file is parsed, wrapped as an internal Item, and indexed in the worklist.
// If any files fail to load, they are skipped and a warning is logged.
// Thread-safe and updates server status during processing.
bool DICOMWorklistSCP::loadAllDatasets()
{
    std::lock_guard<std::mutex> lock(mutex_);
    ScopedStatus scoped(serverStatus_, "Loading all datasets from file");

    return datasets_.loadAllDatasets(serverStatus_);
}

    
// ===============================================================================================================
// ========================================== DICOMWorklistSCP::SCPStatus ========================================
// ===============================================================================================================


// Constructs a new SCPStatus object to represent the server’s runtime state.
// Tracks whether the server is running, how many DIMSE requests were processed,
// a human-readable status message, and a cumulative error log.
// Used for diagnostics, logging, and external status querying.
DICOMWorklistSCP::SCPStatus::SCPStatus(bool isRunning, int requestCount, std::string statusText, std::string lastErrors)
{
    isRunning_ = isRunning;
    requestCount_ = requestCount;
    statusText_ = statusText;
    lastErrors_ = lastErrors;
}

// Returns a formatted string summarizing the server status.
// Includes whether the server is running, the total number of DIMSE requests,
// current status text, and accumulated error messages.
// Clears the error log after reporting, ensuring fresh status output on next call.
// Useful for external monitoring tools, GUI status panels, or logging.
std::string DICOMWorklistSCP::SCPStatus::ToString()
{
    std::ostringstream ss;
    ss 
        << "Running: " << (isRunning_ ? "true" : "false")
        << "\n Requests: " << requestCount_
        << "\n State: " << statusText_
        << "\n Last Errors: " << (lastErrors_.empty() ? "None" : lastErrors_);
    lastErrors_ = "";
    return ss.str();
}

// Adds a new error message to the SCP’s cumulative error log.
// Each message is prefixed with a timestamp in local time (HH:MM:SS format).
// Messages are stored and included in the next call to ToString(), then cleared.
// Allows chronological tracking of multiple errors during runtime.
void DICOMWorklistSCP::SCPStatus::error(const std::string& message)
{
    auto now = std::chrono::system_clock::now();
    auto nowTimeT = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&nowTimeT), "%H:%M:%S");
    lastErrors_ += "\n\t" + ss.str() + " Error: " + message;
}


// ===============================================================================================================
// ========================================= DICOMWorklistSCP::ScopedStatus ======================================
// ===============================================================================================================


// Constructs a scoped status modifier for the SCP server.
// Sets the status text to indicate ongoing processing (with optional action name),
// and increments the request counter.
// Stores the previous status text for automatic restoration on scope exit.
// Designed for RAII-based status tracking during operations.
DICOMWorklistSCP::ScopedStatus::ScopedStatus(
    DICOMWorklistSCP::SCPStatus& status, 
    const std::string& actionName, 
    const std::string& finalStatusText)
    : status_(status), previousStatusText_(status.statusText_), finalStatusText_(finalStatusText)
{
    status_.statusText_ = actionName == "" ? "Processing" : "Processing: " + actionName;
}

// Updates the final status text that will be applied upon scope exit.
// Useful for reflecting operation results or error conditions in the SCP status log.
void DICOMWorklistSCP::ScopedStatus::changeStatus(const std::string& finalStatusText)
{
    finalStatusText_ = finalStatusText;
}

// Destructor that restores the SCP status text upon scope exit.
// If a custom final status was provided via changeStatus(), it is applied.
// Otherwise, the previous status text is restored.
// Enables automatic and consistent status tracking through RAII.
DICOMWorklistSCP::ScopedStatus::~ScopedStatus()
{
    status_.statusText_ = finalStatusText_ == "None" ? previousStatusText_ : finalStatusText_;
}


// ===============================================================================================================
// =========================================== DICOMWorklistSCP::Worklist ========================================
// ===============================================================================================================


// ---------------------------------------------- Dataset management ---------------------------------------------

// Loads all DICOM dataset files from the configured data folder into memory.
// Each valid file is parsed into a DcmDataset, wrapped into an Item object,
// assigned a unique index, and inserted into the internal worklist map.
// If loading fails for a file, an error message is reported via SCPStatus,
// including the file name and timestamp.  
// Returns true if at least one dataset was successfully loaded; false otherwise.
bool DICOMWorklistSCP::Worklist::loadAllDatasets(SCPStatus& serverStatus)
{
    using namespace std::filesystem;
    int loadedCount = 0;

    for (const auto& entry : directory_iterator(dataFolder_))
    {
        if (!entry.is_regular_file()) continue;

        std::string path = entry.path().string();
        std::string fileName = entry.path().filename().string();

        std::shared_ptr<DcmDataset> dataset = std::make_shared<DcmDataset>();
        OFCondition status = dataset->loadFile(path.c_str());

        if (status.good())
        {
            Item* item = new Item(dataset, fileName, false);
            int id = getFreeIndex();
            indexMap_[id] = item;
            loadedCount++;
        }
        else
        {
            serverStatus.error("[Worklist] Failed to load: " + fileName);
        }
    }

    return loadedCount > 0;
}

// Adds a new DICOM dataset to the worklist.
// Wraps the dataset into an Item with a generated filename and marks it as dirty.
// Assigns a unique index and stores the Item in the internal map.
// Returns the assigned index of the newly added dataset.
int DICOMWorklistSCP::Worklist::add(std::shared_ptr<DcmDataset> dataset)
{
    Item* newItem = new Item(dataset, newFileName(), true);
    int index = getFreeIndex();
    indexMap_[index] = newItem;
    return index;
}

// Removes the dataset associated with the given index from the worklist.
// If the dataset file exists on disk, it is deleted.
// The Item object is deallocated, and its index is returned to the reusable pool.
// Returns true if the index existed and was successfully removed; false otherwise.
bool DICOMWorklistSCP::Worklist::remove(int index)
{
    auto it = indexMap_.find(index);
    if (it != indexMap_.end())
    {
        Item* item = it->second;
        if (item)
        {
            std::string path = dataFolder_ + item->fileName_;
            if (std::filesystem::exists(path))
            {
                std::filesystem::remove(path);
            }

            delete item;
        }

        indexMap_.erase(it);
        freeIndexes_.insert(index);

        return true;
    }
    return false;
}

// Returns the number of currently loaded datasets in the worklist.
// Represents the size of the internal index map.
// Useful for diagnostics, iteration, and external queries.
int DICOMWorklistSCP::Worklist::count() const
{
    return static_cast<int>(indexMap_.size());
}

// Provides direct access to a worklist Item by index.
// Returns a pointer to the Item if the index exists; nullptr otherwise.
// Enables array-style lookup for simplified code usage.
DICOMWorklistSCP::Worklist::Item* DICOMWorklistSCP::Worklist::operator[](int index) const
{
    auto it = indexMap_.find(index);
    return (it != indexMap_.end()) ? it->second : nullptr;
}

// Clears the entire worklist, removing all loaded datasets from memory and disk.
// For each Item, deletes the associated DICOM file if it exists. If deletion fails,
// the error is reported via the provided SCPStatus object.
// After cleanup, resets the internal index map and index reuse pool.
void DICOMWorklistSCP::Worklist::clear(SCPStatus& serverStatus)
{
    for (auto& [id, item] : indexMap_)
    {
        if (item)
        {
            std::string path = dataFolder_ + item->fileName_;
            if (std::filesystem::exists(path))
            {
                if (!std::filesystem::remove(path))
                {
                    serverStatus.error("Failed to remove file: " + item->fileName_);
                }
            }

            delete item;
        }
    }

    indexMap_.clear();
    freeIndexes_.clear();
}

// ------------------------------------------------ Saving logic -------------------------------------------------

// Marks the dataset at the given index as dirty, indicating it has been modified
// and needs to be saved to disk.
// Returns true if the index exists and the flag was successfully set; false otherwise.
bool DICOMWorklistSCP::Worklist::markDatasetDirty(int index)
{
    Item* item = (*this)[index];
    if (!item) return false;

    item->dirty_ = true;
    return true;
}

// Saves the dataset associated with the given index to disk in explicit little-endian format.
// If saving fails, an error message is reported via the provided SCPStatus object.
// On success, the dataset is marked as not dirty.
// Returns true if the save operation succeeded; false otherwise.
bool DICOMWorklistSCP::Worklist::saveDatasetInFile(int index, SCPStatus& serverStatus)
{
    Item* item = (*this)[index];
    if (!item || !item->dataset_) return false;

    std::string path = dataFolder_ + item->fileName_;
    OFCondition status = item->dataset_->saveFile(path.c_str(), EXS_LittleEndianExplicit);

    if (status.good())
    {
        item->dirty_ = false;
        return true;
    }
    else
    {
        serverStatus.error("Failed to save: " + item->fileName_);
    }
    return false;
}

// Saves all datasets currently loaded in the worklist to disk using explicit little-endian encoding.
// Each dataset is written to its assigned filename under the configured data folder.
// If any save operation fails, an error message is reported via the provided SCPStatus object,
// and the method returns false. Otherwise, returns true on complete success.
bool DICOMWorklistSCP::Worklist::saveAllDatasetsInFile(SCPStatus& serverStatus)
{
    bool success = true;

    for (auto& [id, item] : indexMap_)
    {
        if (!item || !item->dataset_) continue;

        std::string path = dataFolder_ + item->fileName_;
        OFCondition status = item->dataset_->saveFile(path.c_str(), EXS_LittleEndianExplicit);

        if (status.good())
        {
            item->dirty_ = false;
        }
        else
        {
            serverStatus.error("Failed to save: " + item->fileName_);
            success = false;
        }
    }

    return success;
}

// Saves all dirty datasets (marked as modified) in the worklist to disk using explicit little-endian format.
// Only Items with dirty_ == true are saved. If any save operation fails,
// an error message is reported via the provided SCPStatus object,
// and the method returns false. Otherwise, returns true on complete success.
bool DICOMWorklistSCP::Worklist::saveDirtyDatasetsInFile(SCPStatus& serverStatus)
{
    bool success = true;

    for (auto& [id, item] : indexMap_)
    {
        if (!item || !item->dataset_ || !item->dirty_) continue;

        std::string path = dataFolder_ + item->fileName_;
        OFCondition status = item->dataset_->saveFile(path.c_str(), EXS_LittleEndianExplicit);

        if (status.good())
        {
            item->dirty_ = false;
        }
        else
        {
            serverStatus.error("Failed to save: " + item->fileName_);
            success = false;
        }
    }

    return success;
}

// ----------------------------------------------- DIMSE Handling ------------------------------------------------

// Handles incoming DIMSE commands from the DICOM network association.
// This method is invoked internally by the DcmSCP framework whenever a request is received.
// Specifically detects C-FIND requests and sends a generic success response.
// Serves as the primary entry point for DIMSE command processing (e.g., C-FIND Worklist queries).
// To extend functionality, implement dataset matching, filtering, and dynamic responses here.
// Returns OFCondition::good() if a valid response is sent; otherwise returns an error code.
OFCondition DICOMWorklistSCP::handleIncomingCommand(
    T_DIMSE_Message* incomingMsg,
    const DcmPresentationContextInfo& presInfo)
{
    serverStatus_.requestCount_++;
    if (!incomingMsg)
    {
        return EC_IllegalCall;
    }

    if (incomingMsg->CommandField == DIMSE_C_FIND_RQ)
    {
        T_DIMSE_C_FindRQ& request = incomingMsg->msg.CFindRQ;

        T_DIMSE_Message response;
        response.CommandField = DIMSE_C_FIND_RSP;
        response.msg.CFindRSP.MessageIDBeingRespondedTo = request.MessageID;
        response.msg.CFindRSP.DimseStatus = STATUS_Success;
        response.msg.CFindRSP.DataSetType = DIMSE_DATASET_NULL;
        strcpy(response.msg.CFindRSP.AffectedSOPClassUID, request.AffectedSOPClassUID);

        return sendDIMSEMessage(
            presInfo.presentationContextID,
            &response,
            nullptr,
            nullptr,
            nullptr);
    }

    return DcmSCP::handleIncomingCommand(incomingMsg, presInfo);
}

// ------------------------------------------- Index & Naming Helpers --------------------------------------------

// Generates a new unique filename for a DICOM dataset using the current local time.
// The filename follows the format: <prefix>_YYYYMMDD_HHMMSS_<ms>.dcm
// This ensures chronological ordering and prevents collisions.
// Useful for saving new datasets without overwriting existing files.
std::string DICOMWorklistSCP::Worklist::newFileName(const std::string& prefix)
{
    auto now = std::chrono::system_clock::now();
    auto nowTimeT = std::chrono::system_clock::to_time_t(now);
    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;

    std::stringstream ss;
    ss << prefix << "_";
    ss << std::put_time(std::localtime(&nowTimeT), "%Y%m%d_%H%M%S");
    ss << "_" << nowMs;
    ss << ".dcm";

    return ss.str();
}

// Returns an available index for a new worklist Item.
// If there are recycled indices from previously removed Items, reuses the smallest one.
// Otherwise, assigns the next index based on the current map size.
// Ensures consistent and collision-free indexing within the worklist.
int DICOMWorklistSCP::Worklist::getFreeIndex()
{
    if (freeIndexes_.empty())
    {
        return static_cast<int>(indexMap_.size());
    }
    else
    {
        int index = *freeIndexes_.begin();
        freeIndexes_.erase(freeIndexes_.begin());
        return index;
    }
}


// ===============================================================================================================
// ======================================== DICOMWorklistSCP::Worklist::Item =====================================
// ===============================================================================================================


// Constructs a new Item object to represent a DICOM dataset within the worklist.
// Stores the dataset pointer, its filename, and the dirty flag indicating unsaved modifications.
DICOMWorklistSCP::Worklist::Item::Item(std::shared_ptr<DcmDataset> dataset, std::string fileName, bool dirty)
{
    dataset_ = dataset;
    fileName_ = fileName;
    dirty_ = dirty;
}


// ===============================================================================================================
// ================================================== End of file ================================================
// ===============================================================================================================
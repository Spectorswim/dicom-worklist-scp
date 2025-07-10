#include <windows.h>
#include "stdio.h"
#define _EXPORTS_DICOM_
#include "DICOMC.h"
#include "CDICOMWorklistSCP.h"


// 
// DICOMWLSPCreate
// 
LPVOID _DICOMC_API_ DICOMWLSPCreate()
{
    return new DICOMWorklistSCP();
}

// 
// DICOMWLSPSetTemplateFile
// 
BOOL _DICOMC_API_ DICOMWLSPSetTemplateFile(LPVOID a_Obj, LPCSTR a_FileName)
{
    auto obj = static_cast<DICOMWorklistSCP*>(a_Obj);
    return obj->setTemplateFile(a_FileName);
}

// 
// DICOMWLSPClear
// 
BOOL _DICOMC_API_ DICOMWLSPClear(PVOID a_Obj)
{
    auto obj = static_cast<DICOMWorklistSCP*>(a_Obj);
    return obj->clearAllDatasets();
}

// 
// DICOMWLSPAddDataset
// 
BOOL _DICOMC_API_ DICOMWLSPAddDataset(PVOID a_Obj, PINT a_INDEX)
{
    auto obj = static_cast<DICOMWorklistSCP*>(a_Obj);
    return obj->addDataset(a_INDEX);
}

// 
// DICOMWLSPDelDataset
// 
BOOL _DICOMC_API_ DICOMWLSPDelDataset(PVOID a_Obj, INT a_INDEX)
{
    auto obj = static_cast<DICOMWorklistSCP*>(a_Obj);
    return obj->deleteDataset(a_INDEX);
}

// 
// DICOMWLSPCntDataset
// 
BOOL _DICOMC_API_ DICOMWLSPCntDataset(PVOID a_Obj, PINT a_Count)
{
    auto obj = static_cast<const DICOMWorklistSCP*>(a_Obj);
    return obj->getDatasetCount(a_Count);
}

// 
// DICOMWLSPGetDataset
// 
LPVOID _DICOMC_API_ DICOMWLSPGetDataset(LPVOID a_Obj, INT a_Index)
{
    auto obj = static_cast<DICOMWorklistSCP*>(a_Obj);
    auto dataset = obj->getDataset(a_Index);
    return dataset.get();
}

// 
// DICOMWLSPStart
// 
BOOL _DICOMC_API_ DICOMWLSPStart(PVOID a_Obj)
{
    auto obj = static_cast<DICOMWorklistSCP*>(a_Obj);
    return obj->start();
}

// 
// DICOMWLSPStop
// 
BOOL _DICOMC_API_ DICOMWLSPStop(PVOID a_Obj)
{
    auto obj = static_cast<DICOMWorklistSCP*>(a_Obj);
    return obj->stop();
}

// 
// DICOMWLSPStatus
// 
BOOL _DICOMC_API_ DICOMWLSPStatus(PVOID a_Obj, LPVOID a_Status)
{
    auto obj = static_cast<DICOMWorklistSCP*>(a_Obj);
    auto statusStr = static_cast<std::string*>(a_Status);
    return obj->getStatus(*statusStr);
}

// 
// DICOMWLSPMarkDirty
// 
BOOL _DICOMC_API_ DICOMWLSPMarkDirty(PVOID a_Obj, INT a_INDEX)
{
    auto obj = static_cast<DICOMWorklistSCP*>(a_Obj);
    return obj->markDatasetDirty(a_INDEX);
}

// 
// DICOMWLSPFlushDataset
// 
BOOL _DICOMC_API_ DICOMWLSPFlushDataset(PVOID a_Obj, INT a_INDEX)
{
    auto obj = static_cast<DICOMWorklistSCP*>(a_Obj);
    return obj->saveDataset(a_INDEX);
}

// 
// DICOMWLSPFlushAll
// 
BOOL _DICOMC_API_ DICOMWLSPFlushAll(PVOID a_Obj)
{
    auto obj = static_cast<DICOMWorklistSCP*>(a_Obj);
    return obj->saveAllDatasets();
}

// 
// DICOMWLSPFlushDirty
// 
BOOL _DICOMC_API_ DICOMWLSPFlushDirty(PVOID a_Obj)
{
    auto obj = static_cast<DICOMWorklistSCP*>(a_Obj);
    return obj->saveDirtyDatasets();
}
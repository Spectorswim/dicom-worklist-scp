#ifndef _DICOMC_
#define _DICOMC_

#ifdef _EXPORTS_DICOM_
#define _DICOMC_API_ __declspec(dllexport)
#else
#define _DICOMC_API_ __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

	
	LPVOID _DICOMC_API_ DICOMWLSPCreate();
	BOOL _DICOMC_API_ DICOMWLSPSetTemplateFile(LPVOID a_Obj, LPCSTR a_FileName);	// Load template file to initialize new elements

	BOOL _DICOMC_API_ DICOMWLSPClear(PVOID a_Obj);									// Clear list
	BOOL _DICOMC_API_ DICOMWLSPAddDataset(PVOID a_Obj, PINT a_INDEX);				// add new item to list
	BOOL _DICOMC_API_ DICOMWLSPDelDataset(PVOID a_Obj, INT a_INDEX);                // remove item from list
	BOOL _DICOMC_API_ DICOMWLSPCntDataset(PVOID a_Obj, PINT a_Count);                 
	LPVOID _DICOMC_API_ DICOMWLSPGetDataset(LPVOID a_Obj, INT a_Index);				// a_Index = element in listm, returns dataset instance

	BOOL _DICOMC_API_ DICOMWLSPStart(PVOID a_Obj);
	BOOL _DICOMC_API_ DICOMWLSPStop(PVOID a_Obj);
	BOOL _DICOMC_API_ DICOMWLSPStatus(PVOID a_Obj, LPVOID a_Status);				// Providing status information about WL SP - structure need to be defined

	BOOL _DICOMC_API_ DICOMWLSPMarkDirty(PVOID a_Obj, INT a_INDEX);                   // Mark dataset by index as dirty
	BOOL _DICOMC_API_ DICOMWLSPFlushDataset(PVOID a_Obj, INT a_INDEX);                // Save dataset by index
	BOOL _DICOMC_API_ DICOMWLSPFlushAll(PVOID a_Obj);                                 // Save all datasets
	BOOL _DICOMC_API_ DICOMWLSPFlushDirty(PVOID a_Obj);                               // Save only dirty datasets



#ifdef __cplusplus
}
#endif

#endif
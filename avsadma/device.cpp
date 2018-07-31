/**************************************************************************

    AVStream Simulated Hardware Sample

    Copyright (c) 2001, Microsoft Corporation.

    File:

        device.cpp

    Abstract:

        This file contains the device level implementation of the AVStream
        hardware sample.  Note that this is not the "fake" hardware.  The
        "fake" hardware is in hwsim.cpp.

    History:

        created 3/9/2001

**************************************************************************/

#include "avshws.h"

#include "trace.h"
#ifdef DBG
#include "device.tmh"
#endif

PVOID operator new
(
    size_t          iSize,
    _When_((poolType & NonPagedPoolMustSucceed) != 0,
       __drv_reportError("Must succeed pool allocations are forbidden. "
             "Allocation failures cause a system crash"))
    POOL_TYPE       poolType
)
{
    PVOID result = ExAllocatePoolWithTag(poolType,iSize,'wNCK');

    if (result) {
        RtlZeroMemory(result,iSize);
    }

    return result;
}

PVOID operator new
(
    size_t          iSize,
    _When_((poolType & NonPagedPoolMustSucceed) != 0,
       __drv_reportError("Must succeed pool allocations are forbidden. "
             "Allocation failures cause a system crash"))
    POOL_TYPE       poolType,
    ULONG           tag
)
{
    PVOID result = ExAllocatePoolWithTag(poolType,iSize,tag);

    if (result) {
        RtlZeroMemory(result,iSize);
    }

    return result;
}

PVOID 
operator new[](
    size_t          iSize,
    _When_((poolType & NonPagedPoolMustSucceed) != 0,
        __drv_reportError("Must succeed pool allocations are forbidden. "
            "Allocation failures cause a system crash"))
    POOL_TYPE       poolType,
    ULONG           tag
)
{
    PVOID result = ExAllocatePoolWithTag(poolType, iSize, tag);

    if (result)
    {
        RtlZeroMemory(result, iSize);
    }

    return result;
}

/*++

Routine Description:

    Array delete() operator.

Arguments:

    pVoid -
        The memory to free.

Return Value:

    None

--*/
void 
__cdecl 
operator delete[](
    PVOID pVoid
)
{
    if (pVoid)
    {
        ExFreePool(pVoid);
    }
}

/*++

Routine Description:

    Sized delete() operator.

Arguments:

    pVoid -
        The memory to free.

    size -
        The size of the memory to free.

Return Value:

    None

--*/
void __cdecl operator delete
(
    void *pVoid,
    size_t /*size*/
)
{
    if (pVoid)
    {
        ExFreePool(pVoid);
    }
}

/*++

Routine Description:

    Sized delete[]() operator.

Arguments:

    pVoid -
        The memory to free.

    size -
        The size of the memory to free.

Return Value:

    None

--*/
void __cdecl operator delete[]
(
    void *pVoid,
    size_t /*size*/
)
{
    if (pVoid)
    {
        ExFreePool(pVoid);
    }
}

void __cdecl operator delete
(
    PVOID pVoid
    )
{
    if (pVoid) {
        ExFreePool(pVoid);
    }
}

/**************************************************************************

    PAGEABLE CODE

**************************************************************************/

#ifdef ALLOC_PRAGMA
#pragma code_seg("PAGE")
#endif // ALLOC_PRAGMA

NTSTATUS
CCaptureDevice::
DispatchCreate (
    IN PKSDEVICE Device
    )

/*++

Routine Description:

    Create the capture device.  This is the creation dispatch for the
    capture device.

Arguments:

    Device -
        The AVStream device being created.

Return Value:

    Success / Failure

--*/

{

    PAGED_CODE();

    NTSTATUS Status;

    CCaptureDevice *CapDevice = new (NonPagedPoolNx, 'veDC') CCaptureDevice (Device);

    if (!CapDevice) {
        //
        // Return failure if we couldn't create the pin.
        //
        Status = STATUS_INSUFFICIENT_RESOURCES;

    } else {

        //
        // Add the item to the object bag if we were successful.
        // Whenever the device goes away, the bag is cleaned up and
        // we will be freed.
        //
        // For backwards compatibility with DirectX 8.0, we must grab
        // the device mutex before doing this.  For Windows XP, this is
        // not required, but it is still safe.
        //
        KsAcquireDevice (Device);
        Status = KsAddItemToObjectBag (
            Device -> Bag,
            reinterpret_cast <PVOID> (CapDevice),
            reinterpret_cast <PFNKSFREE> (CCaptureDevice::Cleanup)
            );
        KsReleaseDevice (Device);

        if (!NT_SUCCESS (Status)) {
            delete CapDevice;
        } else {
            Device -> Context = reinterpret_cast <PVOID> (CapDevice);
        }

    }

    return Status;

}

/*************************************************/

NTSTATUS
CCaptureDevice::
MapResources(
	IN PCM_RESOURCE_LIST TranslatedResourceList,
	IN PCM_RESOURCE_LIST UntranslatedResourceList
)
{
	PAGED_CODE();

	NTSTATUS status = STATUS_SUCCESS;
	PCM_PARTIAL_RESOURCE_DESCRIPTOR resource;
	PCM_PARTIAL_RESOURCE_LIST       partialResourceListTranslated;
	ULONG                           i;

	//
	// Parameters.StartDevice.AllocatedResourcesTranslated points
	// to a CM_RESOURCE_LIST describing the hardware resources that
	// the PnP Manager assigned to the device. This list contains
	// the resources in translated form. Use the translated resources
	// to connect the interrupt vector, map I/O space, and map memory.
	//

	partialResourceListTranslated = &TranslatedResourceList->List[0].PartialResourceList;
	resource = &partialResourceListTranslated->PartialDescriptors[0];

	for (i = 0; i < partialResourceListTranslated->Count; i++, resource++) {

		switch (resource->Type) {

		case CmResourceTypePort:

			break;

		case CmResourceTypeMemory:

			m_NumberOfBARs++;

			m_DmaBarLen = resource->u.Memory.Length;
			m_DmaBar = MmMapIoSpace(resource->u.Memory.Start, m_DmaBarLen, MmNonCached);
			if (m_DmaBar == NULL) {
				TraceError(DBG_INIT, "MmMapIoSpace returned NULL! for BAR%u", i);
				return STATUS_DEVICE_CONFIGURATION_ERROR;
			}
			TraceInfo(DBG_INIT, "MM BAR %d (addr:0x%llx, length:%u) mapped at 0x%08p",
				i, resource->u.Memory.Start.QuadPart, m_DmaBarLen, m_DmaBar);

			break;

		case CmResourceTypeMemoryLarge:

			m_NumberOfBARs++;
			if (resource->Flags & CM_RESOURCE_MEMORY_LARGE_40) {
				PHYSICAL_ADDRESS start;
				start.QuadPart = resource->u.Memory40.Start.QuadPart;
				m_VideoBarLen = resource->u.Memory40.Length40 << 8;
				m_VideoBarLen = 0x10000;//we only use part of it because the design is ugly
				//offset to 2GB, customer design is stupid, I'll use only 2GB~2GB+64KB(0x10000) of 4GB
				start.QuadPart += 0x80000000LL;
				m_VideoBar = MmMapIoSpace(start, m_VideoBarLen, MmNonCached);

				if (m_VideoBar == NULL) {
					TraceError(DBG_INIT, "MmMapIoSpace returned NULL! for BAR%u", i);
					return STATUS_DEVICE_CONFIGURATION_ERROR;
				}
				TraceInfo(DBG_INIT, "MM40 BAR %d (addr:0x%llx, length:%u) mapped at 0x%08p",
					i, resource->u.Memory40.Start.QuadPart, m_VideoBarLen, m_VideoBar);
			}

			break;

		case CmResourceTypeInterrupt:

			//
			// Save all the interrupt specific information in the device
			// extension because we will need it to disconnect and connect the
			// interrupt later on during power suspend and resume.
			//

			m_NumberOfIntrs++;
			TraceInfo(DBG_INIT, "Interrupt resource Cnt: %d\n", m_NumberOfIntrs);

			if (m_NumberOfIntrs > 1) {
				break;
			}

			if (m_InterruptFlags & CM_RESOURCE_INTERRUPT_MESSAGE) {
				// The resource is for a message-signaled interrupt. Use the u.MessageInterrupt.Translated member of IntResource.
				m_InterruptLevel = (UCHAR)resource->u.MessageInterrupt.Translated.Level;
				m_InterruptVector = resource->u.MessageInterrupt.Translated.Vector;
				m_InterruptAffinity = resource->u.MessageInterrupt.Translated.Affinity;
				m_MessageCount = resource->u.MessageInterrupt.Raw.MessageCount;
			}
			else {
				// The resource is for a line-based interrupt. Use the u.Interrupt member of IntResource.
				m_InterruptLevel = (UCHAR)resource->u.Interrupt.Level;
				m_InterruptVector = resource->u.Interrupt.Vector;
				m_InterruptAffinity = resource->u.Interrupt.Affinity;
			}
			m_InterruptFlags = resource->Flags;
			m_ShareDisposition = resource->ShareDisposition;

			//
			// Because this is a PCI device, we KNOW it must be
			// a LevelSensitive Interrupt.
			//


			TraceInfo(DBG_INIT,
				"Interrupt level: 0x%0x, Vector: 0x%0x, Affinity: 0x%x, Flags: 0x%x\n",
				m_InterruptLevel,
				m_InterruptVector,
				(UINT)m_InterruptAffinity,
				m_InterruptFlags); // casting is done to keep WPP happy
			break;

		default:
			//
			// This could be device-private type added by the PCI bus driver. We
			// shouldn't filter this or change the information contained in it.
			//
			TraceInfo(DBG_INIT, "Unhandled resource type (0x%x)\n",
				resource->Type);
			break;

		}
	}

	if (m_NumberOfBARs < 2 || m_NumberOfIntrs < 1) {
		return STATUS_DEVICE_CONFIGURATION_ERROR;
	}

	if (m_NumberOfIntrs > 1) {
		TraceInfo(DBG_INIT, "Unsupported Multi MSI/MSI-X resources\n");
		return STATUS_DEVICE_CONFIGURATION_ERROR;
	}

	return status;
}

/*************************************************/


NTSTATUS CCaptureDevice::UnmapResources()
{
	PAGED_CODE();

	NTSTATUS status = STATUS_SUCCESS;

	if (m_DmaBar != NULL) {
		TraceInfo(DBG_INIT, "Unmapping DMA BAR, VA:(%p) Length %ul",
			m_DmaBar, m_DmaBarLen);
		MmUnmapIoSpace(m_DmaBar, m_DmaBarLen);
		m_DmaBar = NULL;
	}

	if (m_VideoBar != NULL) {
		TraceInfo(DBG_INIT, "Unmapping DMA BAR, VA:(%p) Length %ul",
			m_VideoBar, m_VideoBarLen);
		MmUnmapIoSpace(m_VideoBar, m_VideoBarLen);
		m_VideoBar = NULL;
	}

	return status;
}

/*************************************************/


VOID
CCaptureDevice::
AdmaDpcForIsr(
	PKDPC            Dpc,
	PDEVICE_OBJECT   DeviceObject,
	PIRP             Irp, //Unused
	PVOID            Context
)

/*++

Routine Description:

DPC callback for ISR.

Arguments:

DeviceObject - Pointer to the device object.

Context - MiniportAdapterContext.

Irp - Unused.

Context - Pointer to FDO_DATA.

Return Value:

--*/
{
	CCaptureDevice *CapDevice = NULL;
	CapDevice = reinterpret_cast <CCaptureDevice *> (Context);

	TraceInfo(DBG_IRQ, "--> AdmaDpcForIsr\n");


	TraceInfo(DBG_IRQ, "<-- AdmaDpcForIsr\n");
}

/*************************************************/

BOOLEAN
CCaptureDevice::
AdmaInterruptHandler(
	__in PKINTERRUPT  Interupt,
	__in PVOID        ServiceContext
)
/*++
Routine Description:

Interrupt handler for the device.

Arguments:

Interupt - Address of the KINTERRUPT Object for our device.
ServiceContext - Pointer to our adapter

Return Value:

TRUE if our device is interrupting, FALSE otherwise.

--*/
{
	//BOOLEAN     InterruptRecognized = FALSE;
	CCaptureDevice *CapDevice = NULL;
	CapDevice = reinterpret_cast <CCaptureDevice *> (ServiceContext);
	//USHORT      IntStatus;

	TraceInfo(DBG_IRQ, "--> AdmaInterruptHandler\n");


	TraceInfo(DBG_IRQ, "Requesting DPC\n");

	IoRequestDpc(CapDevice->m_Device->FunctionalDeviceObject, NULL, reinterpret_cast <PVOID> (CapDevice));

	TraceInfo(DBG_IRQ, "<-- AdmaInterruptHandler\n");

	return TRUE;// InterruptRecognized;
}

/*************************************************/

BOOLEAN
CCaptureDevice::
AdmaInterruptMessageService(
	__in PKINTERRUPT Interrupt,
	__in PVOID  ServiceContext,
	__in ULONG  MessageId
)
/*++
Routine Description:

Interrupt handler for the device.

Arguments:

Interupt - Address of the KINTERRUPT Object for our device.
ServiceContext - Pointer to our adapter

Return Value:

TRUE if our device is interrupting, FALSE otherwise.

--*/
{
	//BOOLEAN     InterruptRecognized = FALSE;
	CCaptureDevice *CapDevice = NULL;
	CapDevice = reinterpret_cast <CCaptureDevice *> (ServiceContext);
	//USHORT      IntStatus;

	TraceInfo(DBG_IRQ, "--> AdmaInterruptHandler\n");


	TraceInfo(DBG_IRQ, "Requesting DPC MessageId %d\n", MessageId);

	IoRequestDpc(CapDevice->m_Device->FunctionalDeviceObject, NULL, reinterpret_cast <PVOID> (CapDevice));

	TraceInfo(DBG_IRQ, "<-- AdmaInterruptHandler\n");

	return TRUE;// InterruptRecognized;
}


/*************************************************/


NTSTATUS 
CCaptureDevice::
SetupInterrupts(
	IN PCM_RESOURCE_LIST TranslatedResourceList,
	IN PCM_RESOURCE_LIST UntranslatedResourceList
)
{
	PAGED_CODE();

	NTSTATUS status = STATUS_SUCCESS;
	DEVICE_DESCRIPTION              deviceDescription;
#if defined(DMA_VER2) // To avoid  unreferenced local variables error
	ULONG                           SGMapRegsisters;
	ULONG                           ScatterGatherListSize;
#endif
	IO_CONNECT_INTERRUPT_PARAMETERS interruptParams;
	PVOID intrMsgConnectionContext;
	//
	// Disable interrupts here which is as soon as possible
	//
	// to do
	IoInitializeDpcRequest(m_Device->FunctionalDeviceObject, 
		reinterpret_cast <PIO_DPC_ROUTINE> (CCaptureDevice::AdmaDpcForIsr));

	//
	// Register the interrupt
	//
#if 0
	if (m_InterruptFlags & CM_RESOURCE_INTERRUPT_LATCHED) {
		m_InterruptMode = Latched;
	}
	else {
		m_InterruptMode = LevelSensitive;
	}

	status = IoConnectInterrupt(&m_Interrupt,
		reinterpret_cast <PKSERVICE_ROUTINE> (CCaptureDevice::AdmaInterruptHandler),
		reinterpret_cast <PVOID> (this),                   // ISR Context
		NULL,
		m_InterruptVector,
		m_InterruptLevel,
		m_InterruptLevel,
		m_InterruptMode,
		TRUE, // shared interrupt
		m_InterruptAffinity,
		FALSE);
	if (status != STATUS_SUCCESS)
	{
		TraceError(DBG_INIT, "IoConnectInterrupt failed %x\n", status);
		goto End;
	}
#endif
#if 0
	interruptParams.Version = CONNECT_FULLY_SPECIFIED;
	interruptParams.FullySpecified.PhysicalDeviceObject = m_Device->PhysicalDeviceObject;
	interruptParams.FullySpecified.InterruptObject = &m_Interrupt;
	interruptParams.FullySpecified.ServiceRoutine = reinterpret_cast <PKSERVICE_ROUTINE> (CCaptureDevice::AdmaInterruptHandler);
	interruptParams.FullySpecified.ServiceContext = reinterpret_cast <PVOID> (this);
	interruptParams.FullySpecified.FloatingSave = FALSE;
	interruptParams.FullySpecified.SpinLock = NULL;
	interruptParams.FullySpecified.Vector = m_InterruptVector;
	interruptParams.FullySpecified.Irql = (KIRQL)m_InterruptLevel;
	interruptParams.FullySpecified.SynchronizeIrql = (KIRQL)m_InterruptLevel;
	interruptParams.FullySpecified.ProcessorEnableMask = m_InterruptAffinity;
	interruptParams.FullySpecified.InterruptMode = (m_InterruptFlags & CM_RESOURCE_INTERRUPT_LATCHED ? Latched : LevelSensitive);
	interruptParams.FullySpecified.ShareVector = (BOOLEAN)(m_ShareDisposition == CmResourceShareShared);
	status = IoConnectInterruptEx(&interruptParams);

	if (status != STATUS_SUCCESS) {
		TraceError(DBG_INIT, "IoConnectInterruptEx failed %x\n", status);
		goto End;
	}
#endif

	RtlZeroMemory(&interruptParams, sizeof(IO_CONNECT_INTERRUPT_PARAMETERS));
	interruptParams.Version = CONNECT_MESSAGE_BASED;
	interruptParams.MessageBased.PhysicalDeviceObject = m_Device->PhysicalDeviceObject;
	interruptParams.MessageBased.ConnectionContext.Generic = &intrMsgConnectionContext;
	interruptParams.MessageBased.MessageServiceRoutine = reinterpret_cast <PKMESSAGE_SERVICE_ROUTINE> (CCaptureDevice::AdmaInterruptMessageService);
	interruptParams.MessageBased.ServiceContext = reinterpret_cast <PVOID> (this);
	interruptParams.MessageBased.SpinLock = NULL;
	interruptParams.MessageBased.SynchronizeIrql = 0;
	interruptParams.MessageBased.FloatingSave = FALSE;
	interruptParams.MessageBased.FallBackServiceRoutine = reinterpret_cast <PKSERVICE_ROUTINE> (CCaptureDevice::AdmaInterruptHandler);

	status = IoConnectInterruptEx(&interruptParams);//WdmlibIoConnectInterruptEx(&interruptParams); //
	if (NT_SUCCESS(status)) {
		// We record the type of ISR registered.
		m_InterruptType = interruptParams.Version;
		if (m_InterruptType == CONNECT_MESSAGE_BASED) {
			m_InterruptMessageTable = (PIO_INTERRUPT_MESSAGE_INFO)intrMsgConnectionContext;
			TraceInfo(DBG_INIT, "MSI/MSI-X interrupt Allocated msg num %d\n", m_InterruptMessageTable->MessageCount);
		}
		else {
			m_Interrupt = (PKINTERRUPT)intrMsgConnectionContext;
		}
	}
	else {
		// Operation failed. Handle error.
		TraceError(DBG_INIT, "IoConnectInterruptEx failed %x\n", status);
		goto End;
	}

	//MP_SET_FLAG(FdoData, fMP_ADAPTER_INTERRUPT_IN_USE);

	//
	// Zero out the entire structure first.
	//
	RtlZeroMemory(&deviceDescription, sizeof(DEVICE_DESCRIPTION));

	//
	// DMA_VER2 is defined when the driver is built to work on XP and
	// above. The difference between DMA_VER2 and VER0 is that VER2
	// support BuildScatterGatherList & CalculateScatterGatherList functions.
	// BuildScatterGatherList performs the same operation as
	// GetScatterGatherList, except that it uses the buffer supplied
	// in the ScatterGatherBuffer parameter to hold the scatter/gather
	// list that it creates. In contrast, GetScatterGatherList
	// dynamically allocates a buffer to hold the scatter/gather list.
	// If insufficient memory is available to allocate the buffer,
	// GetScatterGatherList can fail with a STATUS_INSUFFICIENT_RESOURCES
	// error. Drivers that must avoid this scenario can pre-allocate a
	// buffer to hold the scatter/gather list, and use BuildScatterGatherList
	// instead. A driver can use the CalculateScatterGatherList routine
	// to determine the size of buffer to allocate to hold the
	// scatter/gather list.
	//
#if defined(DMA_VER2)
	deviceDescription.Version = DEVICE_DESCRIPTION_VERSION2;
#else
	deviceDescription.Version = DEVICE_DESCRIPTION_VERSION;
#endif

	deviceDescription.Master = TRUE;
	deviceDescription.ScatterGather = TRUE;
	deviceDescription.Dma32BitAddresses = TRUE;
	deviceDescription.Dma64BitAddresses = FALSE;
	deviceDescription.InterfaceType = PCIBus;
	deviceDescription.MaximumLength = ADMA_MAX_TRANSFER_SIZE;// 128 << PAGE_SHIFT;

	m_DmaAdapterObject = IoGetDmaAdapter(m_Device->PhysicalDeviceObject,
		&deviceDescription,
		&m_NumberOfMapRegisters);
	if (m_DmaAdapterObject == NULL)
	{
		TraceError(DBG_INIT, "IoGetDmaAdapter failed\n");
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto End;
	}

	TraceInfo(DBG_INIT, "MapRegisters Allocated %d\n", m_NumberOfMapRegisters);
	//MP_SET_FLAG(FdoData, fMP_ADAPTER_SCATTER_GATHER);

#if defined(DMA_VER2)

	status = DmaAdapterObject->DmaOperations->CalculateScatterGatherList(
		DmaAdapterObject,
		NULL,
		0,
		MapRegisters * PAGE_SIZE,
		&ScatterGatherListSize,
		&SGMapRegsisters);


	ASSERT(NT_SUCCESS(status));
	ASSERT(SGMapRegsisters == MapRegisters);
	if (!NT_SUCCESS(status))
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto End;
	}

	FdoData->ScatterGatherListSize = ScatterGatherListSize;

#endif

	//
	// For convenience, let us save the frequently used DMA operational
	// functions in our device context.
	//
	AllocateCommonBuffer =
		*m_DmaAdapterObject->DmaOperations->AllocateCommonBuffer;
	FreeCommonBuffer =
		*m_DmaAdapterObject->DmaOperations->FreeCommonBuffer;

	//
	// Initialize our DMA adapter object with AVStream.  This is 
	// **ONLY** necessary **IF** you are doing DMA directly into
	// capture buffers as this sample does.  For this,
	// KSPIN_FLAG_GENERATE_MAPPINGS must be specified on a queue.
	//

	PUNKNOWN DeviceUnk = KsDeviceGetOuterUnknown(
			m_Device);

	// Register the DMA adapter with AVStream
	IKsDeviceFunctions *DeviceFunctions;
	status = DeviceUnk->QueryInterface(
		__uuidof (IKsDeviceFunctions),
		(PVOID *)&DeviceFunctions);

	// Conditionally, call IksDeviceFunctions::RegisterAdapterObjectEx, 
	// which will not break downlevel load compatibility.

	if (NT_SUCCESS(status)) {
		DeviceFunctions->RegisterAdapterObjectEx(
			m_DmaAdapterObject,
			&deviceDescription,
			m_NumberOfMapRegisters,
			ADMA_MAX_TRANSFER_SIZE,
			sizeof(KSMAPPING)
		);
		DeviceFunctions->Release();
}

	// If this call fails, call KsDeviceRegisterAdapterObject to
	// preserve downlevel load compatibility.
	else {
		KsDeviceRegisterAdapterObject(
			m_Device,
			m_DmaAdapterObject,
			ADMA_MAX_TRANSFER_SIZE,
			sizeof(KSMAPPING)
		);
	}

End:
	//
	// If we have jumped here due to any kind of mapping or resource allocation
	// failure, we should clean up. Since we know that if we fail Start-Device,
	// the system is going to send Remove-Device request, we will defer the
	// job of cleaning to NICUnmapHWResources which is called in the Remove path.
	//
	return status;
}

/*************************************************/


NTSTATUS
CCaptureDevice::
PnpStart (
    IN PCM_RESOURCE_LIST TranslatedResourceList,
    IN PCM_RESOURCE_LIST UntranslatedResourceList
    )

/*++

Routine Description:

    Called at Pnp start.  We start up our virtual hardware simulation.

Arguments:

    TranslatedResourceList -
        The translated resource list from Pnp

    UntranslatedResourceList -
        The untranslated resource list from Pnp

Return Value:

    Success / Failure

--*/

{

    PAGED_CODE();

    //
    // Normally, we'd do things here like parsing the resource lists and
    // connecting our interrupt.  Since this is a simulation, there isn't
    // much to parse.  The parsing and connection should be the same as
    // any WDM driver.  The sections that will differ are illustrated below
    // in setting up a simulated DMA.
    //

    NTSTATUS Status = STATUS_SUCCESS;

    if (!m_Device -> Started) {
        // Create the Filter for the device
        KsAcquireDevice(m_Device);
        Status = KsCreateFilterFactory( m_Device->FunctionalDeviceObject,
                                        &CaptureFilterDescriptor,
                                        L"GLOBAL",
                                        NULL,
                                        KSCREATE_ITEM_FREEONSTOP,
                                        NULL,
                                        NULL,
                                        NULL );
        KsReleaseDevice(m_Device);

    }
    //
    // By PnP, it's possible to receive multiple starts without an intervening
    // stop (to reevaluate resources, for example).  Thus, we only perform
    // creations of the simulation on the initial start and ignore any 
    // subsequent start.  Hardware drivers with resources should evaluate
    // resources and make changes on 2nd start.
    //
    if (NT_SUCCESS(Status) && (!m_Device -> Started)) {

        m_HardwareSimulation = new (NonPagedPoolNx, 'miSH') CHardwareSimulation (this);
        if (!m_HardwareSimulation) {
            //
            // If we couldn't create the hardware simulation, fail.
            //
            Status = STATUS_INSUFFICIENT_RESOURCES;
    
        } else {
            Status = KsAddItemToObjectBag (
                m_Device -> Bag,
                reinterpret_cast <PVOID> (m_HardwareSimulation),
                reinterpret_cast <PFNKSFREE> (CHardwareSimulation::Cleanup)
                );

            if (!NT_SUCCESS (Status)) {
                delete m_HardwareSimulation;
            }
        }
		
		MapResources(TranslatedResourceList, UntranslatedResourceList);

		SetupInterrupts(TranslatedResourceList, UntranslatedResourceList);
    }
    
    return Status;

}

/*************************************************/


void
CCaptureDevice::
PnpStop (
    )

/*++

Routine Description:

    This is the pnp stop dispatch for the capture device.  It releases any
    adapter object previously allocated by IoGetDmaAdapter during Pnp Start.

Arguments:

    None

Return Value:

    None

--*/

{

    PAGED_CODE();

    if (m_DmaAdapterObject) {
        //
        // Return the DMA adapter back to the system.
        //
        m_DmaAdapterObject -> DmaOperations -> 
            PutDmaAdapter (m_DmaAdapterObject);

        m_DmaAdapterObject = NULL;
    }

}

/*************************************************/


NTSTATUS
CCaptureDevice::
AcquireHardwareResources (
    IN ICaptureSink *CaptureSink,
    IN PKS_VIDEOINFOHEADER VideoInfoHeader
    )

/*++

Routine Description:

    Acquire hardware resources for the capture hardware.  If the 
    resources are already acquired, this will return an error.
    The hardware configuration must be passed as a VideoInfoHeader.

Arguments:

    CaptureSink -
        The capture sink attempting to acquire resources.  When scatter /
        gather mappings are completed, the capture sink specified here is
        what is notified of the completions.

    VideoInfoHeader -
        Information about the capture stream.  This **MUST** remain
        stable until the caller releases hardware resources.  Note
        that this could also be guaranteed by bagging it in the device
        object bag as well.

Return Value:

    Success / Failure

--*/

{

    PAGED_CODE();

    NTSTATUS Status = STATUS_SUCCESS;

    //
    // If we're the first pin to go into acquire (remember we can have
    // a filter in another graph going simultaneously), grab the resources.
    //
    if (InterlockedCompareExchange (
        &m_PinsWithResources,
        1,
        0) == 0) {

        m_VideoInfoHeader = VideoInfoHeader;

        //
        // If there's an old hardware simulation sitting around for some
        // reason, blow it away.
        //
        if (m_ImageSynth) {
            delete m_ImageSynth;
            m_ImageSynth = NULL;
        }
    
        //
        // Create the necessary type of image synthesizer.
        //
        if (m_VideoInfoHeader -> bmiHeader.biBitCount == 24 &&
            m_VideoInfoHeader -> bmiHeader.biCompression == KS_BI_RGB) {
    
            //
            // If we're RGB24, create a new RGB24 synth.  RGB24 surfaces
            // can be in either orientation.  The origin is lower left if
            // height < 0.  Otherwise, it's upper left.
            //
            m_ImageSynth = new (NonPagedPoolNx, 'RysI') 
                CRGB24Synthesizer (
                    m_VideoInfoHeader -> bmiHeader.biHeight >= 0
                    );
    
        } else
        if (m_VideoInfoHeader -> bmiHeader.biBitCount == 16 &&
           (m_VideoInfoHeader -> bmiHeader.biCompression == FOURCC_YUY2)) {
    
            //
            // If we're UYVY, create the YUV synth.
            //
            m_ImageSynth = new(NonPagedPoolNx, 'YysI') CYUVSynthesizer;
    
        }
        else
            //
            // We don't synthesize anything but RGB 24 and UYVY.
            //
            Status = STATUS_INVALID_PARAMETER;
    
        if (NT_SUCCESS (Status) && !m_ImageSynth) {
    
            Status = STATUS_INSUFFICIENT_RESOURCES;
    
        } 

        if (NT_SUCCESS (Status)) {
            //
            // If everything has succeeded thus far, set the capture sink.
            //
            m_CaptureSink = CaptureSink;

        } else {
            //
            // If anything failed in here, we release the resources we've
            // acquired.
            //
            ReleaseHardwareResources ();
        }
    
    } else {

        //
        // TODO: Better status code?
        //
        Status = STATUS_SHARING_VIOLATION;

    }

    return Status;

}

/*************************************************/


void
CCaptureDevice::
ReleaseHardwareResources (
    )

/*++

Routine Description:

    Release hardware resources.  This should only be called by
    an object which has acquired them.

Arguments:

    None

Return Value:

    None

--*/

{

    PAGED_CODE();

    //
    // Blow away the image synth.
    //
    if (m_ImageSynth) {
        delete m_ImageSynth;
        m_ImageSynth = NULL;

    }

    m_VideoInfoHeader = NULL;
    m_CaptureSink = NULL;

    //
    // Release our "lock" on hardware resources.  This will allow another
    // pin (perhaps in another graph) to acquire them.
    //
    InterlockedExchange (
        &m_PinsWithResources,
        0
        );

}

/*************************************************/


NTSTATUS
CCaptureDevice::
Start (
    )

/*++

Routine Description:

    Start the capture device based on the video info header we were told
    about when resources were acquired.

Arguments:

    None

Return Value:

    Success / Failure

--*/

{

    PAGED_CODE();

    m_LastMappingsCompleted = 0;
    m_InterruptTime = 0;

    return
        m_HardwareSimulation -> Start (
            m_ImageSynth,
            m_VideoInfoHeader -> AvgTimePerFrame,
            m_VideoInfoHeader -> bmiHeader.biWidth,
            ABS (m_VideoInfoHeader -> bmiHeader.biHeight),
            m_VideoInfoHeader -> bmiHeader.biSizeImage
            );


}

/*************************************************/


NTSTATUS
CCaptureDevice::
Pause (
    IN BOOLEAN Pausing
    )

/*++

Routine Description:

    Pause or unpause the hardware simulation.  This is an effective start
    or stop without resetting counters and formats.  Note that this can
    only be called to transition from started -> paused -> started.  Calling
    this without starting the hardware with Start() does nothing.

Arguments:

    Pausing -
        An indicatation of whether we are pausing or unpausing

        TRUE -
            Pause the hardware simulation

        FALSE -
            Unpause the hardware simulation

Return Value:

    Success / Failure

--*/

{

    PAGED_CODE();

    return
        m_HardwareSimulation -> Pause (
            Pausing
            );

}

/*************************************************/


NTSTATUS
CCaptureDevice::
Stop (
    )

/*++

Routine Description:

    Stop the capture device.

Arguments:

    None

Return Value:

    Success / Failure

--*/

{

    PAGED_CODE();

    return
        m_HardwareSimulation -> Stop ();

}

/*************************************************/


ULONG
CCaptureDevice::
ProgramScatterGatherMappings (
    IN PKSSTREAM_POINTER Clone,
    IN PUCHAR *Buffer,
    IN PKSMAPPING Mappings,
    IN ULONG MappingsCount
    )

/*++

Routine Description:

    Program the scatter / gather mappings for the "fake" hardware.

Arguments:

    Buffer -
        Points to a pointer to the virtual address of the topmost
        scatter / gather chunk.  The pointer will be updated as the
        device "programs" mappings.  Reason for this is that we get
        the physical addresses and sizes, but must calculate the virtual
        addresses...  This is used as scratch space for that.

    Mappings -
        An array of mappings to program

    MappingsCount -
        The count of mappings in the array

Return Value:

    The number of mappings successfully programmed

--*/

{

    PAGED_CODE();

    

    return 
        m_HardwareSimulation -> ProgramScatterGatherMappings (
            Clone,
            Buffer,
            Mappings,
            MappingsCount,
            sizeof (KSMAPPING)
            );

}

/*************************************************************************

    LOCKED CODE

**************************************************************************/

#ifdef ALLOC_PRAGMA
#pragma code_seg()
#endif // ALLOC_PRAGMA


ULONG
CCaptureDevice::
QueryInterruptTime (
    )

/*++

Routine Description:

    Return the number of frame intervals that have elapsed since the
    start of the device.  This will be the frame number.

Arguments:

    None

Return Value:

    The interrupt time of the device (the number of frame intervals that
    have elapsed since the start of the device).

--*/

{

    return m_InterruptTime;

}

/*************************************************/


void
CCaptureDevice::
Interrupt (
    )

/*++

Routine Description:

    This is the "faked" interrupt service routine for this device.  It
    is called at dispatch level by the hardware simulation.

Arguments:

    None

Return Value:

    None

--*/

{

    m_InterruptTime++;

    //
    // Realistically, we'd do some hardware manipulation here and then queue
    // a DPC.  Since this is fake hardware, we do what's necessary here.  This
    // is pretty much what the DPC would look like short of the access
    // of hardware registers (ReadNumberOfMappingsCompleted) which would likely
    // be done in the ISR.
    //
    ULONG NumMappingsCompleted = 
        m_HardwareSimulation -> ReadNumberOfMappingsCompleted ();

    //
    // Inform the capture sink that a given number of scatter / gather
    // mappings have completed.
    //
    m_CaptureSink -> CompleteMappings (
        NumMappingsCompleted - m_LastMappingsCompleted
        );

    m_LastMappingsCompleted = NumMappingsCompleted;

}

/**************************************************************************

    DESCRIPTOR AND DISPATCH LAYOUT

**************************************************************************/

//
// CaptureFilterDescriptor:
//
// The filter descriptor for the capture device.
DEFINE_KSFILTER_DESCRIPTOR_TABLE (FilterDescriptors) { 
    &CaptureFilterDescriptor
};

//
// CaptureDeviceDispatch:
//
// This is the dispatch table for the capture device.  Plug and play
// notifications as well as power management notifications are dispatched
// through this table.
//
const
KSDEVICE_DISPATCH
CaptureDeviceDispatch = {
    CCaptureDevice::DispatchCreate,         // Pnp Add Device
    CCaptureDevice::DispatchPnpStart,       // Pnp Start
    NULL,                                   // Post-Start
    NULL,                                   // Pnp Query Stop
    NULL,                                   // Pnp Cancel Stop
    CCaptureDevice::DispatchPnpStop,        // Pnp Stop
    NULL,                                   // Pnp Query Remove
    NULL,                                   // Pnp Cancel Remove
    NULL,                                   // Pnp Remove
    NULL,                                   // Pnp Query Capabilities
    NULL,                                   // Pnp Surprise Removal
    NULL,                                   // Power Query Power
    NULL,                                   // Power Set Power
    NULL                                    // Pnp Query Interface
};

//
// CaptureDeviceDescriptor:
//
// This is the device descriptor for the capture device.  It points to the
// dispatch table and contains a list of filter descriptors that describe
// filter-types that this device supports.  Note that the filter-descriptors
// can be created dynamically and the factories created via 
// KsCreateFilterFactory as well.  
//
const
KSDEVICE_DESCRIPTOR
CaptureDeviceDescriptor = {
    &CaptureDeviceDispatch,
    0,
    NULL
};

/**************************************************************************

    INITIALIZATION CODE

**************************************************************************/


extern "C" DRIVER_INITIALIZE DriverEntry;
const char * const dateTimeStr = "Built " __DATE__ ", " __TIME__ ".";

extern "C"
NTSTATUS
DriverEntry (
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    Driver entry point.  Pass off control to the AVStream initialization
    function (KsInitializeDriver) and return the status code from it.

Arguments:

    DriverObject -
        The WDM driver object for our driver

    RegistryPath -
        The registry path for our registry info

Return Value:

    As from KsInitializeDriver

--*/

{
	// Initialize WPP Tracing
	WPP_INIT_TRACING(DriverObject, RegistryPath);
	TraceInfo(DBG_INIT, "avsadma Driver - %s", dateTimeStr);

    //
    // Simply pass the device descriptor and parameters off to AVStream
    // to initialize us.  This will cause filter factories to be set up
    // at add & start.  Everything is done based on the descriptors passed
    // here.
    //
    return 
        KsInitializeDriver (
            DriverObject,
            RegistryPath,
            &CaptureDeviceDescriptor
            );

}

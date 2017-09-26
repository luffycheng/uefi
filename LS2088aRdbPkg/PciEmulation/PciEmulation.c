/** PciEmulation.c
 Provides all functions of PCI Host Bridge Resource Allocation Protocol

  Reference taken from PCI Emulation implementation in
  Omap35xxPkg/PciEmulation/
  ArmPlatformPkg/ArmJunoPkg/Drivers/ArmJunoDxe/

  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
  Copyright (c) 2016, Freescale Semiconductor, Inc.
  Copyright 2017 NXP

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution. The full text of the license may be found
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

 **/

#include <Library/UsbHcd.h>
#include "PciEmulation.h"

UINTN UsbRegAdd[NUM_OF_USB_CONTROLLERS] = {USB1_REG_ADDR, USB2_REG_ADDR};

typedef struct {
  ACPI_HID_DEVICE_PATH      AcpiDevicePath;
  PCI_DEVICE_PATH           PciDevicePath;
  EFI_DEVICE_PATH_PROTOCOL  EndDevicePath;
} EFI_PCI_IO_DEVICE_PATH;

typedef struct {
  UINT32                  Signature;
  EFI_PCI_IO_DEVICE_PATH  DevicePath;
  EFI_PCI_IO_PROTOCOL     PciIoProtocol;
  PCI_TYPE00              *ConfigSpace;
  PCI_ROOT_BRIDGE         RootBridge;
  UINTN                   Segment;
} EFI_PCI_IO_PRIVATE_DATA;

#define EFI_PCI_IO_PRIVATE_DATA_SIGNATURE     SIGNATURE_32('p', 'c', 'i', 'o')
#define EFI_PCI_IO_PRIVATE_DATA_FROM_THIS(a)  CR(a, EFI_PCI_IO_PRIVATE_DATA, PciIoProtocol, EFI_PCI_IO_PRIVATE_DATA_SIGNATURE)

EFI_PCI_IO_DEVICE_PATH PciIoDevicePathTemplate =
{
  {
    { ACPI_DEVICE_PATH, ACPI_DP, { sizeof (ACPI_HID_DEVICE_PATH), 0 } },
    EISA_PNP_ID(0x0A03),  // HID
    0                     // UID
  },
  {
    { HARDWARE_DEVICE_PATH, HW_PCI_DP, { sizeof (PCI_DEVICE_PATH), 0 } },
    0,
    0
  },
  { END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, { sizeof (EFI_DEVICE_PATH_PROTOCOL), 0} }
};

EFI_STATUS
PciIoPollMem (
  IN EFI_PCI_IO_PROTOCOL           *This,
  IN  EFI_PCI_IO_PROTOCOL_WIDTH    Width,
  IN  UINT8                        BarIndex,
  IN  UINT64                       Offset,
  IN  UINT64                       Mask,
  IN  UINT64                       Value,
  IN  UINT64                       Delay,
  OUT UINT64                       *Result
  )
{
  ASSERT (FALSE);
  return EFI_UNSUPPORTED;
}

EFI_STATUS
PciIoPollIo (
  IN EFI_PCI_IO_PROTOCOL           *This,
  IN  EFI_PCI_IO_PROTOCOL_WIDTH    Width,
  IN  UINT8                        BarIndex,
  IN  UINT64                       Offset,
  IN  UINT64                       Mask,
  IN  UINT64                       Value,
  IN  UINT64                       Delay,
  OUT UINT64                       *Result
  )
{
  ASSERT (FALSE);
  return EFI_UNSUPPORTED;
}

EFI_STATUS
PciIoMemRead (
  IN EFI_PCI_IO_PROTOCOL              *This,
  IN     EFI_PCI_IO_PROTOCOL_WIDTH    Width,
  IN     UINT8                        BarIndex,
  IN     UINT64                       Offset,
  IN     UINTN                        Count,
  IN OUT VOID                         *Buffer
  )
{
  EFI_PCI_IO_PRIVATE_DATA *Private = EFI_PCI_IO_PRIVATE_DATA_FROM_THIS(This);

  return PciRootBridgeIoMemRead (&Private->RootBridge.Io,
                                (EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH) Width,
                                Private->ConfigSpace->Device.Bar[0] + Offset,
                                Count,
                                Buffer
                                );
}

EFI_STATUS
PciIoMemWrite (
  IN EFI_PCI_IO_PROTOCOL              *This,
  IN     EFI_PCI_IO_PROTOCOL_WIDTH    Width,
  IN     UINT8                        BarIndex,
  IN     UINT64                       Offset,
  IN     UINTN                        Count,
  IN OUT VOID                         *Buffer
  )
{
  EFI_PCI_IO_PRIVATE_DATA *Private = EFI_PCI_IO_PRIVATE_DATA_FROM_THIS(This);

  return PciRootBridgeIoMemWrite (&Private->RootBridge.Io,
                                 (EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH) Width,
                                 Private->ConfigSpace->Device.Bar[0] + Offset,
                                 Count,
                                 Buffer
                                 );
}

EFI_STATUS
PciIoIoRead (
  IN EFI_PCI_IO_PROTOCOL              *This,
  IN     EFI_PCI_IO_PROTOCOL_WIDTH    Width,
  IN     UINT8                        BarIndex,
  IN     UINT64                       Offset,
  IN     UINTN                        Count,
  IN OUT VOID                         *Buffer
  )
{
  ASSERT (FALSE);
  return EFI_UNSUPPORTED;
}

EFI_STATUS
PciIoIoWrite (
  IN EFI_PCI_IO_PROTOCOL              *This,
  IN     EFI_PCI_IO_PROTOCOL_WIDTH    Width,
  IN     UINT8                        BarIndex,
  IN     UINT64                       Offset,
  IN     UINTN                        Count,
  IN OUT VOID                         *Buffer
  )
{
  ASSERT (FALSE);
  return EFI_UNSUPPORTED;
}

/**
  Enable a PCI driver to read PCI controller registers in PCI configuration space.

  @param[in]      This    A pointer to the EFI_PCI_IO_PROTOCOL instance.
  @param[in]      Width   Signifies the width of the memory operations.
  @param[in]      Offset  The offset within the PCI configuration space for
                          the PCI controller.
  @param[in]      Count   The number of PCI configuration operations to
                          perform. Bytes moved is Width size * Count,
                          starting at Offset.

  @param[in out]  Buffer  The destination buffer to store the results.

  @retval  EFI_SUCCESS            The data was read from the PCI controller.
  @retval  EFI_INVALID_PARAMETER  "Width" is invalid.
  @retval  EFI_INVALID_PARAMETER  "Buffer" is NULL.

**/
EFI_STATUS
PciIoPciRead (
  IN     EFI_PCI_IO_PROTOCOL       *This,
  IN     EFI_PCI_IO_PROTOCOL_WIDTH  Width,
  IN     UINT32                     Offset,
  IN     UINTN                      Count,
  IN OUT VOID                      *Buffer
  )
{
  EFI_PCI_IO_PRIVATE_DATA *Private = EFI_PCI_IO_PRIVATE_DATA_FROM_THIS (This);
  EFI_STATUS Status;
  UINT64 Address;

  if ((Width < 0) || (Width >= EfiPciIoWidthMaximum) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Address = (UINT64)((UINT8 *)Private->ConfigSpace + Offset);
  Status = PciRootBridgeIoMemRW (
	     FALSE,  
             (EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH)Width,
             Address,
             Count,
             Buffer
             );

  return Status;
}

/**
  Enable a PCI driver to write PCI controller registers in PCI configuration space.

  @param[in]      This    A pointer to the EFI_PCI_IO_PROTOCOL instance.
  @param[in]      Width   Signifies the width of the memory operations.
  @param[in]      Offset  The offset within the PCI configuration space for
                          the PCI controller.
  @param[in]      Count   The number of PCI configuration operations to
                          perform. Bytes moved is Width size * Count,
                          starting at Offset.

  @param[in out]  Buffer  The source buffer to write data from.

  @retval  EFI_SUCCESS            The data was read from the PCI controller.
  @retval  EFI_INVALID_PARAMETER  "Width" is invalid.
  @retval  EFI_INVALID_PARAMETER  "Buffer" is NULL.

**/
EFI_STATUS
PciIoPciWrite (
  IN EFI_PCI_IO_PROTOCOL              *This,
  IN     EFI_PCI_IO_PROTOCOL_WIDTH    Width,
  IN     UINT32                       Offset,
  IN     UINTN                        Count,
  IN OUT VOID                         *Buffer
  )
{
  EFI_PCI_IO_PRIVATE_DATA *Private = EFI_PCI_IO_PRIVATE_DATA_FROM_THIS (This);

  if ((Width < 0) || (Width >= EfiPciIoWidthMaximum) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  return PciRootBridgeIoMemRW (TRUE,
		  	       (EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH) Width,
                               (UINT64)(Private->ConfigSpace + Offset),
                               Count,
                               Buffer
                               );
}

EFI_STATUS
PciIoCopyMem (
  IN EFI_PCI_IO_PROTOCOL              *This,
  IN     EFI_PCI_IO_PROTOCOL_WIDTH    Width,
  IN     UINT8                        DestBarIndex,
  IN     UINT64                       DestOffset,
  IN     UINT8                        SrcBarIndex,
  IN     UINT64                       SrcOffset,
  IN     UINTN                        Count
  )
{
  ASSERT (FALSE);
  return EFI_UNSUPPORTED;
}

EFI_STATUS
PciIoMap (
  IN EFI_PCI_IO_PROTOCOL                *This,
  IN     EFI_PCI_IO_PROTOCOL_OPERATION  Operation,
  IN     VOID                           *HostAddress,
  IN OUT UINTN                          *NumberOfBytes,
  OUT    EFI_PHYSICAL_ADDRESS           *DeviceAddress,
  OUT    VOID                           **Mapping
  )
{
  DMA_MAP_OPERATION   DmaOperation;

  if (Operation == EfiPciIoOperationBusMasterRead) {
    DmaOperation = MapOperationBusMasterRead;
  } else if (Operation == EfiPciIoOperationBusMasterWrite) {
    DmaOperation = MapOperationBusMasterWrite;
  } else if (Operation == EfiPciIoOperationBusMasterCommonBuffer) {
    DmaOperation = MapOperationBusMasterCommonBuffer;
  } else {
    return EFI_INVALID_PARAMETER;
  }

  return DmaMap (DmaOperation, HostAddress, NumberOfBytes, DeviceAddress, Mapping);
}

EFI_STATUS
PciIoUnmap (
  IN EFI_PCI_IO_PROTOCOL           *This,
  IN  VOID                         *Mapping
  )
{
  return DmaUnmap (Mapping);
}

/**
  Allocate pages that are suitable for an EfiPciIoOperationBusMasterCommonBuffer
  mapping.

  @param[in]   This         A pointer to the EFI_PCI_IO_PROTOCOL instance.
  @param[in]   Type         This parameter is not used and must be ignored.
  @param[in]   MemoryType   The type of memory to allocate, EfiBootServicesData or
                            EfiRuntimeServicesData.
  @param[in]   Pages        The number of pages to allocate.
  @param[out]  HostAddress  A pointer to store the base system memory address of
                            the allocated range.
  @param[in]   Attributes   The requested bit mask of attributes for the allocated
                            range. Only the attributes,
                            EFI_PCI_ATTRIBUTE_MEMORY_WRITE_COMBINE and
                            EFI_PCI_ATTRIBUTE_MEMORY_CACHED may be used with this
                            function. If any other bits are set, then EFI_UNSUPPORTED
                            is returned. This function ignores this bit mask.

  @retval  EFI_SUCCESS            The requested memory pages were allocated.
  @retval  EFI_INVALID_PARAMETER  HostAddress is NULL.
  @retval  EFI_INVALID_PARAMETER  MemoryType is invalid.
  @retval  EFI_UNSUPPORTED        Attributes is unsupported.
  @retval  EFI_OUT_OF_RESOURCES   The memory pages could not be allocated.

**/
EFI_STATUS
PciIoAllocateBuffer (
  IN EFI_PCI_IO_PROTOCOL  *This,
  IN  EFI_ALLOCATE_TYPE   Type,
  IN  EFI_MEMORY_TYPE     MemoryType,
  IN  UINTN               Pages,
  OUT VOID                **HostAddress,
  IN  UINT64              Attributes
  )
{
  if (Attributes &
      (~(EFI_PCI_ATTRIBUTE_MEMORY_WRITE_COMBINE |
         EFI_PCI_ATTRIBUTE_MEMORY_CACHED))) {
    return EFI_UNSUPPORTED;
  }

  return DmaAllocateBuffer (MemoryType, Pages, HostAddress);
}

/**
   Frees memory that was allocated with AllocateBuffer().

   The FreeBuffer() function frees memory that was allocated with AllocateBuffer().

   @param This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
   @param Pages       The number of pages to free.
   @param HostAddress The base system memory address of the allocated range.
   
   @retval EFI_SUCCESS            The requested memory pages were freed.
   @retval EFI_INVALID_PARAMETER  The memory range specified by HostAddress and Pages
                                  was not allocated with AllocateBuffer().

**/

EFI_STATUS
PciIoFreeBuffer (
  IN EFI_PCI_IO_PROTOCOL           *This,
  IN  UINTN                        Pages,
  IN  VOID                         *HostAddress
  )
{
  return DmaFreeBuffer (Pages, HostAddress);
}

EFI_STATUS
PciIoFlush (
  IN EFI_PCI_IO_PROTOCOL  *This
  )
{
  //
  // not supported yet
  //
  return EFI_SUCCESS;
}

/**
  Retrieves this PCI controller's current PCI bus number, device number, and function number.

  @param[in]   This            A pointer to the EFI_PCI_IO_PROTOCOL instance.
  @param[out]  SegmentNumber   The PCI controller's current PCI segment number.
  @param[out]  BusNumber       The PCI controller's current PCI bus number.
  @param[out]  DeviceNumber    The PCI controller's current PCI device number.
  @param[out]  FunctionNumber  The PCI controller’s current PCI function number.

  @retval  EFI_SUCCESS            The PCI controller location was returned.
  @retval  EFI_INVALID_PARAMETER  At least one out of the four output parameters is
                                  a NULL pointer.
**/
EFI_STATUS
PciIoGetLocation (
  IN   EFI_PCI_IO_PROTOCOL  *This,
  OUT  UINTN                *SegmentNumber,
  OUT  UINTN                *BusNumber,
  OUT  UINTN                *DeviceNumber,
  OUT  UINTN                *FunctionNumber
  )
{
  EFI_PCI_IO_PRIVATE_DATA *Private = EFI_PCI_IO_PRIVATE_DATA_FROM_THIS (This);

  if ((SegmentNumber == NULL) || (BusNumber      == NULL) ||
      (DeviceNumber  == NULL) || (FunctionNumber == NULL)    ) {
    return EFI_INVALID_PARAMETER;
  }

  *SegmentNumber  = Private->Segment;
  *BusNumber      = 0xff;
  *DeviceNumber   = 0;
  *FunctionNumber = 0;

  return EFI_SUCCESS;
}

/**
  Performs an operation on the attributes that this PCI controller supports.

  The operations include getting the set of supported attributes, retrieving
  the current attributes, setting the current attributes, enabling attributes,
  and disabling attributes.

  @param[in]   This        A pointer to the EFI_PCI_IO_PROTOCOL instance.
  @param[in]   Operation   The operation to perform on the attributes for this
                           PCI controller.
  @param[in]   Attributes  The mask of attributes that are used for Set,
                           Enable and Disable operations.
  @param[out]  Result      A pointer to the result mask of attributes that are
                           returned for the Get and Supported operations. This
                           is an optional parameter that may be NULL for the
                           Set, Enable, and Disable operations.

  @retval  EFI_SUCCESS            The operation on the PCI controller's
                                  attributes was completed. If the operation
                                  was Get or Supported, then the attribute mask
                                  is returned in Result.
  @retval  EFI_INVALID_PARAMETER  Operation is greater than or equal to
                                  EfiPciIoAttributeOperationMaximum.
  @retval  EFI_INVALID_PARAMETER  Operation is Get and Result is NULL.
  @retval  EFI_INVALID_PARAMETER  Operation is Supported and Result is NULL.

**/
EFI_STATUS
PciIoAttributes (
  IN EFI_PCI_IO_PROTOCOL                       *This,
  IN  EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION  Operation,
  IN  UINT64                                   Attributes,
  OUT UINT64                                   *Result OPTIONAL
  )
{
  switch (Operation) {
  case EfiPciIoAttributeOperationGet:
  case EfiPciIoAttributeOperationSupported:
    if (Result == NULL) {
      return EFI_INVALID_PARAMETER;
    }
    //
    // We are not a real PCI device so just say things we kind of do
    //
    *Result = EFI_PCI_DEVICE_ENABLE;
    break;

  case EfiPciIoAttributeOperationSet:
  case EfiPciIoAttributeOperationEnable:
  case EfiPciIoAttributeOperationDisable:
    if (Attributes & (~EFI_PCI_DEVICE_ENABLE)) {
      return EFI_UNSUPPORTED;
    }
    // Since we are not a real PCI device no enable/set or disable operations exist.
    return EFI_SUCCESS;

  default:
    return EFI_INVALID_PARAMETER;
  };
  return EFI_SUCCESS;
}

EFI_STATUS
PciIoGetBarAttributes (
  IN EFI_PCI_IO_PROTOCOL             *This,
  IN  UINT8                          BarIndex,
  OUT UINT64                         *Supports, OPTIONAL
  OUT VOID                           **Resources OPTIONAL
  )
{
  if (Supports == NULL && Resources == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_UNSUPPORTED;
}

EFI_STATUS
PciIoSetBarAttributes (
  IN EFI_PCI_IO_PROTOCOL              *This,
  IN     UINT64                       Attributes,
  IN     UINT8                        BarIndex,
  IN OUT UINT64                       *Offset,
  IN OUT UINT64                       *Length
  )
{
  ASSERT (FALSE);
  return EFI_UNSUPPORTED;
}

EFI_PCI_IO_PROTOCOL PciIoTemplate =
{
  PciIoPollMem,
  PciIoPollIo,
  { PciIoMemRead, PciIoMemWrite },
  { PciIoIoRead,  PciIoIoWrite },
  { PciIoPciRead, PciIoPciWrite },
  PciIoCopyMem,
  PciIoMap,
  PciIoUnmap,
  PciIoAllocateBuffer,
  PciIoFreeBuffer,
  PciIoFlush,
  PciIoGetLocation,
  PciIoAttributes,
  PciIoGetBarAttributes,
  PciIoSetBarAttributes,
  0,
  0
};

EFI_STATUS
PciInstallDevice (
  IN UINTN            DeviceId,
  IN PHYSICAL_ADDRESS MemoryStart,
  IN UINT64           MemorySize,
  IN UINTN            ClassCode1,
  IN UINTN            ClassCode2,
  IN UINTN            ClassCode3
  )
{
  EFI_STATUS              Status;
  EFI_HANDLE              Handle;
  EFI_PCI_IO_PRIVATE_DATA *Private;

  // Create a private structure
  Private = AllocatePool (sizeof (EFI_PCI_IO_PRIVATE_DATA));
  if (Private == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((EFI_D_ERROR, "Failed to allocate memory for EFI_PCI_IO_PRIVATE_DATA\n"));
    return Status;
  }

  Private->Signature              = EFI_PCI_IO_PRIVATE_DATA_SIGNATURE;  // Fill in signature
  Private->RootBridge.Signature   = PCI_ROOT_BRIDGE_SIGNATURE;          // Fake Root Bridge structure needs a signature too
  Private->RootBridge.MemoryStart = MemoryStart; // Get the controller register base
  Private->Segment                = 0;                                  // Default to segment zero

  // Calculate the total size of the controller.
  Private->RootBridge.MemorySize = MemorySize;

  // HBA reset
  if (PCI_CLASS_MASS_STORAGE_SATADPA == ClassCode2) {
    MmioWrite32 ((Private->RootBridge.MemoryStart + HBA_GHC), HBA_RESET);
  }

  // Create fake PCI config space
  Private->ConfigSpace = AllocateZeroPool (sizeof (PCI_TYPE00));
  if (Private->ConfigSpace == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((EFI_D_ERROR, "Failed to allocate memory for PCI_TYPE00\n"));
    FreePool (Private);
    return Status;
  }

  //
  // Configure PCI config space
  //
  Private->ConfigSpace->Hdr.VendorId = 0xFFFF; // Invalid vendor Id as it is not an actual device.
  Private->ConfigSpace->Hdr.DeviceId = 0x0000; // Not relevant as the vendor id is not valid.
  Private->ConfigSpace->Hdr.ClassCode[0] = ClassCode1;
  Private->ConfigSpace->Hdr.ClassCode[1] = ClassCode2;
  Private->ConfigSpace->Hdr.ClassCode[2] = ClassCode3;
  Private->ConfigSpace->Device.Bar[0] = Private->RootBridge.MemoryStart;

  Handle = NULL;

  // Unique device path.
  CopyMem (&Private->DevicePath, &PciIoDevicePathTemplate, sizeof (PciIoDevicePathTemplate));
  Private->DevicePath.AcpiDevicePath.UID = 0;
  Private->DevicePath.PciDevicePath.Device = DeviceId;

  // Copy protocol structure
  CopyMem (&Private->PciIoProtocol, &PciIoTemplate, sizeof (PciIoTemplate));

  Status = gBS->InstallMultipleProtocolInterfaces (&Handle,
                                                  &gEfiPciIoProtocolGuid,       &Private->PciIoProtocol,
                                                  &gEfiDevicePathProtocolGuid,  &Private->DevicePath,
                                                  NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "PciInstallDevice InstallMultipleProtocolInterfaces () failed.\n"));
    FreePool(Private->ConfigSpace);
    FreePool(Private);
  }

  return Status;
}

EFI_STATUS
PciEmulationEntryPoint (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS              Status;
  BOOLEAN                 SuccessFlag;
  UINT8                   DeviceId;
  SuccessFlag = FALSE;

  for (DeviceId = 0; DeviceId < NUM_OF_USB_CONTROLLERS; DeviceId++) {
    Status = InitializeUsbController (UsbRegAdd[DeviceId]);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "USB HC initialization Failed for %d (0x%x)\n",
                            UsbRegAdd[DeviceId], Status));
      continue;
    }

    Status = PciInstallDevice (DeviceId, UsbRegAdd[DeviceId], SIZE_64KB,
                                     PCI_IF_XHCI, PCI_CLASS_SERIAL_USB,
                                     PCI_CLASS_SERIAL);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "PciEmulation: failed to install USB %d device\n", DeviceId));
    } else {
      SuccessFlag = TRUE;
    }
  }

  Status = PciInstallDevice (DeviceId, SATA1_REG_ADDR, SIZE_64KB,
                                   PCI_IF_MASS_STORAGE_SATA,
                                   PCI_CLASS_MASS_STORAGE_SATADPA,
                                   PCI_CLASS_MASS_STORAGE);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "PciEmulation: failed to install SATA device.\n"));
  } else {
    SuccessFlag = TRUE;
  }

  if (SuccessFlag) {
    return EFI_SUCCESS;
  } else {
    return Status;
  }
}

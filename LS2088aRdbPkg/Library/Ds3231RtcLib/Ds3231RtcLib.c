/** @Ds3231RtcLib.c
  Implement EFI RealTimeClock with runtime services via RTC Lib for DS3231 RTC.

  Based on RTC implementation available in
  EmbeddedPkg/Library/TemplateRealTimeClockLib/RealTimeClockLib.c

  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
  Copyright (c) 2015, Freescale Semiconductor, Inc. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/RealTimeClockLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiRuntimeLib.h>
#include <I2c.h>
#include <Protocol/RealTimeClock.h>

#define Bin(Bcd) ((Bcd) & 0x0f) + ((Bcd) >> 4) * 10
#define Bcd(Bin) (((Bin / 10) << 4) | (Bin % 10))

#define DS3231_I2C_ADDR			0x68
#define MUX_I2C_ADDR			0x75
#define DEFAULT_I2C_CHANNEL		0x8
#define RTC_I2C_CHANNEL			0x9
#define I2C_CHANNEL_REG			0x9

/*
 * RTC register addresses
 */
#define DS3231_SEC_REG_ADDR	0x00
#define DS3231_MIN_REG_ADDR	0x01
#define DS3231_HR_REG_ADDR	0x02
#define DS3231_DAY_REG_ADDR	0x03
#define DS3231_DATE_REG_ADDR	0x04
#define DS3231_MON_REG_ADDR	0x05
#define DS3231_YR_REG_ADDR	0x06
#define DS3231_CTL_REG_ADDR     0x0e
#define DS3231_STAT_REG_ADDR    0x0f

#define DS3231_SEC_BIT_CH       0x80    /* Clock Halt (in Register 0)   */

/*
 * RTC control register bits
 */
#define RTC_CTL_BIT_RS1         0x8     /* Rate select 1                */
#define RTC_CTL_BIT_RS2         0x10    /* Rate select 2                */

/*
 * RTC status register bits
 */
#define RTC_STAT_BIT_OSF        0x80    /* Oscillator stop flag         */
#define RTC_STAT_BIT_BB32KHZ    0x40    /* Battery backed 32KHz Output  */
#define RTC_STAT_BIT_EN32KHZ    0x8     /* Enable 32KHz Output  */

#define DS3231_CTL_BIT_RS0	0x01	/* Rate select 0                */
#define DS3231_CTL_BIT_RS1	0x02	/* Rate select 1                */
#define DS3231_CTL_BIT_SQWE	0x10	/* Square Wave Enable           */
#define DS3231_CTL_BIT_OUT	0x80	/* Output Control               */

STATIC CONST CHAR16           mTimeZoneVariableName[] = L"Ds3231RtcTimeZone";
STATIC CONST CHAR16           mDaylightVariableName[] = L"Ds3231RtcDaylight";
STATIC EFI_EVENT              mRtcVirtualAddrChangeEvent;
STATIC UINTN                  mI2c1BaseAddress;

UINT8 RtcRead(
		UINT8 RtcRegAddr
)
{
	INT32 Status;
	UINT8 Val = 0;
	Status = I2cDataRead((VOID*)mI2c1BaseAddress, DS3231_I2C_ADDR, RtcRegAddr, 0x1, &Val, sizeof(Val));
	if(EFI_ERROR(Status))
		DEBUG((EFI_D_ERROR, "RTC read error at Addr:0x%x\n", RtcRegAddr));
	return Val;
}

VOID RtcWrite(
		UINT8 RtcRegAddr,
		UINT8 Val)
{
	INT32 Status;
	Status = I2cDataWrite((VOID*)mI2c1BaseAddress, DS3231_I2C_ADDR, RtcRegAddr, 0x1, &Val, sizeof(Val));
	if(EFI_ERROR(Status))
		DEBUG((EFI_D_ERROR, "RTC write error at Addr:0x%x\n", RtcRegAddr));

}

BOOLEAN
IsLeapYear (
  IN EFI_TIME   *Time
  )
{
  if (Time->Year % 4 == 0) {
    if (Time->Year % 100 == 0) {
      if (Time->Year % 400 == 0) {
        return TRUE;
      } else {
        return FALSE;
      }
    } else {
      return TRUE;
    }
  } else {
    return FALSE;
  }
}

BOOLEAN
DayValid (
  IN  EFI_TIME  *Time
  )
{
  STATIC CONST INTN DayOfMonth[12] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

  if (Time->Day < 1 ||
      Time->Day > DayOfMonth[Time->Month - 1] ||
      (Time->Month == 2 && (!IsLeapYear (Time) && Time->Day > 28))
     ) {
    return FALSE;
  }

  return TRUE;
}

/**
  Returns the current time and date information, and the time-keeping capabilities
  of the hardware platform.

  @param  Time                  A pointer to storage to receive a snapshot of the current time.
  @param  Capabilities          An optional pointer to a buffer to receive the real time clock
                                device's capabilities.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval EFI_INVALID_PARAMETER Time is NULL.
  @retval EFI_DEVICE_ERROR      The time could not be retrieved due to hardware error.

**/

EFI_STATUS
EFIAPI
LibGetTime (
  OUT EFI_TIME                *Time,
  OUT  EFI_TIME_CAPABILITIES  *Capabilities
  )
{
	EFI_STATUS Status = EFI_SUCCESS;
	CHAR16 Control, Stat, Second, Minute, Hour, Day, Date, Month, Year;
	INT32 Status1;
	UINT8 Val = RTC_I2C_CHANNEL;
	UINTN Size;
	INT16 TimeZone;
	UINT8 Daylight;

	/* Switching to the channel from where
	   RTC is accesible
	*/  
        Status1 = I2cDataWrite((VOID*)mI2c1BaseAddress, MUX_I2C_ADDR, I2C_CHANNEL_REG, 0x1, &Val, sizeof(Val));
        if(EFI_ERROR(Status1))
                DEBUG((EFI_D_ERROR, "RTC write error at Addr:0x%x\n", MUX_I2C_ADDR));
	if (Time == NULL)
		return EFI_INVALID_PARAMETER;

	Control = RtcRead (DS3231_CTL_REG_ADDR);
	Stat = RtcRead (DS3231_STAT_REG_ADDR);

	Second = RtcRead (DS3231_SEC_REG_ADDR);
	Minute = RtcRead (DS3231_MIN_REG_ADDR);
	Hour = RtcRead (DS3231_HR_REG_ADDR);
	Day = RtcRead (DS3231_DAY_REG_ADDR);
	Date = RtcRead (DS3231_DATE_REG_ADDR);
	Month = RtcRead (DS3231_MON_REG_ADDR);
	Year = RtcRead (DS3231_YR_REG_ADDR);

	DEBUG((EFI_D_VERBOSE,"\nGet RTC year: %02x Month: %02x Date: %02x Day: %02x "
                "hr: %02x min: %02x sec: %02x control: %02x status: %02x\n",
                Year, Month, Date, Day, Hour, Minute, Second, Control, Stat));

	if (Stat & DS3231_SEC_BIT_CH) {
		DEBUG((EFI_D_ERROR, "## Warning: RTC oscillator has stopped\n"));
		/* clear the CH flag */
		RtcWrite (DS3231_STAT_REG_ADDR,
			   RtcRead (DS3231_STAT_REG_ADDR) & ~DS3231_SEC_BIT_CH);
		Status = EFI_DEVICE_ERROR;
	}

	Time->Nanosecond = 0;
	Time->Second  = Bin (Second & 0x7F);
	Time->Minute  = Bin (Minute & 0x7F);
	Time->Hour = Bin (Hour & 0x3F);
	Time->Day = Bin (Date & 0x3F);
	Time->Month  = Bin (Month & 0x1F);
	Time->Year = Bin (Year) + ( Bin (Year) >= 98 ? 1900 : 2000);

	/* Switching I2C to default channel
	 */
	Val = DEFAULT_I2C_CHANNEL;
        Status1 = I2cDataWrite((VOID*)mI2c1BaseAddress, MUX_I2C_ADDR, I2C_CHANNEL_REG, 0x1, &Val, sizeof(Val));
        if(EFI_ERROR(Status1))
                DEBUG((EFI_D_ERROR, "RTC write error at Addr:0x%x\n", MUX_I2C_ADDR));

	// Get the current time zone information from non-volatile storage
	Size = sizeof (TimeZone);
	Status = EfiGetVariable (
	                (CHAR16 *)mTimeZoneVariableName,
	                &gEfiCallerIdGuid,
	                NULL,
	                &Size,
	                (VOID *)&TimeZone
	                );

	if (EFI_ERROR (Status)) {
	  ASSERT(Status != EFI_INVALID_PARAMETER);
	  ASSERT(Status != EFI_BUFFER_TOO_SMALL);

	  if (Status != EFI_NOT_FOUND)
	    goto EXIT;

	  // The time zone variable does not exist in non-volatile storage, so create it.
	  Time->TimeZone = EFI_UNSPECIFIED_TIMEZONE;
	  // Store it
	  Status = EfiSetVariable (
	                  (CHAR16 *)mTimeZoneVariableName,
	                  &gEfiCallerIdGuid,
	                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
	                  Size,
	                  (VOID *)&(Time->TimeZone)
	                  );
	  if (EFI_ERROR (Status)) {
	    DEBUG ((
	      EFI_D_ERROR,
	      "LibGetTime: Failed to save %s variable to non-volatile storage, Status = %r\n",
	      mTimeZoneVariableName,
	      Status
	      ));
	    goto EXIT;
	  }
	} else {
	  // Got the time zone
	  Time->TimeZone = TimeZone;

	  // Check TimeZone bounds:   -1440 to 1440 or 2047
	  if (((Time->TimeZone < -1440) || (Time->TimeZone > 1440))
	      && (Time->TimeZone != EFI_UNSPECIFIED_TIMEZONE)) {
	    Time->TimeZone = EFI_UNSPECIFIED_TIMEZONE;
	  }
	}

	// Get the current daylight information from non-volatile storage
	Size = sizeof (Daylight);
	Status = EfiGetVariable (
	                (CHAR16 *)mDaylightVariableName,
	                &gEfiCallerIdGuid,
	                NULL,
	                &Size,
	                (VOID *)&Daylight
	                );

	if (EFI_ERROR (Status)) {
	  ASSERT(Status != EFI_INVALID_PARAMETER);
	  ASSERT(Status != EFI_BUFFER_TOO_SMALL);

	  if (Status != EFI_NOT_FOUND)
	    goto EXIT;

	  // The daylight variable does not exist in non-volatile storage, so create it.
	  Time->Daylight = 0;
	  // Store it
	  Status = EfiSetVariable (
	                  (CHAR16 *)mDaylightVariableName,
	                  &gEfiCallerIdGuid,
	                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
	                  Size,
	                  (VOID *)&(Time->Daylight)
	                  );
	  if (EFI_ERROR (Status)) {
	    DEBUG ((
	      EFI_D_ERROR,
	      "LibGetTime: Failed to save %s variable to non-volatile storage, Status = %r\n",
	      mDaylightVariableName,
	      Status
	      ));
	    goto EXIT;
	  }
	} else {
	  // Got the daylight information
	  Time->Daylight = Daylight;
	}

	EXIT:
	  return Status;
}


/**
  Sets the current local time and date information.

  @param  Time                  A pointer to the current time.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval EFI_INVALID_PARAMETER A time field is out of range.
  @retval EFI_DEVICE_ERROR      The time could not be set due due to hardware error.

**/
EFI_STATUS
EFIAPI
LibSetTime (
  IN EFI_TIME                *Time
  )
{
	EFI_STATUS  Status;
	UINT8 Val = RTC_I2C_CHANNEL;

	/* Switching to the channel from where
	   RTC is accesible
	*/  
        Status = I2cDataWrite((VOID*)mI2c1BaseAddress, MUX_I2C_ADDR, I2C_CHANNEL_REG, 0x1, &Val, sizeof(Val));
        if(EFI_ERROR(Status))
                DEBUG((EFI_D_ERROR, "RTC write error at Addr:0x%x\n", MUX_I2C_ADDR));

	// Check the input parameters are within the range specified by UEFI
	if ((Time->Year   < 1900) ||
	     (Time->Year   > 9999) ||
	     (Time->Month  < 1   ) ||
	     (Time->Month  > 12  ) ||
	     (!DayValid (Time)    ) ||
	     (Time->Hour   > 23  ) ||
	     (Time->Minute > 59  ) ||
	     (Time->Second > 59  ) ||
	     (Time->Nanosecond > 999999999) ||
	     (!((Time->TimeZone == EFI_UNSPECIFIED_TIMEZONE) || ((Time->TimeZone >= -1440) && (Time->TimeZone <= 1440)))) ||
	     (Time->Daylight & (~(EFI_TIME_ADJUST_DAYLIGHT | EFI_TIME_IN_DAYLIGHT)))
	  ) {
	  Status = EFI_INVALID_PARAMETER;
	  goto EXIT;
	}

	if (Time->Year < 1998 || Time->Year > 2097)
	  DEBUG((EFI_D_ERROR, "WARNING: Year should be between 1998 and 2097!\n"));

	RtcWrite (DS3231_YR_REG_ADDR, Bcd (Time->Year % 100));
	RtcWrite (DS3231_MON_REG_ADDR, Bcd (Time->Month));
	RtcWrite (DS3231_DATE_REG_ADDR, Bcd (Time->Day));
	RtcWrite (DS3231_HR_REG_ADDR, Bcd (Time->Hour));
	RtcWrite (DS3231_MIN_REG_ADDR, Bcd (Time->Minute));
	RtcWrite (DS3231_SEC_REG_ADDR, Bcd (Time->Second));

	/* Switching I2C to default channel
	 */
	Val = DEFAULT_I2C_CHANNEL;
        Status = I2cDataWrite((VOID*)mI2c1BaseAddress, MUX_I2C_ADDR, I2C_CHANNEL_REG, 0x1, &Val, sizeof(Val));
        if(EFI_ERROR(Status))
                DEBUG((EFI_D_ERROR, "RTC write error at Addr:0x%x\n", MUX_I2C_ADDR));

	// The accesses to Variable Services can be very slow, because we may be writing to Flash.
	// Do this after having set the RTC.

	// Save the current time zone information into non-volatile storage
	Status = EfiSetVariable (
	                (CHAR16 *)mTimeZoneVariableName,
	                &gEfiCallerIdGuid,
	                EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
	                sizeof (Time->TimeZone),
	                (VOID *)&(Time->TimeZone)
	                );
	if (EFI_ERROR (Status)) {
	    DEBUG ((
	      EFI_D_ERROR,
	      "LibSetTime: Failed to save %s variable to non-volatile storage, Status = %r\n",
	      mTimeZoneVariableName,
	      Status
	      ));
	  goto EXIT;
	}
	// Save the current daylight information into non-volatile storage
	Status = EfiSetVariable (
	                (CHAR16 *)mDaylightVariableName,
	                &gEfiCallerIdGuid,
	                EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
	                sizeof(Time->Daylight),
	                (VOID *)&(Time->Daylight)
	                );
	if (EFI_ERROR (Status)) {
	  DEBUG ((
	    EFI_D_ERROR,
	    "LibSetTime: Failed to save %s variable to non-volatile storage, Status = %r\n",
	    mDaylightVariableName,
	    Status
	    ));
	  goto EXIT;
	}

	EXIT:
	  return Status;
}


/**
  Returns the current wakeup alarm clock setting.

  @param  Enabled               Indicates if the alarm is currently enabled or disabled.
  @param  Pending               Indicates if the alarm signal is pending and requires acknowledgement.
  @param  Time                  The current alarm setting.

  @retval EFI_SUCCESS           The alarm settings were returned.
  @retval EFI_INVALID_PARAMETER Any parameter is NULL.
  @retval EFI_DEVICE_ERROR      The wakeup time could not be retrieved due to a hardware error.

**/
EFI_STATUS
EFIAPI
LibGetWakeupTime (
  OUT BOOLEAN     *Enabled,
  OUT BOOLEAN     *Pending,
  OUT EFI_TIME    *Time
  )
{
  // Not a required feature
  return EFI_UNSUPPORTED;
}


/**
  Sets the system wakeup alarm clock time.

  @param  Enabled               Enable or disable the wakeup alarm.
  @param  Time                  If Enable is TRUE, the time to set the wakeup alarm for.

  @retval EFI_SUCCESS           If Enable is TRUE, then the wakeup alarm was enabled. If
                                Enable is FALSE, then the wakeup alarm was disabled.
  @retval EFI_INVALID_PARAMETER A time field is out of range.
  @retval EFI_DEVICE_ERROR      The wakeup time could not be set due to a hardware error.
  @retval EFI_UNSUPPORTED       A wakeup timer is not supported on this platform.

**/
EFI_STATUS
EFIAPI
LibSetWakeupTime (
  IN BOOLEAN      Enabled,
  OUT EFI_TIME    *Time
  )
{
  // Not a required feature
  return EFI_UNSUPPORTED;
}

/**
  Fixup internal data so that EFI can be call in virtual mode.
  Call the passed in Child Notify event and convert any pointers in
  lib to virtual mode.

  @param[in]    Event   The Event that is being processed
  @param[in]    Context Event Context
**/
VOID
EFIAPI
LibRtcVirtualNotifyEvent (
  IN EFI_EVENT        Event,
  IN VOID             *Context
  )
{
  //
  // Only needed if you are going to support the OS calling RTC functions in virtual mode.
  // You will need to call EfiConvertPointer (). To convert any stored physical addresses
  // to virtual address. After the OS transistions to calling in virtual mode, all future
  // runtime calls will be made in virtual mode.
  //
  EfiConvertPointer (0x0, (VOID**)&mI2c1BaseAddress);
  return;
}

/**
  This is the declaration of an EFI image entry point. This can be the entry point to an application
  written to this specification, an EFI boot service driver, or an EFI runtime driver.

  @param  ImageHandle           Handle that identifies the loaded image.
  @param  SystemTable           System Table for this image.

  @retval EFI_SUCCESS           The operation completed successfully.

**/
EFI_STATUS
EFIAPI
LibRtcInitialize (
  IN EFI_HANDLE                            ImageHandle,
  IN EFI_SYSTEM_TABLE                      *SystemTable
  )
{
  //
  // Do some initialization if reqruied to turn on the RTC
  //
  EFI_STATUS    Status;
  EFI_HANDLE    Handle;

  // Initialize RTC Base Address
  mI2c1BaseAddress = (UINTN)I2C1_BASE_ADDRESS;

  // Declare the controller as EFI_MEMORY_RUNTIME
  Status = gDS->AddMemorySpace (
                  EfiGcdMemoryTypeMemoryMappedIo,
                  mI2c1BaseAddress, I2C_SIZE,
                  EFI_MEMORY_UC | EFI_MEMORY_RUNTIME
                  );
  if (EFI_ERROR (Status)) {
    return Status;
}

  Status = gDS->SetMemorySpaceAttributes (mI2c1BaseAddress, I2C_SIZE, EFI_MEMORY_UC | EFI_MEMORY_RUNTIME);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Install the protocol
  Handle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gEfiRealTimeClockArchProtocolGuid,  NULL,
                  NULL
                 );
  ASSERT_EFI_ERROR (Status);

  //
  // Register for the virtual address change event
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  LibRtcVirtualNotifyEvent,
                  NULL,
                  &gEfiEventVirtualAddressChangeGuid,
                  &mRtcVirtualAddrChangeEvent
                  );
  ASSERT_EFI_ERROR (Status);

  return Status;
}
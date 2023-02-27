// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "SUDeviceUID.h"

@implementation SUDeviceUID

+ (NSString*)uniqueIdentifierString {
  return [[self class] hostHardwareUUIDNumber];
}

+ (NSString*)hostHardwareUUIDNumber {
  io_service_t service = IOServiceGetMatchingService(
      kIOMasterPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
  CFStringRef hardwareUUIDAsCFString = NULL;

  if (service) {
    hardwareUUIDAsCFString = IORegistryEntryCreateCFProperty(
        service, CFSTR(kIOPlatformUUIDKey), kCFAllocatorDefault, 0);
    IOObjectRelease(service);
  }

  NSString* hardwareUUIDAsNSString = nil;
  if (hardwareUUIDAsCFString != NULL) {
    hardwareUUIDAsNSString = (__bridge NSString*)hardwareUUIDAsCFString;
    CFRelease(hardwareUUIDAsCFString);
  }

  return hardwareUUIDAsNSString;
}

@end

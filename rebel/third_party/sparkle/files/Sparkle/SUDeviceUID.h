// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

@interface SUDeviceUID : NSObject

/*
 String representation of hardware UUID of the system.
 Format as described in NSUUID class: printf(3) format
 "%08X-%04X-%04X-%04X-%012X"
 */
+ (NSString*)uniqueIdentifierString;

@end

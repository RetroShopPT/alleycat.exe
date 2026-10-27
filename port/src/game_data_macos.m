/* First-run import stays outside the signed app, in the current user's Library. */
#import <Cocoa/Cocoa.h>
#include "game_data.h"
#include <stdlib.h>

int game_data_prepare(void)
{
    @autoreleasepool {
        NSFileManager *files = NSFileManager.defaultManager;
        NSURL *support = [files URLForDirectory:NSApplicationSupportDirectory
                                      inDomain:NSUserDomainMask appropriateForURL:nil
                                        create:YES error:nil];
        NSURL *folder = [support URLByAppendingPathComponent:@"RetroShopPT/Alley Cat" isDirectory:YES];
        const char *override = getenv("ALLEYCAT_DATA_DIR");
        if (override && *override)
            folder = [NSURL fileURLWithPath:[NSString stringWithUTF8String:override] isDirectory:YES];
        NSURL *cached = [folder URLByAppendingPathComponent:@"CAT.EXE"];
        char error[256];
        if (cached && game_data_load(cached.fileSystemRepresentation, error, sizeof error))
            return 1;
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp activateIgnoringOtherApps:YES];
        while (1) {
            NSOpenPanel *panel = [NSOpenPanel openPanel];
            panel.title = @"Alley Cat — Select CAT.EXE";
            panel.message = @"Select your original DOS CAT.EXE to play. It will be saved for your user account, so you only need to do this once.";
            panel.prompt = @"Import Game";
            panel.canChooseFiles = YES;
            panel.canChooseDirectories = NO;
            panel.allowsMultipleSelection = NO;
            if ([panel runModal] != NSModalResponseOK)
                return 0;
            if (!game_data_load(panel.URL.fileSystemRepresentation, error, sizeof error)) {
                NSAlert *alert = [[NSAlert alloc] init];
                alert.messageText = @"Please choose a supported copy of Alley Cat";
                alert.informativeText = [NSString stringWithUTF8String:error];
                [alert addButtonWithTitle:@"Choose Another File"];
                [alert addButtonWithTitle:@"Quit"];
                if ([alert runModal] != NSAlertFirstButtonReturn)
                    return 0;
                continue;
            }
            NSError *saveError = nil;
            BOOL saved = folder && [files createDirectoryAtURL:folder withIntermediateDirectories:YES
                                                    attributes:nil error:&saveError];
            NSData *data = saved ? [NSData dataWithContentsOfURL:panel.URL] : nil;
            saved = data && [data writeToURL:cached options:NSDataWritingAtomic error:&saveError];
            if (!saved) {
                NSAlert *alert = [[NSAlert alloc] init];
                alert.messageText = @"Your game could not be saved";
                alert.informativeText = @"You can play this session, but will need to select CAT.EXE again next time.";
                [alert runModal];
            }
            return 1;
        }
    }
}

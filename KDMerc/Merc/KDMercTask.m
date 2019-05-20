//
//  KDMercTask.mm
//  KDMerc
//
//  Created by hour on 2018/1/4.
//

#import "KDMercTask.h"

@implementation KDMercTask

- (id)init {
    if (self = [super init]) {
        self.channel_select = ChannelType_All;
        self.sendOnly = YES;
        self.cgi = @"";
        self.host = @"";
        
        self.networkStatusSensitive = NO;
        self.channelStragery = ChannelNormalStrategy;
        self.priority = 3;  // normal
        self.retryCount = 10;   // 
        self.totalTimeout = 10 * 60 * 1000; // ms
    }
    
    return self;
}

- (void)dealloc {
    //
}
@end

//
//  KDMarsService.m
//  KDMerc
//
//  Created by hour on 2018/1/4.
//

#import "KDMercService.h"


#import <SystemConfiguration/SCNetworkReachability.h>
#import <string>

#import "app_callback.h"
#import "stn_callback.h"
#import "stnproto_logic.h"
#import <mars/baseevent/base_logic.h>

#import "KDMercTask.h"

//#import <mars/app/app_logic.h>
//#import <mars/xlog/xlogger.h>
//#import <mars/xlog/xloggerbase.h>
//#import <mars/xlog/appender.h>

#include <mach/mach.h>
#include <mach/mach_time.h>

// 注意此处应该跟longlink_packer.cc中的 MERC_CMDID_TYPE 保持一致
typedef NS_ENUM(int32_t, KDMercCmdidType) {
    kMercCmdidTypeAsePassword = 1,  // 上行确认通讯加密密码（成功ACK返回，错误直接关闭该长连接）
    kMercCmdidTypeSendData = 2, // 上行业务数据（是否需要ACK由业务决定）
    kMercCmdidTypePushData = 3, // 服务端下行推送消息（是否需要ACK由业务决定）
    //    kMercCmdidTypeHB = 6    // 心跳
};

NSErrorDomain const KDMercErrorDomain = @"KDMercErrorDomain";

using namespace mars::stn;

@interface KDMercService () {
    //
    mach_timebase_info_data_t _clock_timebase;
}

@end

@implementation KDMercService

- (instancetype)init {
    self = [super init];
    if (self) {
        mach_timebase_info(&_clock_timebase);

        _connStatus = KDMercConnUnknow;
    }
    return self;
}

+ (KDMercService *)sharedInstance {
    static KDMercService *sharedSingleton = nil;
    
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        if (sharedSingleton == nil) {
            sharedSingleton = [[KDMercService alloc] init];
        }
    });
    
    return sharedSingleton;
}

- (void)createMerc {
    mars::stn::SetCallback(mars::stn::StnCallBack::Instance());
    mars::app::SetCallback(mars::app::AppCallBack::Instance());
    
    mars::baseevent::OnCreate();
}

- (void)destoryMerc {
    mars::baseevent::OnDestroy();
}

- (void)openSessionWithPubKey:(NSString *)pubKey
                    openToken:(NSString *)token
                    userAgent:(NSString *)ua
                     clientId:(NSString *)clientId {
    mars::stn::SetParameter(MERC_PARAMETER_CLIENT_ID, std::string(clientId.UTF8String));
    mars::stn::SetParameter(MERC_PARAMETER_OPEN_TOKEN, std::string(token.UTF8String));
    mars::stn::SetParameter(MERC_PARAMETER_PUBLIC_KEY, std::string(pubKey.UTF8String));
    mars::stn::SetParameter(MERC_PARAMETER_USER_AGENT, std::string(ua.UTF8String));
    mars::stn::OpenSession();
    
    [self updateConnStatus:KDMercConnecting errCode:0 errMsg:nil];
}
- (void)closeSession {
    mars::stn::CloseSession();
}

- (void)report_onNetworkChange {
    mars::baseevent::OnNetworkChange();
}

- (void)report_onForeground:(BOOL)isForeground {
    mars::baseevent::OnForeground(isForeground);
}

- (void)OnExceptionCrash {
    mars::baseevent::OnExceptionCrash();
}

#pragma mark -
#pragma mark setter
- (void)setClientVersion:(UInt32)clientVersion {
    mars::stn::SetClientVersion(clientVersion);
}

- (void)setShortLinkDebugIP:(NSString *)IP port:(const unsigned short)port {
    std::string ipAddress([IP UTF8String]);
    mars::stn::SetShortlinkSvrAddr(port, ipAddress);
}

- (void)setShortLinkPort:(const unsigned short)port {
    mars::stn::SetShortlinkSvrAddr(port, "");
}

- (void)setLongLinkAddress:(NSString *)string port:(const unsigned short)port debugIP:(NSString *)IP {
    std::string ipAddress([string UTF8String]);
    std::string debugIP([IP UTF8String]);
    std::vector<uint16_t> ports;
    ports.push_back(port);
    mars::stn::SetLonglinkSvrAddr(ipAddress,ports,debugIP);
}

- (void)setLongLinkAddress:(NSString *)string port:(const unsigned short)port {
    std::string ipAddress([string UTF8String]);
    std::vector<uint16_t> ports;
    ports.push_back(port);
    mars::stn::SetLonglinkSvrAddr(ipAddress, ports, "");
}

- (void)makesureLongLinkConnect {
    mars::stn::MakesureLonglinkConnected();
}

- (NSString *)longLinkIp {
    std::string str = mars::stn::GetCurrentLonglinkIp();
    return [NSString stringWithUTF8String:str.c_str()];
}

- (int64_t)getSessionId {
    return mars::stn::GetSessionId();
}

#pragma mark - Task
- (uint32_t)startTask:(KDMercTask *)task {
    task.startTime = [self absoluteTime];

    Task ctask;
    ctask.cmdid = kMercCmdidTypeSendData;
    ctask.user_context = (__bridge void *)task;
    
    ctask.channel_select = task.channel_select;
    ctask.cgi = std::string(task.cgi.UTF8String);
    ctask.shortlink_host_list.push_back(std::string(task.host.UTF8String));
    ctask.send_only = task.sendOnly;
    
    ctask.network_status_sensitive = task.networkStatusSensitive;
    ctask.channel_strategy = task.channelStragery;
    ctask.priority = task.priority;

    ctask.retry_count = task.retryCount;
    ctask.total_timetout = task.totalTimeout;

    NSString *taskIdKey = [NSString stringWithFormat:@"%d", ctask.taskid];
    [_delegate addCGITask:task forKey:taskIdKey];
    
    mars::stn::StartTask(ctask);
    
    return ctask.taskid;
}

- (void)stopTask:(uint32_t)taskID {
    NSString *taskIdKey = [NSString stringWithFormat:@"%d", taskID];
    [_delegate removeCGITaskForKey:taskIdKey];
    
    mars::stn::StopTask((uint32_t)taskID);
}

- (void)updateConnStatus:(KDMercConnectStatus)status
                 errCode:(NSInteger)errCode
                  errMsg:(NSString *)errMsg {
    _connStatus = status;
    
    NSError *err = [NSError errorWithDomain:KDMercErrorDomain
                                       code:errCode
                                   userInfo:@{NSLocalizedDescriptionKey : errMsg?:@""}];
    [_delegate onConnectStatusChange:status error:err];
    
    //    NSLog(@"err_code:%ld, err_msg: %@", errCode, errMsg);
}

#pragma mark - callback
- (NSArray<NSString *> *)OnNewDns:(NSString *)address {
    if ([_delegate respondsToSelector:@selector(OnNewDns:)]) {
        return [_delegate OnNewDns:address];
    }
    
    return @[];
}

- (void)OnPushWithCmd:(NSInteger)cid data:(NSData *)data {
    NSString *str = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    return [_delegate OnPushWithData:str];
}

- (NSData *)Request2BufferWithTaskID:(uint32_t)tid userContext:(const void *)context {
    KDMercTask *task = (__bridge KDMercTask *)context;
    return [_delegate Request2BufferWithTaskID:tid task:task];
}

- (NSInteger)Buffer2ResponseWithTaskID:(uint32_t)tid ResponseData:(NSData *)data userContext:(const void *)context {
    KDMercTask *task = (__bridge KDMercTask *)context;
    return [_delegate Buffer2ResponseWithTaskID:tid responseData:data task:task];
}

- (NSInteger)OnTaskEndWithTaskID:(uint32_t)tid userContext:(const void *)context errType:(uint32_t)errtype errCode:(uint32_t)errcode {
    KDMercTask *task = (__bridge KDMercTask *)context;
    
    task.errCode = errcode;
    task.errType = errtype;
    task.endTime = [self absoluteTime];
    
    return [_delegate OnTaskEndWithTaskID:tid task:task errType:errtype errCode:errcode];
}

- (void)OnOpenSessionErrorCode:(int)errcode errorMsg:(NSString *)errMsg {
    if (errcode == 0) {
        [self updateConnStatus:KDMercConnected errCode:errcode errMsg:errMsg];
    } else {
        [self updateConnStatus:KDMercConnFailed errCode:errcode errMsg:errMsg];
    }
}

/// tcp的连接状态，biz不需要关心
- (void)OnConnectionStatusChange:(int32_t)status longConnStatus:(int32_t)longConnStatus {
    if (_delegate) {
        [_delegate OnLongLinkStatusChange:status longConnStatus:longConnStatus];
    }
}

#pragma mark - tools
- (NSTimeInterval)machAbsoluteToTimeInterval:(uint64_t)machAbsolute {
    uint64_t nanos = (machAbsolute * _clock_timebase.numer) / _clock_timebase.denom;
    
    return nanos/1.0e9;
}

- (NSTimeInterval)absoluteTime {
    uint64_t machtime = mach_absolute_time();
    return [self machAbsoluteToTimeInterval:machtime];
}
@end


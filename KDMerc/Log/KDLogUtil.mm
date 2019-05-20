//
//  KDLogUtil.m
//  KDMerc
//
//  Created by hour on 2017/12/9.
//

#import "KDLogUtil.h"

#import <mars/xlog/xloggerbase.h>
#import <mars/xlog/xlogger.h>
#import <mars/xlog/appender.h>

#import <cstring>

@interface KDLogUtil (Internal)
+ (TLogLevel)levelWithType:(KDLoggerType)type;
@end

@implementation KDLogUtil

+ (void)setLoggerType:(KDLoggerType)type {
    xlogger_SetLevel([self levelWithType:type]);
}

+ (void)setConsoleLog:(BOOL)enable {
    appender_set_console_log(enable);
}

+ (void)setMaxFileSize:(uint64_t)byteSize {
    appender_set_max_file_size(byteSize);
}

+ (void)flush {
    appender_flush();
}

+(void)flushSync {
    appender_flush_sync();
}

+ (nullable NSArray<NSString *> *)logFilePathFromTimeSpan:(int)timeSpan
                                                   prefix:(nonnull NSString *)prefix {
    if (prefix.length<=0 || timeSpan<0)  return nil;
    
    std::vector<std::string> path_vec;
    bool suc = appender_getfilepath_from_timespan(timeSpan, prefix.UTF8String, path_vec);
    
    if (suc && path_vec.size() > 0) {
        NSMutableArray *mArr = [NSMutableArray array];
        
        for(unsigned long i = 0; i < path_vec.size(); i++) {
//            std::string str = path_vec[i];
//            NSString *tmp = [NSString stringWithUTF8String:path_vec[i].c_str()];
            
            [mArr addObject: [NSString stringWithUTF8String:path_vec[i].c_str()]];
        }
        
        return [NSArray arrayWithArray:mArr];
    }
    
    return nil;
}

+ (void)openLogWithPath:(NSString *)path
                 prefix:(NSString *)prefix
              publicKey:(NSString *)key {
    NSAssert([path isKindOfClass:[NSString class]] && path.length > 0, @"log path can not be empty");
    NSAssert([prefix isKindOfClass:[NSString class]] && prefix.length > 0, @"prefix can not be empty");
    NSAssert([key isKindOfClass:[NSString class]] && key, @"publicKey can not be nil");
    
    appender_open(kAppednerAsync, [path UTF8String], [prefix UTF8String], [key UTF8String]);
}

+ (void)closeLog {
    appender_close();
}

+ (void)log:(KDLoggerType)type
   fileName:(nonnull const char *)fileName
 lineNumber:(int)lineNumber
   funcName:(nonnull const char *)funcName
    message:(nonnull NSString *)message {
    switch (type) {
        case KDLoggerTypeDebug:
        case KDLoggerTypeAll:
            [self writelog:type fileName:fileName lineNumber:lineNumber funcName:funcName message:message];
            break;
        case KDLoggerTypeInfo:
            KDLogInfo(message);
            break;
        case KDLoggerTypeWarn:
            KDLogWarn(message);
            break;
        case KDLoggerTypeError:
        case KDLoggerTypeFatal:
            KDLogError(message);
            break;
        default:
            break;
    }
}

@end

@implementation KDLogUtil (Log)
+ (void)writelog:(KDLoggerType)logType fileName:(const char *)fileName lineNumber:(int)lineNumber funcName:(const char *)funcName message:(NSString *)message {
    XLoggerInfo info;
    info.level = [self levelWithType:logType];
    info.tag = "";
    info.filename = fileName;
    info.func_name = funcName;
    info.line = lineNumber;
    gettimeofday(&info.timeval, NULL);
    info.tid = (uintptr_t)[NSThread currentThread];
    info.maintid = (uintptr_t)[NSThread mainThread];
    info.pid = 0;
    xlogger_Write(&info, message.UTF8String);
}

+ (void)writelog:(KDLoggerType)logType fileName:(const char *)fileName lineNumber:(int)lineNumber funcName:(const char *)funcName format:(NSString *)format, ... {
    if ([self shouldLog:logType]) {
        va_list argList;
        va_start(argList, format);
        NSString* message = [[NSString alloc] initWithFormat:format arguments:argList];
        [self writelog:logType fileName:fileName lineNumber:lineNumber funcName:funcName message:message];
        va_end(argList);
    }
}

+ (BOOL)shouldLog:(int)type {
    if (type >= xlogger_Level()) {
        return YES;
    }
    return NO;
}

@end

@implementation KDLogUtil (Internal)

+ (TLogLevel)levelWithType:(KDLoggerType)type {
    TLogLevel level = kLevelNone;
    switch (type) {
        case KDLoggerTypeDebug:
            level = kLevelDebug;
            break;
        case KDLoggerTypeInfo:
            level = kLevelInfo;
            break;
        case KDLoggerTypeWarn:
            level = kLevelWarn;
            break;
        case KDLoggerTypeError:
            level = kLevelError;
            break;
        case KDLoggerTypeFatal:
            level = kLevelFatal;
            break;
        case KDLoggerTypeNone:
            level = kLevelNone;
            break;
        case KDLoggerTypeAll:
            level = kLevelAll;
            break;
        default:
            break;
    }
    return level;
}

@end

// Tencent is pleased to support the open source community by making Mars available.
// Copyright (C) 2016 THL A29 Limited, a Tencent company. All rights reserved.

// Licensed under the MIT License (the "License"); you may not use this file except in 
// compliance with the License. You may obtain a copy of the License at
// http://opensource.org/licenses/MIT

// Unless required by applicable law or agreed to in writing, software distributed under the License is
// distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
// either express or implied. See the License for the specific language governing permissions and
// limitations under the License.

/** * created on : 2012-11-28 * author : yerungui, caoshaokun
 */
#include "stn_callback.h"

#import <string>
#import <mars/comm/autobuffer.h>
#import <mars/xlog/xlogger.h>
#import <mars/stn/stn.h>

#include "KDMercService.h"

namespace mars {
    namespace stn {
        
StnCallBack* StnCallBack::instance_ = NULL;
        
StnCallBack* StnCallBack::Instance() {
    if(instance_ == NULL) {
        instance_ = new StnCallBack();
    }
    
    return instance_;
}
        
void StnCallBack::Release() {
    delete instance_;
    instance_ = NULL;
}
        
bool StnCallBack::MakesureAuthed() {
    return false;
}

void StnCallBack::TrafficData(ssize_t _send, ssize_t _recv) {
    xdebug2(TSF"send:%_, recv:%_", _send, _recv);
}

std::vector<std::string> StnCallBack::OnNewDns(const std::string& _host) {
    NSString *host = [[NSString alloc] initWithUTF8String:_host.c_str()];
    NSArray *arr = [[KDMercService sharedInstance] OnNewDns:host];
    
    std::vector<std::string> vector;
    for (NSString *str in arr) {
        vector.push_back(std::string(str.UTF8String));
    }
    
    return vector;
}

/*
 收到 SVR PUSH 下来的消息。
 
 _channel_id : 通道 id, 可忽略
 _cmdid : push 下来的消息的命令号。
 _taskid : 任务 id, 暂时忽略 _body : push 下来的数据。
 _extend : 扩展字段，暂时忽略
 */
void StnCallBack::OnPush(uint64_t _channel_id, uint32_t _cmdid, uint32_t _taskid, const AutoBuffer& _body, const AutoBuffer& _extend) {
    if (_body.Length() > 0) {
        NSData* recvData = [NSData dataWithBytes:(const void *) _body.Ptr() length:_body.Length()];
        [[KDMercService sharedInstance] OnPushWithCmd:_cmdid data:recvData];
    }
}

/*
 要求上层对 Task 组包。
 
 _taskid : 任务 id。
 _user_context : 和 Task.user_context 值相同。
 outbuffer : 返回的组包好的数据。
 extend : 扩展字段，暂时忽略
 error_code : 返回的组包的错误码，Mars 用作打日志，不会根据该值做相关逻辑。
 channel_select : 该 Task 所用的长连还是短连。
 return : true 组包成功，false 组包失败。
 */
bool StnCallBack::Req2Buf(uint32_t _taskid, void* const _user_context, AutoBuffer& _outbuffer, AutoBuffer& _extend, int& _error_code, const int _channel_select) {
    NSData* requestData =  [[KDMercService sharedInstance] Request2BufferWithTaskID:_taskid userContext:_user_context];
    if (requestData == nil) {
        requestData = [[NSData alloc] init];
    }
    _outbuffer.AllocWrite(requestData.length);
    _outbuffer.Write(requestData.bytes,requestData.length);
    return requestData.length > 0;
}

/*
 要求上层对服务器的回包进行解包。
 
 taskid : 任务 id。
 user_context : 和 Task. user_context 值相同。
 inbuffer : 服务器返回的数据，待解的数据包。
 error_code : 返回的解包的错误码，Mars 用作打日志，不会根据该值做相关逻辑。
 channel_select : 该 Task 所用的长连还是短连。
 return : 值见 stn.h 中 kTaskFailHandleXXX，解包成功返回kTaskFailHandleNormal，session 超时返回kTaskFailHandleSessionTimeout，解包失败并不再重试该任务返回kTaskFailHandleTaskEnd，其他错误返回kTaskFailHandleDefault。
 */
int StnCallBack::Buf2Resp(uint32_t _taskid, void* const _user_context, const AutoBuffer& _inbuffer, const AutoBuffer& _extend, int& _error_code, const int _channel_select) {
    
    int handle_type = mars::stn::kTaskFailHandleNormal;
    NSData* responseData = [NSData dataWithBytes:(const void *) _inbuffer.Ptr() length:_inbuffer.Length()];
    NSInteger errorCode = [[KDMercService sharedInstance] Buffer2ResponseWithTaskID:_taskid ResponseData:responseData userContext:_user_context];
    
    if (errorCode != 0) {
        handle_type = mars::stn::kTaskFailHandleDefault;
    }
    
    return handle_type;
}

/*
 _taskid : 任务 id。
 _user_context : 和 Task.userContext 值相同。
 _error_type : Buf2Resp 的返回值
 _error_code : 值和 Buf2Resp 的 error_code 相同
 return : 返回该 Task 的错误码，用作统计上报。
 */
int StnCallBack::OnTaskEnd(uint32_t _taskid, void* const _user_context, int _error_type, int _error_code) {
    return (int)[[KDMercService sharedInstance] OnTaskEndWithTaskID:_taskid userContext:_user_context errType:_error_type errCode:_error_code];
}

int StnCallBack::OnOpenSession(int error_code, const std::string& description) {
    [[KDMercService sharedInstance] OnOpenSessionErrorCode:error_code
                                                  errorMsg:[[NSString alloc] initWithUTF8String:description.c_str()]];
    
    return 0;
}

#pragma mark -
void StnCallBack::ReportConnectStatus(int _status, int longlink_status) {
    [[KDMercService sharedInstance] OnConnectionStatusChange:_status longConnStatus:longlink_status];

    /*
    switch (longlink_status) {
        case mars::stn::kServerFailed:
        case mars::stn::kServerDown:
        case mars::stn::kGateWayFailed:
            break;
        case mars::stn::kConnecting:
            break;
        case mars::stn::kConnected:
            break;
        case mars::stn::kNetworkUnkown:
            return;
        default:
            return;
    }
     */
}

// synccheck：长链成功后由网络组件触发
// 需要组件组包，发送一个req过去，网络成功会有resp，但没有taskend，处理事务时要注意网络时序
// 不需组件组包，使用长链做一个sync，不用重试
/*
 长连接刚建立时用来绑定用户身份与长连的对应关系以及同步消息，可用 sync 代替。
 */
int  StnCallBack::GetLonglinkIdentifyCheckBuffer(AutoBuffer& _identify_buffer, AutoBuffer& _buffer_hash, int32_t& _cmdid) {
    
    return IdentifyMode::kCheckNever;
}

bool StnCallBack::OnLonglinkIdentifyResponse(const AutoBuffer& _response_buffer, const AutoBuffer& _identify_buffer_hash) {
    
    return false;
}

//
void StnCallBack::RequestSync() {
    //
}
        
}
}







// Tencent is pleased to support the open source community by making Mars available.
// Copyright (C) 2017 THL A29 Limited, a Tencent company. All rights reserved.

// Licensed under the MIT License (the "License"); you may not use this file except in 
// compliance with the License. You may obtain a copy of the License at
// http://opensource.org/licenses/MIT

// Unless required by applicable law or agreed to in writing, software distributed under the License is
// distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
// either express or implied. See the License for the specific language governing permissions and
// limitations under the License.

/*
*  longlink_packer.cc
*
*  Created on: 2017-7-7
*      Author: chenzihao
*/

#include "longlink_packer.h"

#ifndef _WIN32
#include <arpa/inet.h>
#endif

#if defined(__APPLE__) || defined(OS_WIN) || defined(OS_MACOSX)
#include "mars/xlog/xlogger.h"
#else
#include "mars/comm/xlogger/xlogger.h"
#endif
#include "mars/comm/autobuffer.h"
#include "mars/stn/stn.h"

#define USES_MERC_FUNC1 1	/// 0/1 

#if (USES_MERC_FUNC1==1)
static uint16_t sg_client_version = 1;
#else
static uint32_t sg_client_version = 0;
#endif

#if (USES_MERC_FUNC1==1)
#include "mars/stn/stn_logic.h"
#if defined(__APPLE__) || defined(OS_WIN) || defined(OS_MACOSX)
//#include "stnproto_logic.h"
#include "mars/openssl/aes_crypt.h"
#include "mars/openssl/rsa_crypt.h"
#else
//#include "mars/stn/proto/stnproto_logic.h"
#include "mars/openssl/export_include/aes_crypt.h"
#include "mars/openssl/export_include/rsa_crypt.h"
#endif
//#include "mars/comm/thirdparty/stl/aes.h"
//#include "mars/comm/thirdparty/stl/rsa.h"
//#include "mars/comm/thirdparty/stl/zlib.h"
//#include "mars/comm/thread/mutex.h"
//#include "mars/comm/thread/lock.h"

static uint64_t theSessionId = 0;
static std::string theRSAPublicKey;
static std::string theOpenToken;
static std::string theUserAgent;
static std::string theClientId;
//static std::string theUserData;
static uint32_t theLoginTaskId = 0;
static std::string theAESPasswordRelease;
static std::string theAESPasswordRequest;

//typedef enum MERC_CMDID_TYPE {
//	MERC_CMDID_OPEN_SESSION = 1		/// 上行确认通讯加密密码（成功ACK返回，错误直接关闭该长连接）
//	, MERC_CMDID_SEND_DATA				/// 上行业务数据（是否需要ACK由业务决定）
//	, MERC_CMDID_PUSH_DATA				/// 服务端下行推送消息（是否需要ACK由业务决定）
//	, MERC_CMDID_HB				= 6		/// 心跳
//}MERC_CMDID_TYPE;
//
///// for package_property
//#define PACKAGE_PROPERTY_IS_GZIB	0x1	/// 这是GZIB压缩

#ifndef WIN32
#ifndef htonll
inline uint64_t htonll(uint64_t val) {
	return (((uint64_t)htonl((unsigned int)((val << 32) >> 32))) << 32) | (unsigned int)htonl((unsigned int)(val >> 32));
}
#endif
#ifndef ntohll
inline uint64_t ntohll(uint64_t val) {
	return (((uint64_t)ntohl((unsigned int)((val << 32) >> 32))) << 32) | (unsigned int)ntohl((unsigned int)(val >> 32));
}
#endif
#endif // !WIN32

#endif	/// USES_MERC_FUNC1

#pragma pack(push, 1)
#if (USES_MERC_FUNC1==1)
/// 2+2+4+8+2+4+4=26
struct __STNetMsgXpHeader {
	uint16_t    version;
	uint16_t    cmdid;
	uint32_t    seq;
	uint64_t    session_id;
	uint16_t    package_property;
	uint32_t	body_length;
};
#else
/// 4+4+4+4=20
struct __STNetMsgXpHeader {
    uint32_t    head_length;
    uint32_t    client_version;
    uint32_t    cmdid;
    uint32_t    seq;
    uint32_t	body_length;
};
#endif
#pragma pack(pop)


namespace mars {
namespace stn {
longlink_tracker* (*longlink_tracker::Create)()
= []() {
    return new longlink_tracker;
};
    
void SetClientVersion(uint32_t _client_version)  {
    sg_client_version = _client_version;
}

//#if (USES_MERC_FUNC==1)#
void SetParameter(int32_t parameter, const std::string& value) {
	switch (parameter)
	{
	case MERC_PARAMETER_PUBLIC_KEY:
		theRSAPublicKey = value;
		break;
	case MERC_PARAMETER_OPEN_TOKEN:
		theOpenToken = value;
		break;
	case MERC_PARAMETER_USER_AGENT:
		theUserAgent = value;
		break;
	case MERC_PARAMETER_CLIENT_ID:
		theClientId = value;
		break;
	default:
		break;
	}
}

//Mutex theMutex;
//ScopedLock lock(theMutex);

bool theOpenSessioning = false;
bool theCloseSessioning = false;	/// 为什么用二个变量，因为可以支持普通发起TASK，opensession
uint32_t theCloseSessionTaskId = 0;	/// 记录 close session taskid，用于预防，CloseSession 后立即 OpenSession 申请二个sessionid，使用错乱问题；

bool OpenSession() {
	if (theRSAPublicKey.empty()) {
		xerror2(TSF"public key empty error.");
		return false;
	}
	else if (theOpenToken.empty()) {
		xerror2(TSF"open token empty error.");
		return false;
	}
	else if (theUserAgent.empty()) {
		xerror2(TSF"user agent empty error.");
		return false;
	}
	else if (theClientId.empty()) {
		xerror2(TSF"clientid empty error.");
		return false;
	}
	else if (theSessionId > 0) {
		/// 当前已经 open 成功
		xinfo2(TSF"callback from open session");
		AutoBuffer autoBuffer;
		int error_code = 0;
		Buf2Resp(0, (void*)1001, autoBuffer, autoBuffer, error_code, 0);
		return true;
	}
	else if (theOpenSessioning) {
		xwarn2(TSF"reopen session.");
		//return false;
	}

	theSessionId = 0;
	mars::stn::ClearCmds();
	theOpenSessioning = true;
	theCloseSessioning = false;
	mars::stn::Task ctask;
	ctask.send_only = false;
	ctask.cgi = const_cgi_open_session;
	ctask.total_timetout = 10 * 60 * 1000;
	ctask.cmdid = MERC_CMDID_OPEN_SESSION;
	//ctask.channel_select = Task::kChannelBoth;	/// 
	//ctask.channel_select = Task::kChannelShort;	/// ok
	//ctask.shortlink_host_list.push_back("172.20.200.24");	/// for test
	ctask.channel_select = Task::kChannelLong;		/// ok（强制只走长连）
	ctask.user_context = (void*)999;
	mars::stn::StartTask(ctask);
	return true;
}
void CloseSession() {
	mars::stn::ClearCmds();
	theOpenSessioning = false;
	if (theSessionId != 0) {
		theCloseSessioning = true;
		theSessionId = 0;
		//mars::stn::ClearCmds();	/// *
		//mars::stn::ClearTasks();
		mars::stn::Task ctask;
		theCloseSessionTaskId = ctask.taskid;
		ctask.send_only = true;
		ctask.cgi = const_cgi_open_session;
		ctask.total_timetout = 30 * 1000;
		ctask.cmdid = MERC_CMDID_OPEN_SESSION;
		ctask.channel_select = Task::kChannelBoth;
		//ctask.channel_select = Task::kChannelLong;
		ctask.user_context = (void*)999;
		mars::stn::StartTask(ctask);
	}
}

uint64_t GetSessionId() {
	return theSessionId;
}
bool IsOpenSessioning() {
	return theOpenSessioning;
}
std::string GetRSAPublicKey() {
	return theRSAPublicKey;
}

void SetOpenSessionId(uint64_t session_id, uint32_t from_task_id) {
	if (session_id > 0) {
		if (theCloseSessioning || (theCloseSessionTaskId > 0 && theCloseSessionTaskId == from_task_id)) {
			theCloseSessionTaskId = 0;
			theCloseSessioning = false;
			xinfo2(TSF"close sessionid:%_ taskid:%_", session_id, from_task_id);
			return;
		}
		else {
			theSessionId = session_id;
			theAESPasswordRelease = theAESPasswordRequest;	/// 空表示通讯不加密
			theAESPasswordRequest.clear();
			xinfo2(TSF"opened sessionid:%_ taskid:%_", theSessionId, from_task_id);
		}

		theCloseSessionTaskId = 0;
		theLoginTaskId = 0;
		if (theOpenSessioning) {
			mars::stn::Task ctask;
			theLoginTaskId = ctask.taskid;
			ctask.send_only = false;
			ctask.cgi = const_cgi_login;
			ctask.total_timetout = 10 * 60 * 1000;
			ctask.cmdid = MERC_CMDID_SEND_DATA;
			ctask.channel_select = Task::kChannelBoth;
			//ctask.channel_select = Task::kChannelShort;	/// ok
			//ctask.shortlink_host_list.push_back("172.20.200.24");	/// for test
			//ctask.channel_select = Task::kChannelLong;		/// ok
			ctask.user_context = (void*)1000;
			mars::stn::StartTask(ctask);
		}
	}
}

bool IsNeedAesEncrpty() {
	return theAESPasswordRelease.empty() ? false : true;
}
std::string GetAESPasswordString() { return theAESPasswordRelease; }
std::string GetOpenToken() { return theOpenToken; }
std::string GetUserAgent() { return theUserAgent; }
std::string GetClientId() { return theClientId; }

int GetAESPassword(const std::string& sPublicKey, unsigned char ** pOutSslData) {
	const int nSslSize = 16;
	char pBufferTemp[64];
	memset(pBufferTemp, 0, sizeof(pBufferTemp));
	GetSaltString(pBufferTemp, nSslSize);	/// 16*8=128Bit，RSA加密AES对称密码
	void* pRSA = rsa_open_public_mem2(sPublicKey.c_str());
	if (pRSA == 0) {
		xerror2(TSF"rsa open public key error:%_", sPublicKey.c_str());
		return -1;
	}
	const int nLen = rsa_public_encrypt2(pRSA, (const unsigned char*)pBufferTemp, (int)strlen(pBufferTemp), pOutSslData);
	if (nLen < 0) {
		xerror2(TSF"rsa public enctpty error:%_", nLen);
		rsa_close(pRSA);
		return -2;
	}
	rsa_close(pRSA);
	theAESPasswordRequest = pBufferTemp;
	return nLen;
}
//#endif

#if (USES_MERC_FUNC1==1)
static int __unpack_test(const void* _packed, size_t _packed_len, uint32_t& _cmdid, uint32_t& _seq, size_t& _package_len, size_t& _body_len,
	uint64_t &pOutSessionId, uint16_t &pOutPackageProperty) {
    __STNetMsgXpHeader st = {0};
    if (_packed_len < sizeof(__STNetMsgXpHeader)) {
        _package_len = 0;
        _body_len = 0;
        return LONGLINK_UNPACK_CONTINUE;
    }
    
    memcpy(&st, _packed, sizeof(__STNetMsgXpHeader));
	uint32_t head_len = sizeof(__STNetMsgXpHeader);
	uint16_t client_version = ntohs(st.version);
    if (client_version != sg_client_version) {
        _package_len = 0;
        _body_len = 0;
    	return LONGLINK_UNPACK_FALSE;
    }
    _cmdid = ntohs(st.cmdid);
	_seq = ntohl(st.seq);
	pOutSessionId = ntohll(st.session_id);
	pOutPackageProperty = ntohs(st.package_property);
	_body_len = ntohl(st.body_length);
	_package_len = head_len + _body_len;
    if (_package_len > 1024*1024) { return LONGLINK_UNPACK_FALSE; }
    if (_package_len > _packed_len) { return LONGLINK_UNPACK_CONTINUE; }
    
    return LONGLINK_UNPACK_OK;
}
#else
static int __unpack_test(const void* _packed, size_t _packed_len, uint32_t& _cmdid, uint32_t& _seq, size_t& _package_len, size_t& _body_len) {
	__STNetMsgXpHeader st = { 0 };
	if (_packed_len < sizeof(__STNetMsgXpHeader)) {
		_package_len = 0;
		_body_len = 0;
		return LONGLINK_UNPACK_CONTINUE;
	}

	memcpy(&st, _packed, sizeof(__STNetMsgXpHeader));
	uint32_t head_len = ntohl(st.head_length);
	uint32_t client_version = ntohl(st.client_version);
	if (client_version != sg_client_version) {
		_package_len = 0;
		_body_len = 0;
		return LONGLINK_UNPACK_FALSE;
	}
	_cmdid = ntohl(st.cmdid);
	_seq = ntohl(st.seq);
	_body_len = ntohl(st.body_length);
	_package_len = head_len + _body_len;
	if (_package_len > 1024 * 1024) { return LONGLINK_UNPACK_FALSE; }
	if (_package_len > _packed_len) { return LONGLINK_UNPACK_CONTINUE; }

	return LONGLINK_UNPACK_OK;
}
#endif	/// USES_MERC_FUNC1
void (*longlink_pack)(uint32_t _cmdid, uint32_t _seq, const AutoBuffer& _body, const AutoBuffer& _extension, AutoBuffer& _packed, longlink_tracker* _tracker)
= [](uint32_t _cmdid, uint32_t _seq, const AutoBuffer& _body, const AutoBuffer& _extension, AutoBuffer& _packed, longlink_tracker* _tracker) {
    __STNetMsgXpHeader st = {0};
    
#if (USES_MERC_FUNC1==1)
	st.version = htons(sg_client_version);
	st.cmdid = htons(_cmdid);
	st.seq = htonl(_seq);
	st.session_id = htonll(theSessionId);

	switch (_cmdid)
	{
	case MERC_CMDID_OPEN_SESSION:
	{
		theSessionId = 0;
		/// 不能清空，清空会导致底层 not find task
		///mars::stn::ClearCmds();
		/// 填空，支持通讯不加密
		std::string sPublicKey;
		if (theOpenSessioning) {
			sPublicKey = theRSAPublicKey;
		}
		else {
			/// PublicKey公钥通过 Req2Buf 获取；
			if (_body.Ptr() != 0 && _body.Length() > 0) {
				theRSAPublicKey = std::string((const char*)_body.Ptr(), _body.Length());
			}
			sPublicKey = theRSAPublicKey;
		}
		if (!sPublicKey.empty()) {
			unsigned char * pSslData = NULL;
			const int nLen = GetAESPassword(sPublicKey, &pSslData);
			if (nLen < 0) {
				break;
			}
			//const int nSslSize = 16;
			//char pBufferTemp[64];
			//memset(pBufferTemp, 0, sizeof(pBufferTemp));
			//GetSaltString(pBufferTemp, nSslSize);	/// 16*8=128Bit，RSA加密AES对称密码
			//theAESPasswordRequest = pBufferTemp;
			//void* pRSA = rsa_open_public_mem2(sPublicKey.c_str());
			//if (pRSA==0) {
			//	xerror2(TSF"rsa open public key error:%_", sPublicKey.c_str());
			//	break;
			//}
			//unsigned char * pSslData = NULL;
			//const int nLen = rsa_public_encrypt2(pRSA, (const unsigned char*)theAESPasswordRequest.c_str(), (int)theAESPasswordRequest.size(), &pSslData);
			//if (nLen < 0) {
			//	xerror2(TSF"rsa public enctpty error:%_", nLen);
			//	rsa_close(pRSA);
			//	break;
			//}
			//rsa_close(pRSA);

			st.package_property = 0;
			st.session_id = 0;
			st.body_length = htonl(nLen);
			_packed.AllocWrite(sizeof(__STNetMsgXpHeader) + nLen);
			_packed.Write(&st, sizeof(st));
			_packed.Write((const void*)pSslData, (size_t)nLen);
			delete[] pSslData;
		}
		else {
			theRSAPublicKey.clear();
			theAESPasswordRequest.clear();
			st.body_length = 0;
			_packed.AllocWrite(sizeof(__STNetMsgXpHeader));
			_packed.Write(&st, sizeof(st));
		}
		_packed.Seek(0, AutoBuffer::ESeekStart);
		if (theCloseSessioning || (theCloseSessionTaskId > 0 && theCloseSessionTaskId == _seq)) {
			xinfo2(TSF"request close sessionid...");
		}
		else {
			xinfo2(TSF"request open sessionid...");
		}
		return;
	}
	case MERC_CMDID_HB:
	{
		if (theSessionId == 0) {
			return;
		}
		break;
	}

	case MERC_CMDID_SEND_DATA:
	//case MERC_CMDID_HB:	/// for test
	{
		bool isUserData = false;
		std::string theUserData;
		if (theOpenSessioning && theLoginTaskId > 0 && theLoginTaskId == _seq) {
			theUserData.append("{");
			/// cmd:
			theUserData.append("\"cmd\":\"login\",");
			/// openToken:
			theUserData.append("\"openToken\":\"");
			theUserData.append(theOpenToken);
			theUserData.append("\",");
			/// userAgent:
			theUserData.append("\"userAgent\":\"");
			theUserData.append(theUserAgent);
			theUserData.append("\",");
			/// clientid:
			theUserData.append("\"clientId\":\"");
			theUserData.append(theClientId);
			theUserData.append("\"");
			theUserData.append("}");
			isUserData = true;
		}
		else if (_extension.Ptr() != 0 && _extension.Length() > 0) {
			const std::string theCgi((const char*)_extension.Ptr(), _extension.Length());
			if (theCgi == const_cgi_send_msg ||
				theCgi == const_cgi_public_send_msg) {
				/// 长连发消息
				if (_body.Ptr() == 0 || _body.Length() == 0) {
					break;
				}
				theUserData.append("{");
				/// cmd:
				if (theCgi == const_cgi_send_msg)
					theUserData.append("\"cmd\":\"sendMsg\",");
				else
					theUserData.append("\"cmd\":\"pubSendMsg\",");
				/// data:
				theUserData.append("\"data\":");
				theUserData.append(std::string((const char*)_body.Ptr(), _body.Length()));
				//theUserData.append((const char*)_body.Ptr());
				theUserData.append("}");
				isUserData = true;
			}
		}
		if (!isUserData && (_body.Ptr() == 0 || _body.Length() == 0)) {
			break;
		}
		bool bIsGzib = isUserData ? (theUserData.size() > 128 ? true : false) : (_body.Length() > 128 ? true : false);
		const bool bNeedAesEncrpty = IsNeedAesEncrpty();
		unsigned char * pGZIBDestBuffer = (unsigned char*)(isUserData ? theUserData.c_str() : _body.Ptr());
		unsigned long nGZIBDestSize = (unsigned long)(isUserData ? theUserData.size() : _body.Length());
		if (bIsGzib) {
			/// gzib 压缩
			nGZIBDestSize = (isUserData ? theUserData.size() : _body.Length()) + 256;
			pGZIBDestBuffer = new unsigned char[nGZIBDestSize];
			const int gzipRet = GZipData(
				(const unsigned char*)(isUserData ? theUserData.c_str() : _body.Ptr()),
				(unsigned long)(isUserData ? theUserData.size() : _body.Length()),
				pGZIBDestBuffer,
				&nGZIBDestSize,
				Z_DEFAULT_COMPRESSION);
			if (gzipRet != Z_OK) {
				delete[] pGZIBDestBuffer;
				xerror2(TSF"GZipData error, ret:%_, length:%_", gzipRet, (isUserData ? theUserData.size() : _body.Length()));
				bIsGzib = false;
				pGZIBDestBuffer = (unsigned char*)(isUserData ? theUserData.c_str() : _body.Ptr());
				nGZIBDestSize = (isUserData ? theUserData.size() : _body.Length());
			}
			else {
				pGZIBDestBuffer[nGZIBDestSize] = '\0';	/// 
			}
		}
		
		int nAESDataSize = nGZIBDestSize;
		unsigned char * pAESDataBuffer = pGZIBDestBuffer;
		if (bNeedAesEncrpty) {
			/// AES 加密
			nAESDataSize = (nGZIBDestSize / 16 + 1 )* 16;
			pAESDataBuffer = new unsigned char[nAESDataSize + 1];
			const int aesRet = aes_cbc_encrypt_full(
				(const unsigned char*)theAESPasswordRelease.c_str(),
				(int)theAESPasswordRelease.size(),
				pGZIBDestBuffer,
				nGZIBDestSize,
				pAESDataBuffer,
				&nAESDataSize);
			if (aesRet != 0) {
				xerror2(TSF"aes_cbc_encrypt_full error, ret:%_, length:%_", aesRet, nGZIBDestSize);
				if (bIsGzib) {
					delete[] pGZIBDestBuffer;
				}
				delete[] pAESDataBuffer;
				break;
			}
		}

		st.package_property = htons(bIsGzib ? PACKAGE_PROPERTY_IS_GZIB : 0);
		st.body_length = htonl(nAESDataSize);
		_packed.AllocWrite(sizeof(__STNetMsgXpHeader) + nAESDataSize);
		_packed.Write(&st, sizeof(st));
		_packed.Write((const void*)pAESDataBuffer, (size_t)nAESDataSize);
		_packed.Seek(0, AutoBuffer::ESeekStart);
		if (bIsGzib) {
			delete[] pGZIBDestBuffer;
		}
		if (bNeedAesEncrpty) {
			delete[] pAESDataBuffer;
		}
		xinfo2(TSF"longlink_pack, sessionid:%_, cmdid:%_, body_length:%_, org_length:%_", theSessionId, _cmdid, nAESDataSize, _body.Length());
		return;
	}
	default:
		break;
	}

	st.package_property = 0;
	st.body_length = htonl(_body.Length());
	_packed.AllocWrite(sizeof(__STNetMsgXpHeader) + _body.Length());
	_packed.Write(&st, sizeof(st));
	
	if (NULL != _body.Ptr()) _packed.Write(_body.Ptr(), _body.Length());
	_packed.Seek(0, AutoBuffer::ESeekStart);
#else
	st.head_length = htonl(sizeof(__STNetMsgXpHeader));
	st.client_version = htonl(sg_client_version);
	st.cmdid = htonl(_cmdid);
	st.seq = htonl(_seq);
	st.body_length = htonl(_body.Length());

	_packed.AllocWrite(sizeof(__STNetMsgXpHeader) + _body.Length());
	_packed.Write(&st, sizeof(st));

	if (NULL != _body.Ptr()) _packed.Write(_body.Ptr(), _body.Length());
	_packed.Seek(0, AutoBuffer::ESeekStart);
#endif // USES_MERC_FUNC1
};


int (*longlink_unpack)(const AutoBuffer& _packed, uint32_t& _cmdid, uint32_t& _seq, size_t& _package_len, AutoBuffer& _body, AutoBuffer& _extension, longlink_tracker* _tracker)
= [](const AutoBuffer& _packed, uint32_t& _cmdid, uint32_t& _seq, size_t& _package_len, AutoBuffer& _body, AutoBuffer& _extension, longlink_tracker* _tracker) {
   size_t body_len = 0;
#if (USES_MERC_FUNC1==1)
   uint64_t session_id = 0; 
   uint16_t package_property = 0;
   int ret = __unpack_test(_packed.Ptr(), _packed.Length(), _cmdid, _seq, _package_len, body_len, session_id, package_property);
#else
   int ret = __unpack_test(_packed.Ptr(), _packed.Length(), _cmdid,  _seq, _package_len, body_len);
#endif	/// USES_MERC_FUNC1

    if (LONGLINK_UNPACK_OK != ret) return ret;
    
#if (USES_MERC_FUNC1==1)
	switch (_cmdid)
	{
	case MERC_CMDID_OPEN_SESSION:
	{
		if (session_id == 0) {
			break;
		}
		SetOpenSessionId(session_id, _seq);
		
		/*theLoginTaskId = 0;
		if (theOpenSessioning) {
			mars::stn::Task ctask;
			theLoginTaskId = ctask.taskid;
			ctask.send_only = false;
			ctask.cgi = const_cgi_login;
			ctask.total_timetout = 30 * 1000;
			ctask.cmdid = MERC_CMDID_SEND_DATA;
			ctask.channel_select = Task::kChannelBoth;
			ctask.user_context = (void*)1000;
			mars::stn::StartTask(ctask);
		}*/
		break;
	}
	case MERC_CMDID_SEND_DATA:
	case MERC_CMDID_PUSH_DATA:
	//case MERC_CMDID_HB:	/// for test
	{
		if (body_len == 0) {
			break;
		}
		else if (theSessionId == 0) {
			/// 本地 session_id 处于关闭状态，不接收数据
			xwarn2(TSF"local_session_id 0 warn, cmd:%_, remote_session_id:%_", _cmdid, session_id);
			return LONGLINK_UNPACK_FALSE;
		}
		if (session_id != theSessionId) {
			xerror2(TSF"session_id error, cmd:%_, local_session_id:%_, remote_session_id:%_", _cmdid, theSessionId, session_id);
			if (session_id > 0) {
				return LONGLINK_UNPACK_FALSE;
			}
		}
		const bool bIsGzib = (package_property&PACKAGE_PROPERTY_IS_GZIB) == 0 ? false : true;
		const bool bNeedAesDecrpty = (session_id==0 || theAESPasswordRelease.empty()) ? false : true;

		unsigned char * pAESDataBuffer = (unsigned char*)_packed.Ptr(_package_len - body_len);
		int org_length2 = body_len;
		/// AES 解密
		if (bNeedAesDecrpty) {
			pAESDataBuffer = new unsigned char[body_len + 20];
			//memset(pAESDataBuffer, 0, body_len + 20);
			int dataLength = body_len+20;
			const int aesRet = aes_cbc_decrypt_full(
				(const unsigned char*)theAESPasswordRelease.c_str(),
				(int)theAESPasswordRelease.size(),
				(const unsigned char*)_packed.Ptr(_package_len - body_len),
				(int)body_len,
				pAESDataBuffer,
				&dataLength);
			if (aesRet != 0) {
				xerror2(TSF"aes_cbc_decrypt_full error, ret:%_, length:%_", aesRet, body_len);
				delete[] pAESDataBuffer;
				return LONGLINK_UNPACK_FALSE;
			}
			//pAESDataBuffer[dataLength] = '\0';
			org_length2 = dataLength;
		}
		unsigned char * pUnzipData = pAESDataBuffer;
		if (bIsGzib) {
			/// GZIB解压
			unsigned long nUnZipDestSize = org_length2*100 + 32;
			pUnzipData = new unsigned char[nUnZipDestSize];
			const int nUnZipRet = UnGZipData(
				pAESDataBuffer,
				(unsigned long)org_length2,
				pUnzipData,
				&nUnZipDestSize);
			if (nUnZipRet != Z_OK) {
				xerror2(TSF"UnGZipData error, ret:%_, length:%_", nUnZipRet, body_len);
				if (bNeedAesDecrpty) {
					delete[] pAESDataBuffer;
				}
				delete[] pUnzipData;
				return LONGLINK_UNPACK_FALSE;
			}
			org_length2 = nUnZipDestSize;
		}
		/// {"success":true,"cmd":"login"}

		//{
		//	"cmd":"login",
		//	"success" : false | true
		//}
		if (theOpenSessioning &&
			_cmdid == MERC_CMDID_SEND_DATA &&
			theLoginTaskId > 0 && theLoginTaskId == _seq) {
			theOpenSessioning = false;
			theLoginTaskId = 0;
			//const std::string sLoginResponse((const char*)pUnzipData);
		}
		_body.Write(AutoBuffer::ESeekCur, (const void*)pUnzipData, org_length2);
		if (bNeedAesDecrpty) {
			delete[] pAESDataBuffer;
		}
		if (bIsGzib) {
			delete[] pUnzipData;
		}
		xinfo2(TSF"longlink_unpack, sessionid:%_, cmdid:%_, body_length:%_, org_length:%_", theSessionId, _cmdid, _body.Length(), org_length2);
		return ret;
	}
	default:
		break;
	}
#endif	/// USES_MERC_FUNC1
    _body.Write(AutoBuffer::ESeekCur, _packed.Ptr(_package_len-body_len), body_len);

    return ret;
};


#define NOOP_CMDID 6
#define SIGNALKEEP_CMDID 243
#define PUSH_DATA_TASKID 0

uint32_t (*longlink_noop_cmdid)()
= []() -> uint32_t {
    return NOOP_CMDID;
};

bool  (*longlink_noop_isresp)(uint32_t _taskid, uint32_t _cmdid, uint32_t _recv_seq, const AutoBuffer& _body, const AutoBuffer& _extend)
= [](uint32_t _taskid, uint32_t _cmdid, uint32_t _recv_seq, const AutoBuffer& _body, const AutoBuffer& _extend) {
	return Task::kNoopTaskID == _recv_seq && NOOP_CMDID == _cmdid;
	//return Task::kNoopTaskID == _taskid && NOOP_CMDID == _cmdid;
};

uint32_t (*signal_keep_cmdid)()
= []() -> uint32_t {
    return SIGNALKEEP_CMDID;
};

void (*longlink_noop_req_body)(AutoBuffer& _body, AutoBuffer& _extend)
= [](AutoBuffer& _body, AutoBuffer& _extend) {
    
};
    
void (*longlink_noop_resp_body)(const AutoBuffer& _body, const AutoBuffer& _extend)
= [](const AutoBuffer& _body, const AutoBuffer& _extend) {
    
};

uint32_t (*longlink_noop_interval)()
= []() -> uint32_t {
	return 0;
};

bool (*longlink_complexconnect_need_verify)()
= []() {
	/// longlink_complexconnect_need_verify的含义是如果设为 true 
	/// 经过心跳包验证的连接才认为是成功的连接。
	/// 如果你的长连接建立连接后第一个包必须是验证包，该函数的返回值一定要设为false。
	/// 长连的鉴权是会回调GetLonglinkIdentifyCheckBuffer这个函数，实现即可，记得处理回包：OnLonglinkIdentifyResponse
    return false;
};

bool (*longlink_ispush)(uint32_t _cmdid, uint32_t _taskid, const AutoBuffer& _body, const AutoBuffer& _extend)
= [](uint32_t _cmdid, uint32_t _taskid, const AutoBuffer& _body, const AutoBuffer& _extend) {
#if (USES_MERC_FUNC1==1)
	return _cmdid == MERC_CMDID_PUSH_DATA;
#else
	return PUSH_DATA_TASKID == _taskid;
#endif	/// USES_MERC_FUNC1
};
    
bool (*longlink_identify_isresp)(uint32_t _sent_seq, uint32_t _cmdid, uint32_t _recv_seq, const AutoBuffer& _body, const AutoBuffer& _extend)
= [](uint32_t _sent_seq, uint32_t _cmdid, uint32_t _recv_seq, const AutoBuffer& _body, const AutoBuffer& _extend) {
    return _sent_seq == _recv_seq && 0 != _sent_seq;
};

}
}

#pragma once

#include "HttpRequest.h"

/*
* 用于管理一次 HTTP 请求的解析过程，跟踪解析状态，并保存解析得到的 HttpRequest 对象。
*/

class Buffer;

class HttpContext
{
 public:
  // 当前解析状态
  enum HttpRequestParseState
  {
    kExpectRequestLine, // 期待解析请求行
    kExpectHeaders,     // 期待解析头部
    kExpectBody,        // 期待解析请求体
    kGotAll,            // 解析完成
  };

  HttpContext()
    : state_(kExpectRequestLine)
  {
  }

  // default copy-ctor, dtor and assignment are fine

  // 解析请求，返回是否解析成功
  bool parseRequest(Buffer* buf, Timestamp receiveTime);
  // 判断解析是否完成
  bool gotAll() const
  { return state_ == kGotAll; }
  // 重置解析状态和请求对象，复用
  void reset()
  {
    state_ = kExpectRequestLine;
    HttpRequest dummy;
    request_.swap(dummy);
  }
  // 返回正在解析的http请求对象
  const HttpRequest& request() const
  { return request_; }

  // 返回正在解析的http请求对象
  HttpRequest& request()
  { return request_; }

 private:
  // 用于解析请求行，私有
  bool processRequestLine(const char* begin, const char* end);
  // 当前解析状态
  HttpRequestParseState state_;
  // 正在解析的HTTP请求对象，保存解析后的请求数据
  HttpRequest request_;
};

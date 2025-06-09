#pragma once

#include "Timestamp.h"

#include <map>
#include <assert.h>
#include <stdio.h>
#include <sstream>
#include <iomanip>

using namespace std;

/*
* 用于表示和存储一个 HTTP 请求的所有信息，包括请求方法、路径、查询参数、头部、接收时间等。
*/

class HttpRequest
{
public:
  // http请求的方法
  enum Method
  {
    kInvalid,
    kGet,
    kPost,
    kHead,
    kPut,
    kDelete
  };
  // http版本
  enum Version
  {
    kUnknown,
    kHttp10,
    kHttp11
  };

  HttpRequest()
      : method_(kInvalid),
        version_(kUnknown)
  {
  }
  // 设置http版本
  void setVersion(Version v)
  {
    version_ = v;
  }
  // 获取版本
  Version getVersion() const
  {
    return version_;
  }
  // 设置请求方法
  bool setMethod(const char *start, const char *end)
  {
    assert(method_ == kInvalid);
    string m(start, end);
    if (m == "GET")
    {
      method_ = kGet;
    }
    else if (m == "POST")
    {
      method_ = kPost;
    }
    else if (m == "HEAD")
    {
      method_ = kHead;
    }
    else if (m == "PUT")
    {
      method_ = kPut;
    }
    else if (m == "DELETE")
    {
      method_ = kDelete;
    }
    else
    {
      method_ = kInvalid;
    }
    return method_ != kInvalid;
  }
  // 返回请求方法
  Method method() const
  {
    return method_;
  }
  // 返回字符格式的请求方法
  const char *methodString() const
  {
    const char *result = "UNKNOWN";
    switch (method_)
    {
    case kGet:
      result = "GET";
      break;
    case kPost:
      result = "POST";
      break;
    case kHead:
      result = "HEAD";
      break;
    case kPut:
      result = "PUT";
      break;
    case kDelete:
      result = "DELETE";
      break;
    default:
      break;
    }
    return result;
  }
  // 设置请求路径
  void setPath(const char *start, const char *end)
  {
    path_.assign(start, end);
  }
  // 获取请求路径
  const string &path() const
  {
    return path_;
  }
  // 设置查询字符串
  void setQuery(const char *start, const char *end)
  {
    query_.assign(start, end);
  }
  // 获取查询字符串
  const string &query() const
  {
    return query_;
  }
  // 设置接收时间
  void setReceiveTime(Timestamp t)
  {
    receiveTime_ = t;
  }
  // 返回接收时间
  Timestamp receiveTime() const
  {
    return receiveTime_;
  }
  // 添加请求头。解析一行HTTP头部文件，存入headers_中，colon是冒号
  void addHeader(const char *start, const char *colon, const char *end)
  {
    // 取出头部字段名
    string field(start, colon);
    // 跳过冒号
    ++colon;
    // 跳过冒号后的空白字符
    while (colon < end && ::isspace(*colon))
    {
      ++colon;
    }
    // 取出头部字段值
    string value(colon, end);
    while (!value.empty() && isspace(value[value.size() - 1]))
    {
      // 去除字段值末尾的空白字符
      value.resize(value.size() - 1);
    }
    // 将请求头存入map
    headers_[field] = value;
  }

  string getHeader(const string &field) const
  {
    string result;
    std::map<string, string>::const_iterator it = headers_.find(field);
    if (it != headers_.end())
    {
      result = it->second;
    }
    return result;
  }

  const std::map<string, string> &headers() const
  {
    return headers_;
  }
  // 设置请求体
  void setBody(const string & body)
  {
    body_ = body;
  }
  // 获取请求体
  const string &body() const
  {
    return body_;
  }
  // url解码
  string urlDecode(const string &src) const
  {
      ostringstream oss;
      for (size_t i = 0; i < src.size(); ++i)
      {
          if (src[i] == '%' && i + 2 < src.size())
          {
              int val = 0;
              istringstream iss(src.substr(i + 1, 2));
              if (iss >> hex >> val)
              {
                  oss << static_cast<char>(val);
                  i += 2;
              }
          }
          else if (src[i] == '+')
          {
              oss << ' ';
          }
          else
          {
              oss << src[i];
          }
      }
      return oss.str();
  }
  // 交换两个HttpRequest对象的内容
  void swap(HttpRequest &that)
  {
    std::swap(method_, that.method_);
    std::swap(version_, that.version_);
    path_.swap(that.path_);
    query_.swap(that.query_);
    std::swap(receiveTime_, that.receiveTime_);
    headers_.swap(that.headers_);
  }

private:
  Method method_;
  Version version_;
  // 存储请求的路径
  string path_;
  // 存储URL中的查询字符串
  string query_;
  // 存储请求体
  string body_;
  Timestamp receiveTime_;
  // 存储http请求头
  std::map<string, string> headers_;
};
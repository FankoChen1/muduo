#include "Buffer.h"
#include "HttpContext.h"

bool HttpContext::processRequestLine(const char *begin, const char *end)
{
    bool succeed = false;
    const char *start = begin;
    const char *space = std::find(start, end, ' ');
    if (space != end && request_.setMethod(start, space))
    {
        start = space + 1;
        space = std::find(start, end, ' ');
        if (space != end)
        {
            const char *question = std::find(start, space, '?');
            if (question != space)
            {
                request_.setPath(start, question);
                request_.setQuery(question, space);
            }
            else
            {
                request_.setPath(start, space);
            }
            start = space + 1;
            succeed = end - start == 8 && std::equal(start, end - 1, "HTTP/1.");
            if (succeed)
            {
                if (*(end - 1) == '1')
                {
                    request_.setVersion(HttpRequest::kHttp11);
                }
                else if (*(end - 1) == '0')
                {
                    request_.setVersion(HttpRequest::kHttp10);
                }
                else
                {
                    succeed = false;
                }
            }
        }
    }
    return succeed;
}

bool HttpContext::parseRequest(Buffer *buf, Timestamp receiveTime)
{
    bool ok = true;
    bool hasMore = true;
    while (hasMore)
    {
        if (state_ == kExpectRequestLine)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                ok = processRequestLine(buf->peek(), crlf);
                if (ok)
                {
                    request_.setReceiveTime(receiveTime);
                    buf->retrieveUntil(crlf + 2);
                    state_ = kExpectHeaders;
                }
                else
                {
                    hasMore = false;
                }
            }
            else
            {
                hasMore = false;
            }
        }
        else if (state_ == kExpectHeaders)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                const char *colon = std::find(buf->peek(), crlf, ':');
                if (colon != crlf)
                {
                    request_.addHeader(buf->peek(), colon, crlf);
                }
                else
                {
                    // 判断是否需要解析body（如有Content-Length且为POST/PUT等）
                    if (request_.method() == HttpRequest::kPost || request_.method() == HttpRequest::kPut)
                    {
                        std::string lenStr = request_.getHeader("Content-Length");
                        if (!lenStr.empty())
                        {
                            // 如果有Content-Length，则进入body解析状态
                            state_ = kExpectBody;
                        }
                        else
                        {
                            // 没有Content-Length，直接认为解析完成
                            state_ = kGotAll;
                            hasMore = false;
                        }
                    }
                    else
                    {
                        // GET/HEAD等无body，直接完成
                        state_ = kGotAll;
                        hasMore = false;
                    }
                }
                buf->retrieveUntil(crlf + 2);
            }
            else
            {
                hasMore = false;
            }
        }
        else if (state_ == kExpectBody)
        {
            // 只支持 Content-Length，且一次性读完
            std::string lenStr = request_.getHeader("Content-Length");
            if (!lenStr.empty())
            {
                size_t contentLength = static_cast<size_t>(atoi(lenStr.c_str()));
                if (buf->readableBytes() >= contentLength)
                {
                    request_.setBody(std::string(buf->peek(), contentLength));
                    buf->retrieve(contentLength);
                    state_ = kGotAll;
                    hasMore = false;
                }
                else
                {
                    // 数据还不够，等待更多数据
                    hasMore = false;
                }
            }
            else
            {
                // 没有 Content-Length，直接完成
                state_ = kGotAll;
                hasMore = false;
            }
        }
    }
    return ok;
}
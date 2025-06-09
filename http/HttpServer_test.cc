#include "HttpServer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "EventLoop.h"
#include "Logger.h"
#include "RedisConnectionPool.h"

#include <iostream>
#include <map>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

// #define DEBUG

using json = nlohmann::json;

bool benchmark = false;

// 封装返回 HTML 的函数
void renderHomeHtml(const std::string &htmlPath, HttpResponse *resp, const std::string &messagesJson = "[]")
{
    // 读取 HTML 文件
    std::ifstream htmlFile(htmlPath);
    if (!htmlFile.is_open())
    {
      LOG_ERROR("Failed to open HTML file: %s", htmlPath.c_str());
      resp->setStatusCode(HttpResponse::k404NotFound);
      resp->setBody("404 Not Found: home.html not found 1");
      return;
    }

    std::stringstream buffer;
    buffer << htmlFile.rdbuf();
    std::string htmlContent = buffer.str();

    // 将消息 JSON 嵌入到 HTML 中
    const std::string marker = "</head>";
    size_t pos = htmlContent.find(marker);
    if (pos != std::string::npos)
    {
      std::string script = "<script>window.initMessages = " + messagesJson + ";</script>";
      htmlContent.insert(pos, script);
    }
    else
    {
      LOG_ERROR("%s:Failed to find </head> in HTML, messages not embedded", __FUNCTION__);
    }

    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setContentType("text/html");
    resp->setBody(htmlContent);
}

// 格式化时间
time_t parseTimeString(const std::string& timeStr) {
    // 示例格式: "2023:10:01 10:00:00"
    std::tm tm = {};
    std::istringstream ss(timeStr);
    ss >> std::get_time(&tm, "%Y:%m:%d %H:%M:%S");
    if (ss.fail()) {
        return time(nullptr); // 解析失败返回当前时间
    }
    return std::mktime(&tm);
}

// 处理post请求
void handlePostRequest(const HttpRequest &req, HttpResponse *resp) {
    // 解析JSON请求体
    auto json_body = json::parse(req.body());

    // 验证必要字段
    if (!json_body.contains("name") || !json_body.contains("message") || !json_body.contains("time")) {
          LOG_ERROR("Missing name or message or time field\n");
          // 返回失败响应
          resp->setStatusCode(HttpResponse::k200Ok);
          resp->setContentType("application/json");
          resp->setBody(json{
              {"status", "failed"}
          }.dump());
          return;
    }

    // 提取数据
    std::string name = json_body["name"].get<std::string>();
    std::string message = json_body["message"].get<std::string>();
    std::string time_str = json_body["time"].get<std::string>();
    time_t timestamp = parseTimeString(time_str);

#ifdef DEBUG
    cout << name << ":" << message << ":" << time_str << endl;
#endif

    // 获取Redis连接
    RedisConnectionPool& redis_pool = RedisConnectionPool::getInstance();
    auto conn = redis_pool.getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection\n");
        // 返回失败响应
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setContentType("application/json");
        resp->setBody(json{
            {"status", "failed"}
        }.dump());
        return;
    }

    // 生成留言ID，记录留言到了多少条
    redisReply* id_reply = (redisReply*)redisCommand(conn.get(), "INCR message:id_counter");
    if (!id_reply || id_reply->type == REDIS_REPLY_ERROR) {
        freeReplyObject(id_reply);
        LOG_ERROR("Failed to generate message ID\n");
        // 返回失败响应
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setContentType("application/json");
        resp->setBody(json{
            {"status", "failed"}
        }.dump());
        return;
    }
    std::string message_id = "message:" + std::to_string(id_reply->integer);
    freeReplyObject(id_reply);

    // 存储留言详情到Hash
    redisReply* hset_reply = (redisReply*)redisCommand(conn.get(), 
        "HSET %s name %b message %b time %b", 
        message_id.c_str(),
        name.data(), name.size(),
        message.data(), message.size(),
        time_str.data(), time_str.size());
    
    if (!hset_reply || hset_reply->type == REDIS_REPLY_ERROR) {
        freeReplyObject(hset_reply);
        LOG_ERROR("Failed to store message details\n");
        // 返回失败响应
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setContentType("application/json");
        resp->setBody(json{
            {"status", "failed"}
        }.dump());
        return;
    }
    freeReplyObject(hset_reply);

    // 将留言ID添加到Sorted Set按时间排序
    redisReply* zadd_reply = (redisReply*)redisCommand(conn.get(), 
        "ZADD messages_by_time %ld %s", timestamp, message_id.c_str());
    
    if (!zadd_reply || zadd_reply->type == REDIS_REPLY_ERROR) {
        freeReplyObject(zadd_reply);
        LOG_ERROR("Failed to add message to sorted set\n");
        // 返回失败响应
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setContentType("application/json");
        resp->setBody(json{
            {"status", "failed"}
        }.dump());
        return;
    }
    freeReplyObject(zadd_reply);

    // 释放连接回池中
    redis_pool.releaseConnection(conn);

    // 返回成功响应
    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setContentType("application/json");
    resp->setBody(json{
        {"status", "success"},
        {"message_id", message_id}
    }.dump());
    return;
}

// 处理根路径GET请求
void handleRootRequest(const HttpRequest &req, HttpResponse *resp) {
    // 获取Redis连接
    RedisConnectionPool& redis_pool = RedisConnectionPool::getInstance();
    auto conn = redis_pool.getConnection();
    if (!conn) {
        LOG_ERROR("Failed to get Redis connection\n");
        // 返回失败响应
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setContentType("application/json");
        resp->setBody(json{
            {"status", "failed"}
        }.dump());
    }

    // 获取按时间排序的留言ID列表
    redisReply* zrange_reply = (redisReply*)redisCommand(conn.get(), 
        "ZREVRANGE messages_by_time 0 -1 WITHSCORES");
    
    if (!zrange_reply || zrange_reply->type == REDIS_REPLY_ERROR) {
        freeReplyObject(zrange_reply);

        LOG_ERROR("Failed to retrieve message list\n");
        // 返回失败响应
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setContentType("application/json");
        resp->setBody(json{
            {"status", "failed"}
        }.dump());
    }

    json messages = json::array();
    
    // 遍历所有留言ID，获取详情
    for (size_t i = 0; i < zrange_reply->elements; i += 2) {
        std::string message_id = zrange_reply->element[i]->str;
        time_t timestamp = std::stol(zrange_reply->element[i+1]->str);

        // 获取留言详情
        redisReply* hgetall_reply = (redisReply*)redisCommand(conn.get(), 
            "HGETALL %s", message_id.c_str());
        
        if (hgetall_reply && hgetall_reply->type == REDIS_REPLY_ARRAY) {
            json message;
            // message["id"] = message_id;
            // message["timestamp"] = timestamp;
            
            for (size_t j = 0; j < hgetall_reply->elements; j += 2) {
                std::string field = hgetall_reply->element[j]->str;
                std::string value = hgetall_reply->element[j+1]->str;
                message[field] = value;
            }
            
            messages.push_back(message);
        }
        freeReplyObject(hgetall_reply);
    }
    freeReplyObject(zrange_reply);

    // 释放连接回池中
    redis_pool.releaseConnection(conn);

#ifdef DEBUG
    std::cout << messages.dump() << std::endl;
#endif

    // 渲染HTML页面并嵌入留言数据
    renderHomeHtml("/home/cf/test_Projects/muduo/http/resource/home.html", 
                  resp, 
                  messages.dump());
}

void onRequest(const HttpRequest &req, HttpResponse *resp) {
    std::cout << "Headers " << req.methodString() << " " << req.path() << std::endl;
    
    if (!benchmark) {
        const std::map<string, string> &headers = req.headers();
        for (const auto &header : headers) {
            std::cout << header.first << ": " << header.second << std::endl;
        }
    }

    try {
        // 处理POST请求 - 提交留言
        if (req.method() == HttpRequest::kPost) {
            handlePostRequest(req, resp);
        }
        // 处理根路径请求 - 显示留言板
        else if (req.path() == "/") {
            handleRootRequest(req, resp);
        }
        // 示例hello端点
        else if (req.path() == "/hello") {
            resp->setStatusCode(HttpResponse::k200Ok);
            resp->setStatusMessage("OK");
            resp->setContentType("text/plain");
            resp->addHeader("Server", "Muduo");
            resp->setBody("hello, world!\n");
        }
        // 404处理
        else {
            resp->setStatusCode(HttpResponse::k404NotFound);
            resp->setStatusMessage("Not Found");
            resp->setCloseConnection(true);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Request handling error: %s", e.what());
        resp->setStatusCode(HttpResponse::k500InternalServerError);
        resp->setStatusMessage("Internal Server Error");
        resp->setContentType("application/json");
        resp->setBody(json{
            {"status", "error"},
            {"message", e.what()}
        }.dump());
    }
}

RedisConnectionPool& createRedisPool()
{
    try {
        auto& pool = RedisConnectionPool::getInstance();
        pool.init("127.0.0.1", 6379);
        return pool;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to initialize Redis pool\n");
        LOG_FATAL("%s Failed to initialize Redis pool.\n", __FUNCTION__);
    }
}


int main(int argc, char *argv[])
{
    int numThreads = 0;
    if (argc > 1)
    {
      benchmark = true;
      numThreads = atoi(argv[1]);
    }

    RedisConnectionPool& redis_pool = createRedisPool();

    EventLoop loop;
    HttpServer server(&loop, InetAddress(8080), "dummy", TcpServer::kReusePort);
    server.setHttpCallback(onRequest);
    server.setThreadNum(numThreads);
    server.start();
    loop.loop();
}